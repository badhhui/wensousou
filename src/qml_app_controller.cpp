#include "qml_app_controller.h"

#include "app_paths.h"
#include "index_worker.h"
#include "root_policy.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QMessageBox>
#include <QMetaObject>
#include <QSet>
#include <QUrl>

namespace wensousou {
namespace {

QString htmlPreview(QString text) {
  text = text.toHtmlEscaped();
  text.replace(QStringLiteral("[[["),
               QStringLiteral("<span style=\"background-color:#fff19a;"
                              "color:#172033;border-radius:3px;\">"));
  text.replace(QStringLiteral("]]]"), QStringLiteral("</span>"));
  text.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
  return text;
}

}  // namespace

QmlAppController::QmlAppController(QObject* parent) : QObject(parent) {
  QString error;
  if (!database_.open(AppPaths::databasePath(), AppPaths::simpleLibraryPath(), &error)) {
    setStatusText(error);
  }
  refreshRoots();

  indexWorker_ = new IndexWorker;
  indexWorker_->moveToThread(&indexThread_);
  connect(&indexThread_, &QThread::finished, indexWorker_, &QObject::deleteLater);
  connect(indexWorker_, &IndexWorker::runningChanged,
          this, &QmlAppController::handleIndexRunning);
  connect(indexWorker_, &IndexWorker::finished,
          this, &QmlAppController::handleIndexFinished);
  connect(indexWorker_, &IndexWorker::progress,
          this, &QmlAppController::handleProgress);
  indexThread_.start();
}

QmlAppController::~QmlAppController() {
  if (indexWorker_) indexWorker_->cancel();
  indexThread_.quit();
  indexThread_.wait();
}

SearchResultsModel* QmlAppController::resultsModel() { return &resultsModel_; }
RootsModel* QmlAppController::rootsModel() { return &rootsModel_; }
QString QmlAppController::statusText() const { return statusText_; }
QString QmlAppController::progressText() const { return progressText_; }
QString QmlAppController::resultSummary() const { return resultSummary_; }
bool QmlAppController::indexRunning() const { return indexRunning_; }
QStringList QmlAppController::enabledExtensions() const {
  QStringList values;
  for (const QString& extension : RootPolicy::enabledExtensions()) {
    values.append(extension.toUpper());
  }
  return values;
}

QStringList QmlAppController::rootNames() const {
  QStringList names;
  for (int row = 0; row < rootsModel_.rowCount(); ++row) {
    names.append(rootsModel_.data(rootsModel_.index(row, 0), RootsModel::NameRole).toString());
  }
  return names;
}

void QmlAppController::refreshRoots() {
  QString error;
  const QList<RootRecord> roots = database_.roots(&error);
  if (!error.isEmpty()) {
    setStatusText(error);
    return;
  }
  rootsModel_.setRoots(roots);
  emit rootNamesChanged();
}

void QmlAppController::search(const QString& query, qint64 rootId,
                              const QStringList& extensions, int modifiedDays) {
  const QString trimmed = query.trimmed();
  if (trimmed.isEmpty()) {
    resultsModel_.setResults({});
    setResultSummary(QStringLiteral("输入关键词后开始搜索"));
    setStatusText(QStringLiteral("准备就绪"));
    return;
  }
  const qint64 modifiedAfterMs =
      modifiedDays > 0
          ? QDateTime::currentDateTime().addDays(-modifiedDays).toMSecsSinceEpoch()
          : 0;
  QString error;
  int total = 0;
  QList<SearchResult> results = database_.search(
      trimmed, rootId, QString(), modifiedAfterMs, SearchSort::Relevance,
      2000, 0, &error, &total);
  if (!error.isEmpty()) {
    setStatusText(QStringLiteral("搜索失败：%1").arg(error));
    return;
  }
  QList<SearchResult> filtered;
  QSet<QString> selected;
  for (const QString& extension : extensions) selected.insert(extension);
  for (const SearchResult& result : results) {
    if (selected.isEmpty() || selected.contains(result.extension.toUpper())) {
      filtered.append(result);
    }
  }
  resultsModel_.setResults(filtered);
  setResultSummary(QStringLiteral("找到 %1 个匹配文件").arg(filtered.size()));
  setStatusText(QStringLiteral("搜索完成，显示前 %1 个结果。").arg(filtered.size()));
}

void QmlAppController::addRoot() {
  const QString path = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择索引目录"));
  if (path.isEmpty()) return;
  QString error;
  if (!RootPolicy::canAdd(path, rootsModel_.paths(), &error)) {
    QMessageBox::warning(nullptr, QStringLiteral("无法添加目录"), error);
    return;
  }
  if (QMessageBox::question(nullptr, QStringLiteral("确认建立索引"),
                            QStringLiteral("是否确认对以下目录建立索引？\n\n%1").arg(path)) !=
      QMessageBox::Yes) {
    return;
  }
  if (!database_.addRoot(path, &error)) {
    QMessageBox::warning(nullptr, QStringLiteral("无法添加目录"), error);
    return;
  }
  refreshRoots();
  updateAllRoots();
}

void QmlAppController::updateAllRoots() {
  if (!indexWorker_) return;
  setStatusText(QStringLiteral("正在准备扫描索引目录..."));
  QMetaObject::invokeMethod(indexWorker_, "updateAll", Qt::QueuedConnection);
}

void QmlAppController::updateRoot(qint64 rootId) {
  if (!indexWorker_ || rootId <= 0) return;
  setStatusText(QStringLiteral("正在准备扫描所选目录..."));
  QMetaObject::invokeMethod(indexWorker_, "updateRoot", Qt::QueuedConnection,
                            Q_ARG(qint64, rootId));
}

void QmlAppController::openDocument(const QString& path) {
  if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void QmlAppController::openFolder(const QString& path) {
  if (!path.isEmpty()) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
  }
}

QString QmlAppController::previewHtml(qint64 documentId, const QString& query) {
  QString error;
  const QString text = database_.previewDocument(documentId, query, &error);
  return error.isEmpty() ? htmlPreview(text) : error.toHtmlEscaped();
}

void QmlAppController::handleIndexRunning(bool running) {
  if (indexRunning_ == running) return;
  indexRunning_ = running;
  emit indexRunningChanged();
}

void QmlAppController::handleIndexFinished(bool, const QString& message) {
  setStatusText(message);
  setProgressText(QString());
  refreshRoots();
}

void QmlAppController::handleProgress(const QString& currentFile, int processed,
                                      int failed, int total) {
  setStatusText(currentFile.isEmpty()
                    ? QStringLiteral("正在统计待处理文件...")
                    : QStringLiteral("正在处理：%1").arg(currentFile));
  setProgressText(QStringLiteral("已处理 %1   失败 %2   总数 %3")
                      .arg(processed)
                      .arg(failed)
                      .arg(total));
}

void QmlAppController::setStatusText(const QString& value) {
  if (statusText_ == value) return;
  statusText_ = value;
  emit statusTextChanged();
}

void QmlAppController::setProgressText(const QString& value) {
  if (progressText_ == value) return;
  progressText_ = value;
  emit progressTextChanged();
}

void QmlAppController::setResultSummary(const QString& value) {
  if (resultSummary_ == value) return;
  resultSummary_ = value;
  emit resultSummaryChanged();
}

}  // namespace wensousou
