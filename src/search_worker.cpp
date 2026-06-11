#include "search_worker.h"

#include "app_paths.h"

#include <QDebug>
#include <QElapsedTimer>

namespace wensousou {

void registerSearchMetaTypes() {
  // MOC records this signal parameter as QList<SearchResult> because the
  // signal is declared inside the wensousou namespace. Register that exact
  // spelling so Qt can marshal results back to the UI thread.
  qRegisterMetaType<QList<SearchResult>>("QList<SearchResult>");
  qRegisterMetaType<SearchSort>("SearchSort");
}

SearchWorker::SearchWorker(QObject* parent) : QObject(parent) {}

void SearchWorker::initialize() {
  QElapsedTimer timer;
  timer.start();
  QString error;
  const bool ready = ensureDatabase(&error);
  qInfo() << "Search worker initialized"
          << "ready=" << ready
          << "elapsedMs=" << timer.elapsed()
          << "error=" << error;
}

void SearchWorker::cancel() {
  if (databaseReady_.load()) database_.interrupt();
}

void SearchWorker::cancelBefore(qint64 requestId) {
  latestRequestId_.store(requestId);
  if (databaseReady_.load()) database_.interrupt();
}

void SearchWorker::search(qint64 requestId, const QString& query, qint64 rootId,
                          const QStringList& extensions, qint64 modifiedAfterMs,
                          SearchSort sort, int limit, int offset, bool countTotal) {
  QElapsedTimer timer;
  timer.start();
  if (requestId != latestRequestId_.load()) {
    emit finished(requestId, {}, 0, QStringLiteral("interrupted"), timer.elapsed());
    return;
  }
  QString error;
  if (!ensureDatabase(&error)) {
    emit finished(requestId, {}, 0, error, timer.elapsed());
    return;
  }
  int totalCount = 0;
  const QList<SearchResult> results = database_.search(
      query, rootId, extensions, modifiedAfterMs, sort, limit, offset, &error,
      &totalCount, countTotal);
  qInfo() << "Search finished"
          << "requestId=" << requestId
          << "query=" << query
          << "rootId=" << rootId
          << "extensions=" << extensions
          << "modifiedAfterMs=" << modifiedAfterMs
          << "sort=" << static_cast<int>(sort)
          << "limit=" << limit
          << "offset=" << offset
          << "countTotal=" << countTotal
          << "results=" << results.size()
          << "totalCount=" << totalCount
          << "elapsedMs=" << timer.elapsed()
          << "error=" << error;
  emit finished(requestId, results, totalCount, error, timer.elapsed());
}

bool SearchWorker::ensureDatabase(QString* error) {
  if (database_.isOpen()) return true;
  if (!database_.openReadOnly(AppPaths::databasePath(), AppPaths::simpleLibraryPath(), error)) {
    return false;
  }
  databaseReady_.store(true);
  if (database_.warmUpSearch(error)) return true;
  databaseReady_.store(false);
  database_.close();
  return false;
}

}  // namespace wensousou
