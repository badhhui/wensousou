#pragma once

#include "database.h"

#include <QObject>

#include <atomic>

namespace wensousou {

struct IndexSettings {
  qint64 maxFileBytes = 10LL * 1024 * 1024;
  int maxCharacters = 2'000'000;
  int timeoutSeconds = 30;

  static IndexSettings load();
};

class IndexWorker : public QObject {
  Q_OBJECT

 public:
  explicit IndexWorker(QObject* parent = nullptr);

 public slots:
  void updateAll();
  void updateRoot(qint64 rootId);
  void retryFailures();
  void cancel();

 signals:
  void runningChanged(bool running);
  void progress(const QString& currentFile, int processed, int failed, int total);
  void finished(bool success, const QString& message);

 private:
  void run(qint64 selectedRootId, bool retryFailed);
  bool scanRoot(Database* database, const RootRecord& root,
                const IndexSettings& settings, bool retryFailed,
                int total, int* processed, int* indexed, int* failed, QString* error);
  int countDocuments(const QList<RootRecord>& roots, qint64 selectedRootId) const;

  std::atomic_bool cancelled_{false};
  bool running_ = false;
};

}  // namespace wensousou
