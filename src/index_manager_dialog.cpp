#include "index_manager_dialog.h"

#include "app_paths.h"
#include "database.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QMetaType>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace wensousou {

class IndexManagerWorker : public QObject {
  Q_OBJECT

 public slots:
  void load() {
    Database database;
    QString error;
    QList<RootIndexSummary> roots;
    if (database.openReadOnly(AppPaths::databasePath(), AppPaths::simpleLibraryPath(),
                              &error)) {
      roots = database.rootIndexSummaries(&error);
    }
    emit loaded(roots, error);
  }

 signals:
  void loaded(const QList<RootIndexSummary>& roots, const QString& error);
};

class RootRemovalWorker : public QObject {
  Q_OBJECT

 public slots:
  void removeRoot(qint64 rootId, const QString& path) {
    Database database;
    QString error;
    if (database.open(AppPaths::databasePath(), AppPaths::simpleLibraryPath(),
                      &error)) {
      database.removeRoot(rootId, &error);
    }
    emit removed(rootId, path, error);
  }

 signals:
  void removed(qint64 rootId, const QString& path, const QString& error);
};

namespace {

QString formatTime(qint64 timestampMs) {
  return timestampMs > 0
             ? QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
             : QStringLiteral("尚未更新");
}

QString formatFileSize(qint64 bytes) {
  if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
  if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  if (bytes < 1024LL * 1024 * 1024) {
    return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
  }
  return QStringLiteral("%1 GB").arg(bytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 1);
}

}  // namespace

IndexManagerDialog::IndexManagerDialog(Database* database, QWidget* parent)
    : QDialog(parent), database_(database) {
  setWindowTitle(QStringLiteral("索引管理"));
  resize(1200, 720);

  summaryLabel_ = new QLabel(this);
  summaryLabel_->setObjectName(QStringLiteral("resultHint"));
  rootsTable_ = new QTableWidget(0, 7, this);
  rootsTable_->setHorizontalHeaderLabels(
      {QStringLiteral("目录"), QStringLiteral("文件数"), QStringLiteral("失败"),
       QStringLiteral("索引大小"), QStringLiteral("更新时间"), QStringLiteral("状态"),
       QStringLiteral("操作")});
  rootsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column < 7; ++column) {
    rootsTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
  }
  rootsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  rootsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  rootsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  rootsTable_->setAlternatingRowColors(true);
  rootsTable_->verticalHeader()->setVisible(false);

  addButton_ = new QPushButton(QStringLiteral("添加目录"), this);
  refreshButton_ = new QPushButton(QStringLiteral("刷新所选"), this);
  refreshAllButton_ = new QPushButton(QStringLiteral("全部更新"), this);
  refreshAllButton_->setObjectName(QStringLiteral("primaryButton"));
  retryButton_ = new QPushButton(QStringLiteral("重试失败文件"), this);
  removeButton_ = new QPushButton(QStringLiteral("移除所选"), this);
  removeButton_->setObjectName(QStringLiteral("dangerButton"));
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(addButton_, &QPushButton::clicked, this, &IndexManagerDialog::addRoot);
  connect(removeButton_, &QPushButton::clicked, this, &IndexManagerDialog::removeSelectedRoot);
  connect(refreshButton_, &QPushButton::clicked, this, &IndexManagerDialog::updateSelectedRoot);
  connect(refreshAllButton_, &QPushButton::clicked, this, &IndexManagerDialog::updateAllRequested);
  connect(retryButton_, &QPushButton::clicked, this, &IndexManagerDialog::retryFailuresRequested);

  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(addButton_);
  toolbar->addWidget(refreshButton_);
  toolbar->addWidget(refreshAllButton_);
  toolbar->addWidget(retryButton_);
  toolbar->addWidget(removeButton_);
  toolbar->addStretch();
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(summaryLabel_);
  layout->addLayout(toolbar);
  layout->addWidget(rootsTable_);
  layout->addWidget(buttons);

  qRegisterMetaType<QList<RootIndexSummary>>("QList<RootIndexSummary>");
  qRegisterMetaType<qint64>("qint64");
  summaryWorker_ = new IndexManagerWorker;
  summaryWorker_->moveToThread(&summaryThread_);
  connect(&summaryThread_, &QThread::finished, summaryWorker_, &QObject::deleteLater);
  connect(summaryWorker_, &IndexManagerWorker::loaded,
          this, &IndexManagerDialog::applySummaries);
  summaryThread_.start();

  removalWorker_ = new RootRemovalWorker;
  removalWorker_->moveToThread(&removalThread_);
  connect(&removalThread_, &QThread::finished, removalWorker_, &QObject::deleteLater);
  connect(this, &IndexManagerDialog::rootRemovalRequested,
          removalWorker_, &RootRemovalWorker::removeRoot);
  connect(removalWorker_, &RootRemovalWorker::removed,
          this, &IndexManagerDialog::handleRemovalFinished);
  removalThread_.start();
  refresh();
}

