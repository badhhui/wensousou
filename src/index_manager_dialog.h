#pragma once

#include "database.h"

#include <QDialog>
#include <QThread>

class QLabel;
class QTableWidget;

namespace wensousou {

class Database;
class IndexManagerWorker;
class RootRemovalWorker;

class IndexManagerDialog : public QDialog {
  Q_OBJECT

 public:
  explicit IndexManagerDialog(Database* database, QWidget* parent = nullptr);
  ~IndexManagerDialog() override;

 signals:
  void rootsChanged();
  void updateRequested(qint64 rootId);
  void updateAllRequested();
  void retryFailuresRequested();
  void rootRemovalRequested(qint64 rootId, const QString& path);

 public slots:
  void refresh();
#ifdef WENSOUSOU_ENABLE_TEST_HOOKS
  void startRootRemovalForTest(qint64 rootId, const QString& path);
#endif

 private slots:
  void addRoot();
  void removeSelectedRoot();
  void updateSelectedRoot();
  void showSelectedFailures();
  void applySummaries(const QList<RootIndexSummary>& roots, const QString& error);
  void handleRemovalFinished(qint64 rootId, const QString& path,
                             const QString& error);

 private:
  qint64 selectedRootId() const;
  void startRootRemoval(qint64 rootId, const QString& path);
  void showFailures(qint64 rootId, const QString& rootPath);
  void setBusy(bool busy, const QString& message = QString());

  Database* database_ = nullptr;
  QThread summaryThread_;
  IndexManagerWorker* summaryWorker_ = nullptr;
  QThread removalThread_;
  RootRemovalWorker* removalWorker_ = nullptr;
  bool summaryLoading_ = false;
  bool summaryReloadPending_ = false;
  bool removalRunning_ = false;
  qint64 removingRootId_ = 0;
  QString removingPath_;
  QTableWidget* rootsTable_ = nullptr;
  QLabel* summaryLabel_ = nullptr;
  QPushButton* addButton_ = nullptr;
  QPushButton* refreshButton_ = nullptr;
  QPushButton* refreshAllButton_ = nullptr;
  QPushButton* retryButton_ = nullptr;
  QPushButton* removeButton_ = nullptr;
};

}  // namespace wensousou
