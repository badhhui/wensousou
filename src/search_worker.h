#pragma once

#include "database.h"

#include <QObject>

#include <atomic>

namespace wensousou {

void registerSearchMetaTypes();

class SearchWorker : public QObject {
  Q_OBJECT

 public:
  explicit SearchWorker(QObject* parent = nullptr);
  void cancel();
  void cancelBefore(qint64 requestId);

 public slots:
  void initialize();
  void search(qint64 requestId, const QString& query, qint64 rootId,
              const QStringList& extensions, qint64 modifiedAfterMs,
              SearchSort sort, int limit, int offset, bool countTotal,
              int searchScope);

 signals:
  void finished(qint64 requestId, const QList<SearchResult>& results,
                int totalCount, const QString& error, qint64 elapsedMs);

 private:
  bool ensureDatabase(QString* error);

  Database database_;
  std::atomic_bool databaseReady_{false};
  std::atomic<qint64> latestRequestId_{0};
};

}  // namespace wensousou