IndexManagerDialog::~IndexManagerDialog() {
  summaryThread_.quit();
  removalThread_.quit();
  summaryThread_.wait();
  removalThread_.wait();
}

void IndexManagerDialog::refresh() {
  if (removalRunning_) {
    summaryReloadPending_ = true;
    return;
  }
  if (summaryLoading_) {
    summaryReloadPending_ = true;
    return;
  }
  summaryLoading_ = true;
  summaryLabel_->setText(QStringLiteral("正在后台读取索引信息..."));
  QMetaObject::invokeMethod(summaryWorker_, "load", Qt::QueuedConnection);
}

void IndexManagerDialog::applySummaries(const QList<RootIndexSummary>& roots,
                                        const QString& error) {
  summaryLoading_ = false;
  if (removalRunning_) return;
  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("读取失败"), error);
  } else {
    rootsTable_->setRowCount(roots.size());
    int totalDocuments = 0;
    int totalFailures = 0;
    qint64 totalSize = 0;
    for (int row = 0; row < roots.size(); ++row) {
      const RootIndexSummary& root = roots.at(row);
      totalDocuments += root.documentCount;
      totalFailures += root.failedCount;
      totalSize += root.totalSize;
      auto* path = new QTableWidgetItem(root.path);
      path->setData(Qt::UserRole, root.id);
      path->setToolTip(root.path);
      rootsTable_->setItem(row, 0, path);
      rootsTable_->setItem(row, 1, new QTableWidgetItem(QString::number(root.documentCount)));
      rootsTable_->setItem(row, 2, new QTableWidgetItem(QString::number(root.failedCount)));
      rootsTable_->setItem(row, 3, new QTableWidgetItem(formatFileSize(root.totalSize)));
      rootsTable_->setItem(row, 4, new QTableWidgetItem(formatTime(root.lastScanAtMs)));
      rootsTable_->setItem(
          row, 5,
          new QTableWidgetItem(root.lastScanStatus == QStringLiteral("ok")
                                   ? QStringLiteral("正常")
                                   : root.lastScanStatus.isEmpty() ? QStringLiteral("待更新")
                                                                   : root.lastScanStatus));
      auto* actions = new QWidget(this);
      auto* actionLayout = new QHBoxLayout(actions);
      actionLayout->setContentsMargins(3, 0, 3, 0);
      auto* failures = new QToolButton(actions);
      failures->setText(QStringLiteral("失败 %1").arg(root.failedCount));
      failures->setToolTip(QStringLiteral("查看失败文件和原因"));
      failures->setEnabled(root.failedCount > 0);
      actionLayout->addWidget(failures);
      connect(failures, &QToolButton::clicked, this,
              [this, root]() { showFailures(root.id, root.path); });
      rootsTable_->setCellWidget(row, 6, actions);
    }
    summaryLabel_->setText(
        QStringLiteral("共 %1 个索引目录，%2 个文件，失败 %3 个，索引文件总大小 %4")
            .arg(roots.size())
            .arg(totalDocuments)
            .arg(totalFailures)
            .arg(formatFileSize(totalSize)));
  }
  if (summaryReloadPending_) {
    summaryReloadPending_ = false;
    refresh();
  }
}

void IndexManagerDialog::addRoot() {
  if (removalRunning_) return;
  const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择索引目录"));
  if (path.isEmpty()) return;
  if (QMessageBox::question(this, QStringLiteral("确认建立索引"),
                            QStringLiteral("是否确认对以下目录建立索引？\n\n%1").arg(path)) !=
      QMessageBox::Yes) {
    return;
  }
  QString error;
  if (!database_->addRoot(path, &error)) {
    QMessageBox::warning(this, QStringLiteral("无法添加目录"), error);
    return;
  }
  refresh();
  emit rootsChanged();
  emit updateAllRequested();
}

void IndexManagerDialog::removeSelectedRoot() {
  if (removalRunning_) return;
  const qint64 rootId = selectedRootId();
  if (rootId <= 0) return;
  const int row = rootsTable_->currentRow();
  const QString path = rootsTable_->item(row, 0)->text();
  if (QMessageBox::question(this, QStringLiteral("移除目录"),
                            QStringLiteral("确定移除以下目录及其本地索引吗？\n\n%1")
                                .arg(path)) !=
      QMessageBox::Yes) {
    return;
  }
  startRootRemoval(rootId, path);
}

