#include "index_worker.h"

#include "app_paths.h"
#include "root_policy.h"
#include "tika_worker.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <QtGlobal>

namespace wensousou {

IndexSettings IndexSettings::load() {
  QSettings settings;
  IndexSettings value;
  value.maxFileBytes = qBound(
      1LL * 1024 * 1024,
      settings.value(QStringLiteral("index/maxFileBytes"), value.maxFileBytes).toLongLong(),
      500LL * 1024 * 1024);
  value.maxCharacters = qBound(
      10'000,
      settings.value(QStringLiteral("index/maxCharacters"), value.maxCharacters).toInt(),
      2'000'000);
  value.timeoutSeconds = qBound(
      5,
      settings.value(QStringLiteral("index/timeoutSeconds"), value.timeoutSeconds).toInt(),
      120);
  return value;
}

IndexWorker::IndexWorker(QObject* parent) : QObject(parent) {}

void IndexWorker::updateAll() { run(0, false); }
void IndexWorker::updateRoot(qint64 rootId) { run(rootId, false); }
void IndexWorker::retryFailures() { run(0, true); }
void IndexWorker::cancel() { cancelled_.store(true); }

void IndexWorker::run(qint64 selectedRootId, bool retryFailed) {
  if (running_) return;
  running_ = true;
  cancelled_.store(false);
  emit runningChanged(true);
  qInfo() << "Starting index update" << selectedRootId << "retryFailed=" << retryFailed;

  Database database;
  QString error;
  if (!database.open(AppPaths::databasePath(), AppPaths::simpleLibraryPath(), &error)) {
    running_ = false;
    emit runningChanged(false);
    emit finished(false, error);
    return;
  }
  const QList<RootRecord> roots = database.roots(&error);
  const IndexSettings settings = IndexSettings::load();
  int processed = 0;
  int indexed = 0;
  int failed = 0;
  emit progress(QString(), processed, failed, 0);
  const int total = countDocuments(roots, selectedRootId);
  emit progress(QString(), processed, failed, total);
  bool success = error.isEmpty();
  for (const RootRecord& root : roots) {
    if (cancelled_.load()) break;
    if (selectedRootId > 0 && root.id != selectedRootId) continue;
    QString scanError;
    if (!scanRoot(&database, root, settings, retryFailed, total,
                  &processed, &indexed, &failed, &scanError)) {
      success = false;
      if (!scanError.isEmpty()) error = scanError;
    }
  }
  const bool cancelled = cancelled_.load();
  const QString message =
      cancelled
          ? QStringLiteral("索引任务已取消。")
          : QStringLiteral("索引更新完成：扫描 %1 个文件，更新 %2 个，失败 %3 个。")
                .arg(processed)
                .arg(indexed)
                .arg(failed);
  running_ = false;
  qInfo() << message;
  emit runningChanged(false);
  emit finished(success && !cancelled, error.isEmpty() ? message : message + QStringLiteral(" ") + error);
}

int IndexWorker::countDocuments(const QList<RootRecord>& roots,
                                qint64 selectedRootId) const {
  int total = 0;
  for (const RootRecord& root : roots) {
    if (cancelled_.load()) break;
    if (selectedRootId > 0 && root.id != selectedRootId) continue;
    if (!QFileInfo(root.path).isDir()) continue;
    QDirIterator iterator(root.path, QDir::Files | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
      const QString path = RootPolicy::normalize(iterator.next());
      const QFileInfo file(path);
      if (!file.isSymLink() && !RootPolicy::shouldSkipPath(path, root.path) &&
          RootPolicy::isSupportedDocument(path)) {
        ++total;
      }
    }
  }
  return total;
}

bool IndexWorker::scanRoot(Database* database, const RootRecord& root,
                           const IndexSettings& settings, bool retryFailed,
                           int total, int* processed, int* indexed, int* failed,
                           QString* error) {
  const qint64 scanId = database->beginScan(root.id, error);
  if (scanId == 0) return false;
  QFileInfo rootInfo(root.path);
  if (!rootInfo.exists() || !rootInfo.isDir()) {
    const QString message = QStringLiteral("目录不可访问，已保留原索引：%1").arg(root.path);
    database->finishScan(scanId, root.id, false, message, nullptr);
    emit progress(message, *processed, *failed, total);
    qWarning() << message;
    return true;
  }

  TikaWorker parser;
  QDirIterator iterator(root.path, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    if (cancelled_.load()) {
      database->finishScan(scanId, root.id, false,
                           QStringLiteral("用户取消索引任务。"), nullptr);
      return false;
    }
    const QString path = RootPolicy::normalize(iterator.next());
    const QFileInfo file(path);
    if (file.isSymLink() || RootPolicy::shouldSkipPath(path, root.path) ||
        !RootPolicy::isSupportedDocument(path)) {
      continue;
    }
    ++(*processed);
    emit progress(file.fileName(), *processed, *failed, total);

    const qint64 size = file.size();
    const qint64 modified = file.lastModified().toMSecsSinceEpoch();
    const DocumentFingerprint current = database->fingerprint(path, error);
    if (!error->isEmpty()) {
      database->finishScan(scanId, root.id, false, *error, nullptr);
      return false;
    }
    const bool changed = !current.found || current.size != size ||
                         current.mtimeMs != modified ||
                         (retryFailed && current.status == QStringLiteral("failed"));
    if (!changed) {
      if (!database->markSeen(path, scanId, error)) {
        database->finishScan(scanId, root.id, false, *error, nullptr);
        return false;
      }
      continue;
    }
    if (size > settings.maxFileBytes) {
      ++(*failed);
      if (!database->markFailure(root.id, path, size, modified,
                                 QStringLiteral("文件超过大小限制。"), scanId, error)) {
        database->finishScan(scanId, root.id, false, *error, nullptr);
        return false;
      }
      continue;
    }
    const ParseResult result =
        parser.parse(path, settings.maxCharacters, settings.timeoutSeconds);
    if (!result.ok) {
      ++(*failed);
      if (!database->markFailure(root.id, path, size, modified,
                                 result.error, scanId, error)) {
        database->finishScan(scanId, root.id, false, *error, nullptr);
        return false;
      }
      continue;
    }
    if (!database->upsertDocument(root.id, path, size, modified,
                                  result.content, scanId, error)) {
      database->finishScan(scanId, root.id, false, *error, nullptr);
      return false;
    }
    ++(*indexed);
  }

  const QString summary = QStringLiteral("目录更新完成：%1").arg(root.path);
  return database->finishScan(scanId, root.id, true, summary, error);
}

}  // namespace wensousou