#ifdef WENSOUSOU_ENABLE_TEST_HOOKS
void IndexManagerDialog::startRootRemovalForTest(qint64 rootId, const QString& path) {
  startRootRemoval(rootId, path);
}
#endif

void IndexManagerDialog::startRootRemoval(qint64 rootId, const QString& path) {
  if (rootId <= 0 || removalRunning_) return;
  removingRootId_ = rootId;
  removingPath_ = path;
  removalRunning_ = true;
  setBusy(true, QStringLiteral("正在删除索引：%1。索引较大时可能需要一些时间...")
                    .arg(path));
  emit rootRemovalRequested(rootId, path);
}

void IndexManagerDialog::updateSelectedRoot() {
  if (removalRunning_) return;
  const qint64 rootId = selectedRootId();
  if (rootId > 0) emit updateRequested(rootId);
}

void IndexManagerDialog::showSelectedFailures() {
  if (removalRunning_) return;
  const qint64 rootId = selectedRootId();
  if (rootId <= 0) return;
  const int row = rootsTable_->currentRow();
  showFailures(rootId, rootsTable_->item(row, 0)->text());
}

qint64 IndexManagerDialog::selectedRootId() const {
  const int row = rootsTable_->currentRow();
  return row >= 0 && rootsTable_->item(row, 0)
             ? rootsTable_->item(row, 0)->data(Qt::UserRole).toLongLong()
             : 0;
}

void IndexManagerDialog::showFailures(qint64 rootId, const QString& rootPath) {
  if (removalRunning_) return;
  QString error;
  const QList<FailureRecord> records = database_->failures(rootId, &error);
  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("读取失败"), error);
    return;
  }
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("索引失败文件：%1").arg(rootPath));
  dialog.resize(1120, 680);
  auto* table = new QTableWidget(records.size(), 4, &dialog);
  table->setHorizontalHeaderLabels(
      {QStringLiteral("文件"), QStringLiteral("大小"), QStringLiteral("记录时间"),
       QStringLiteral("失败原因")});
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setAlternatingRowColors(true);
  table->verticalHeader()->setVisible(false);
  for (int row = 0; row < records.size(); ++row) {
    const FailureRecord& record = records.at(row);
    auto* path = new QTableWidgetItem(record.path);
    path->setToolTip(record.path);
    table->setItem(row, 0, path);
    table->setItem(row, 1, new QTableWidgetItem(formatFileSize(record.size)));
    table->setItem(row, 2, new QTableWidgetItem(formatTime(record.indexedAtMs)));
    auto* reason = new QTableWidgetItem(record.error);
    reason->setToolTip(record.error);
    table->setItem(row, 3, reason);
  }
  auto* retry = new QPushButton(QStringLiteral("重试全部失败文件"), &dialog);
  retry->setObjectName(QStringLiteral("primaryButton"));
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(retry, &QPushButton::clicked, &dialog, [this, &dialog]() {
    emit retryFailuresRequested();
    dialog.accept();
  });
  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(new QLabel(QStringLiteral("共 %1 个失败文件").arg(records.size()), &dialog));
  toolbar->addStretch();
  toolbar->addWidget(retry);
  auto* layout = new QVBoxLayout(&dialog);
  layout->addLayout(toolbar);
  layout->addWidget(table);
  layout->addWidget(buttons);
  dialog.exec();
}

void IndexManagerDialog::handleRemovalFinished(qint64 rootId, const QString& path,
                                               const QString& error) {
  Q_UNUSED(rootId)
  removalRunning_ = false;
  removingRootId_ = 0;
  removingPath_.clear();
  setBusy(false);
  if (!error.isEmpty()) {
    summaryLabel_->setText(QStringLiteral("删除失败：%1").arg(error));
    QMessageBox::warning(this, QStringLiteral("移除失败"), error);
    refresh();
    return;
  }
  summaryLabel_->setText(QStringLiteral("已删除索引：%1").arg(path));
  rootsTable_->setRowCount(0);
  refresh();
  emit rootsChanged();
}

void IndexManagerDialog::setBusy(bool busy, const QString& message) {
  rootsTable_->setEnabled(!busy);
  for (QPushButton* button :
       {addButton_, refreshButton_, refreshAllButton_, retryButton_, removeButton_}) {
    if (button) button->setEnabled(!busy);
  }
  if (!message.isEmpty()) summaryLabel_->setText(message);
}

}  // namespace wensousou

#include "index_manager_dialog.moc"
