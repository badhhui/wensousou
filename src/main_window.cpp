#include "main_window.h"

#include "action_icons.h"
#include "app_paths.h"
#include "index_manager_dialog.h"
#include "index_worker.h"
#include "root_policy.h"
#include "search_worker.h"
#include "settings_dialog.h"

#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMetaObject>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QSize>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace wensousou {
namespace {

constexpr int kPreviewCharacterLimit = 1'000'000;
constexpr int kMaxCachedSearchResults = 20'000;

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

QString highlightedHtml(QString text, const QString& startMarker,
                        const QString& endMarker) {
  text = text.toHtmlEscaped();
  text.replace(startMarker,
               QStringLiteral("<span style=\"background-color:#fff19a;"
                              "color:#1f2328;\">"));
  text.replace(endMarker, QStringLiteral("</span>"));
  return text;
}

QString anchoredHighlightedHtml(QString text, const QString& startMarker,
                                const QString& endMarker, int currentMatch,
                                int* matchCount) {
  text = text.toHtmlEscaped();
  int count = 0;
  int position = 0;
  while ((position = text.indexOf(startMarker, position)) >= 0) {
    const QString color = count == currentMatch ? QStringLiteral("#fb923c")
                                                : QStringLiteral("#fff19a");
    const QString replacement =
        QStringLiteral("<a name=\"match-%1\"></a><span style=\""
                       "background-color:%2;color:#1f2328;\">")
            .arg(count)
            .arg(color);
    text.replace(position, startMarker.size(), replacement);
    ++count;
    position += replacement.size();
  }
  text.replace(endMarker, QStringLiteral("</span>"));
  if (matchCount) *matchCount = count;
  return text;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setupUi();
  qInfo("Main window UI constructed.");
  QString error;
  if (!database_.open(AppPaths::databasePath(), AppPaths::simpleLibraryPath(), &error)) {
    QMessageBox::critical(this, QStringLiteral("无法启动文搜搜"), error);
    statusLabel_->setText(error);
    return;
  }
  ready_ = true;
  refreshRoots();

  registerSearchMetaTypes();
  searchWorker_ = new SearchWorker;
  searchWorker_->moveToThread(&searchThread_);
  connect(&searchThread_, &QThread::finished, searchWorker_, &QObject::deleteLater);
  connect(searchWorker_, &SearchWorker::finished,
          this, &MainWindow::handleSearchFinished);
  searchThread_.start();
  QMetaObject::invokeMethod(searchWorker_, "initialize", Qt::QueuedConnection);

  indexWorker_ = new IndexWorker;
  indexWorker_->moveToThread(&indexThread_);
  connect(&indexThread_, &QThread::finished, indexWorker_, &QObject::deleteLater);
  connect(indexWorker_, &IndexWorker::runningChanged,
          this, &MainWindow::handleIndexRunning);
  connect(indexWorker_, &IndexWorker::finished,
          this, &MainWindow::handleIndexFinished);
  connect(indexWorker_, &IndexWorker::progress,
          this, &MainWindow::handleProgress);
  indexThread_.start();
  if (QSettings().value(QStringLiteral("index/updateOnStartup"), false).toBool()) {
    QTimer::singleShot(3000, this, &MainWindow::scheduleStartupIndexUpdate);
  }
}

MainWindow::~MainWindow() {
  if (searchWorker_) searchWorker_->cancel();
  searchThread_.quit();
  searchThread_.wait();
  if (indexWorker_) indexWorker_->cancel();
  indexThread_.quit();
  indexThread_.wait();
}

bool MainWindow::ready() const { return ready_; }

void MainWindow::scheduleStartupIndexUpdate() {
  if (searchProgress_->isVisible()) {
    QTimer::singleShot(2000, this, &MainWindow::scheduleStartupIndexUpdate);
    return;
  }
  updateAllRoots();
}

void MainWindow::setupUi() {
  setWindowTitle(QStringLiteral("文搜搜"));
  resize(1200, 760);

  auto* central = new QWidget(this);
  central->setObjectName(QStringLiteral("appShell"));
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(34, 29, 34, 21);
  layout->setSpacing(21);

  auto* toolbar = new QHBoxLayout;
  toolbar->setSpacing(18);
  auto* brandMark = new QFrame(this);
  brandMark->setObjectName(QStringLiteral("brandMark"));
  brandMark->setFixedSize(60, 60);
  auto* brandMarkLayout = new QVBoxLayout(brandMark);
  brandMarkLayout->setContentsMargins(0, 0, 0, 0);
  auto* brandGlyph = new QLabel(QStringLiteral("文"), brandMark);
  brandGlyph->setObjectName(QStringLiteral("brandGlyph"));
  brandGlyph->setAlignment(Qt::AlignCenter);
  brandMarkLayout->addWidget(brandGlyph);
  toolbar->addWidget(brandMark);

  auto* brandCopy = new QVBoxLayout;
  brandCopy->setSpacing(2);
  auto* brandTitle = new QLabel(QStringLiteral("文搜搜"), this);
  brandTitle->setObjectName(QStringLiteral("brandTitle"));
  auto* brandSubtitle = new QLabel(QStringLiteral("离线文档全文检索工作台"), this);
  brandSubtitle->setObjectName(QStringLiteral("muted"));
  brandCopy->addWidget(brandTitle);
  brandCopy->addWidget(brandSubtitle);
  toolbar->addLayout(brandCopy);
  toolbar->addStretch();

  rootsStatusPill_ = new QLabel(QStringLiteral("目录 0"), this);
  indexStatusPill_ = new QLabel(QStringLiteral("后台空闲"), this);
  resultStatusPill_ = new QLabel(QStringLiteral("本页结果 0"), this);
  for (QLabel* pill : {rootsStatusPill_, indexStatusPill_, resultStatusPill_}) {
    pill->setObjectName(QStringLiteral("statusPill"));
    toolbar->addWidget(pill);
  }
  layout->addLayout(toolbar);

  searchPanel_ = new QFrame(this);
  searchPanel_->setObjectName(QStringLiteral("searchPanel"));
  auto* controlLayout = new QVBoxLayout(searchPanel_);
  controlLayout->setContentsMargins(18, 16, 18, 16);
  controlLayout->setSpacing(12);

  auto* directoryHeader = new QHBoxLayout;
  auto* searchTitle = new QLabel(QStringLiteral("搜索本机文档"), this);
  searchTitle->setObjectName(QStringLiteral("resultTitle"));
  directoryHeader->addWidget(searchTitle);
  directoryHeader->addStretch();

  auto* managerButton = new QPushButton(QStringLiteral("索引管理"), this);
  auto* settingsButton = new QPushButton(QStringLiteral("设置"), this);
  for (QPushButton* button : {managerButton, settingsButton}) {
    directoryHeader->addWidget(button);
  }
  controlLayout->addLayout(directoryHeader);

  auto* searchRow = new QHBoxLayout;
  searchRow->setSpacing(13);
  searchEdit_ = new QLineEdit(this);
  searchEdit_->setPlaceholderText(QStringLiteral("输入中文关键词进行全文检索"));
  searchEdit_->setClearButtonEnabled(true);
  searchEdit_->setObjectName(QStringLiteral("heroSearch"));
  searchButton_ = new QPushButton(QStringLiteral("搜索"), this);
  searchButton_->setObjectName(QStringLiteral("primaryButton"));
  searchRow->addWidget(searchEdit_, 1);
  searchRow->addWidget(searchButton_);
  controlLayout->addLayout(searchRow);

  auto* filterRow = new QHBoxLayout;
  filterRow->setSpacing(13);
  auto* filterLabel = new QLabel(QStringLiteral("搜索筛选"), this);
  filterLabel->setObjectName(QStringLiteral("sectionLabel"));
  filterRow->addWidget(filterLabel);
  rootFilter_ = new QComboBox(this);
  extensionFilterButton_ = new QPushButton(this);
  extensionFilterMenu_ = new QMenu(extensionFilterButton_);
  extensionFilterButton_->setMenu(extensionFilterMenu_);
  rebuildTypeFilterMenu();
  modifiedFilter_ = new QComboBox(this);
  modifiedFilter_->addItem(QStringLiteral("全部日期"), 0);
  modifiedFilter_->addItem(QStringLiteral("最近一天"), 1);
  modifiedFilter_->addItem(QStringLiteral("最近 7 天"), 7);
  modifiedFilter_->addItem(QStringLiteral("最近 30 天"), 30);
  modifiedFilter_->addItem(QStringLiteral("最近一年"), 365);
  filterRow->addWidget(rootFilter_);
  filterRow->addWidget(extensionFilterButton_);
  filterRow->addWidget(modifiedFilter_);
  filterRow->addStretch();
  controlLayout->addLayout(filterRow);
  layout->addWidget(searchPanel_);

  auto* resultHeader = new QHBoxLayout;
  auto* resultCopy = new QVBoxLayout;
  resultCopy->setSpacing(3);
  auto* resultSection = new QLabel(QStringLiteral("搜索结果"), this);
  resultSection->setObjectName(QStringLiteral("sectionLabel"));
  resultTitleLabel_ = new QLabel(QStringLiteral("准备搜索你的文档"), this);
  resultTitleLabel_->setObjectName(QStringLiteral("resultTitle"));
  resultCopy->addWidget(resultSection);
  resultCopy->addWidget(resultTitleLabel_);
  resultHeader->addLayout(resultCopy);
  resultHeader->addStretch();
  resultHintLabel_ = new QLabel(QStringLiteral("输入关键词后开始检索"), this);
  resultHintLabel_->setObjectName(QStringLiteral("resultHint"));
  resultHeader->addWidget(resultHintLabel_);
  searchProgress_ = new QProgressBar(this);
  searchProgress_->setRange(0, 0);
  searchProgress_->setFixedWidth(143);
  searchProgress_->setTextVisible(false);
  searchProgress_->hide();
  resultHeader->addWidget(searchProgress_);
  collapseButton_ = new QPushButton(QStringLiteral("收起搜索区"), this);
  resultHeader->addWidget(collapseButton_);
  layout->addLayout(resultHeader);

  resultsTable_ = new QTableWidget(0, 5, this);
  QFont resultFont = resultsTable_->font();
  resultFont.setPointSize(resultFont.pointSize() + 1);
  resultsTable_->setFont(resultFont);
  resultsTable_->setHorizontalHeaderLabels(
      {QStringLiteral("文件名"), QStringLiteral("文件大小"), QStringLiteral("命中内容"),
       QStringLiteral("修改时间"), QStringLiteral("操作")});
  resultsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
  resultsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  resultsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  resultsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
  resultsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
  resultsTable_->setColumnWidth(0, 390);
  resultsTable_->setColumnWidth(1, 120);
  resultsTable_->setColumnWidth(3, 175);
  resultsTable_->setColumnWidth(4, 130);
  resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  resultsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultsTable_->setAlternatingRowColors(true);
  resultsTable_->verticalHeader()->setDefaultSectionSize(55);
  resultsTable_->verticalHeader()->setVisible(false);
  layout->addWidget(resultsTable_, 1);
  auto* paging = new QHBoxLayout;
  paging->addStretch();
  auto* pageSizeLabel = new QLabel(QStringLiteral("每页"), this);
  pageSizeCombo_ = new QComboBox(this);
  for (int size : {20, 50, 100}) {
    pageSizeCombo_->addItem(QString::number(size), size);
  }
  pageSizeCombo_->setCurrentIndex(pageSizeCombo_->findData(pageSize_));
  previousButton_ = new QPushButton(QStringLiteral("上一页"), this);
  nextButton_ = new QPushButton(QStringLiteral("下一页"), this);
  pageLabel_ = new QLabel(QStringLiteral("共 0 页"), this);
  auto* pageButtons = new QWidget(this);
  pageButtonsLayout_ = new QHBoxLayout(pageButtons);
  pageButtonsLayout_->setContentsMargins(0, 0, 0, 0);
  pageButtonsLayout_->setSpacing(4);
  paging->addWidget(pageSizeLabel);
  paging->addWidget(pageSizeCombo_);
  paging->addSpacing(8);
  paging->addWidget(previousButton_);
  paging->addWidget(pageButtons);
  paging->addWidget(nextButton_);
  paging->addSpacing(8);
  paging->addWidget(pageLabel_);
  layout->addLayout(paging);
  setCentralWidget(central);

  statusLabel_ = new QLabel(QStringLiteral("准备就绪"), this);
  indexProgressLabel_ = new QLabel(this);
  statusBar()->addWidget(statusLabel_, 1);
  statusBar()->addPermanentWidget(indexProgressLabel_);

  connect(searchEdit_, &QLineEdit::returnPressed, this, &MainWindow::searchFirstPage);
  connect(searchButton_, &QPushButton::clicked, this, &MainWindow::searchFirstPage);
  connect(rootFilter_, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::searchFirstPage);
  connect(modifiedFilter_, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::searchFirstPage);
  connect(managerButton, &QPushButton::clicked, this, &MainWindow::showIndexManager);
  connect(settingsButton, &QPushButton::clicked, this, &MainWindow::showSettings);
  connect(collapseButton_, &QPushButton::clicked, this, &MainWindow::toggleSearchPanel);
  connect(resultsTable_->horizontalHeader(), &QHeaderView::sectionClicked,
          this, &MainWindow::handleResultHeaderClicked);
  connect(previousButton_, &QPushButton::clicked, this, &MainWindow::searchPreviousPage);
  connect(nextButton_, &QPushButton::clicked, this, &MainWindow::searchNextPage);
  connect(pageSizeCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::changePageSize);
  connect(resultsTable_, &QTableWidget::cellDoubleClicked,
          this, &MainWindow::openSelectedDocument);
}

void MainWindow::updateAllRoots() {
  if (indexWorker_) {
    statusLabel_->setText(QStringLiteral("正在准备扫描索引目录..."));
    indexProgressLabel_->setText(QStringLiteral("正在统计待处理文件..."));
    QMetaObject::invokeMethod(indexWorker_, "updateAll", Qt::QueuedConnection);
  }
}

void MainWindow::retryFailures() {
  if (indexWorker_) {
    statusLabel_->setText(QStringLiteral("正在准备重试失败文件..."));
    indexProgressLabel_->setText(QStringLiteral("正在统计待处理文件..."));
    QMetaObject::invokeMethod(indexWorker_, "retryFailures", Qt::QueuedConnection);
  }
}

void MainWindow::cancelIndexing() {
  if (indexWorker_) indexWorker_->cancel();
}

void MainWindow::showSettings() {
  SettingsDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    rebuildTypeFilterMenu();
    applyResultFilters();
  }
}

void MainWindow::showIndexManager() {
  IndexManagerDialog dialog(&database_, this);
  connect(&dialog, &IndexManagerDialog::rootsChanged, this, [this]() {
    refreshRoots();
    searchFirstPage();
  });
  connect(&dialog, &IndexManagerDialog::updateRequested, this, [this](qint64 rootId) {
    if (indexWorker_) {
      statusLabel_->setText(QStringLiteral("正在准备扫描所选目录..."));
      indexProgressLabel_->setText(QStringLiteral("正在统计待处理文件..."));
      QMetaObject::invokeMethod(indexWorker_, "updateRoot", Qt::QueuedConnection,
                                Q_ARG(qint64, rootId));
    }
  });
  connect(&dialog, &IndexManagerDialog::updateAllRequested,
          this, &MainWindow::updateAllRoots);
  connect(&dialog, &IndexManagerDialog::retryFailuresRequested,
          this, &MainWindow::retryFailures);
  if (indexWorker_) {
    connect(indexWorker_, &IndexWorker::finished, &dialog,
            [&dialog](bool, const QString&) { dialog.refresh(); });
  }
  dialog.exec();
  refreshRoots();
}

void MainWindow::toggleSearchPanel() {
  const bool show = !searchPanel_->isVisible();
  searchPanel_->setVisible(show);
  collapseButton_->setText(show ? QStringLiteral("收起搜索区")
                                : QStringLiteral("展开搜索区"));
}

void MainWindow::handleResultHeaderClicked(int column) {
  if (column != 3) return;
  if (searchSort_ == SearchSort::Relevance) {
    searchSort_ = SearchSort::ModifiedDescending;
    resultsTable_->horizontalHeader()->setSortIndicator(3, Qt::DescendingOrder);
    resultsTable_->horizontalHeader()->setSortIndicatorShown(true);
  } else if (searchSort_ == SearchSort::ModifiedDescending) {
    searchSort_ = SearchSort::ModifiedAscending;
    resultsTable_->horizontalHeader()->setSortIndicator(3, Qt::AscendingOrder);
  } else {
    searchSort_ = SearchSort::Relevance;
    resultsTable_->horizontalHeader()->setSortIndicatorShown(false);
  }
  searchFirstPage();
}

void MainWindow::refreshRoots() {
  QString error;
  const QList<RootRecord> roots = database_.roots(&error);
  rootFilter_->blockSignals(true);
  const qint64 oldFilter = rootFilter_->currentData().toLongLong();
  rootFilter_->clear();
  rootFilter_->addItem(QStringLiteral("全部目录"), 0);
  for (const RootRecord& root : roots) {
    rootFilter_->addItem(QFileInfo(root.path).fileName().isEmpty()
                             ? root.path
                             : QFileInfo(root.path).fileName(),
                         root.id);
  }
  const int restore = rootFilter_->findData(oldFilter);
  rootFilter_->setCurrentIndex(qMax(0, restore));
  rootFilter_->blockSignals(false);
  rootsStatusPill_->setText(QStringLiteral("目录 %1").arg(roots.size()));
}

void MainWindow::searchFirstPage() {
  page_ = 0;
  runSearch();
}

void MainWindow::searchPreviousPage() {
  if (page_ > 0) {
    --page_;
    renderCurrentPage();
  }
}

void MainWindow::searchNextPage() {
  if ((page_ + 1) * pageSize_ < totalResultCount_) {
    ++page_;
    renderCurrentPage();
  }
}

void MainWindow::changePageSize() {
  pageSize_ = pageSizeCombo_->currentData().toInt();
  page_ = 0;
  renderCurrentPage();
}

void MainWindow::runSearch() {
  const QString query = searchEdit_->text().trimmed();
  const qint64 requestId = ++latestSearchRequestId_;
  if (searchWorker_) searchWorker_->cancelBefore(requestId);
  if (query.isEmpty()) {
    resultsTable_->setRowCount(0);
    cachedSearchResults_.clear();
    filteredSearchResults_.clear();
    lastSearchQuery_.clear();
    lastSearchElapsedMs_ = 0;
    lastResultCount_ = 0;
    totalResultCount_ = 0;
    page_ = 0;
    updatePagination();
    searchProgress_->hide();
    searchButton_->setEnabled(true);
    resultStatusPill_->setText(QStringLiteral("本页结果 0"));
    resultTitleLabel_->setText(QStringLiteral("准备搜索你的文档"));
    resultHintLabel_->setText(QStringLiteral("输入关键词后开始检索"));
    statusLabel_->setText(QStringLiteral("准备就绪"));
    return;
  }
  if (selectedTypeFilters().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("文件类型筛选"),
                         QStringLiteral("搜索前至少需要选择一种文件类型。"));
    return;
  }
  const int modifiedWithinDays = modifiedFilter_->currentData().toInt();
  const qint64 modifiedAfterMs =
      modifiedWithinDays > 0
          ? QDateTime::currentDateTime().addDays(-modifiedWithinDays).toMSecsSinceEpoch()
          : 0;
  searchProgress_->show();
  searchButton_->setEnabled(false);
  resultTitleLabel_->setText(QStringLiteral("正在检索全文"));
  resultHintLabel_->setText(QStringLiteral("正在搜索：%1").arg(query));
  resultStatusPill_->setText(QStringLiteral("正在搜索"));
  statusLabel_->setText(QStringLiteral("正在搜索，请稍候..."));
  QMetaObject::invokeMethod(
      searchWorker_, "search", Qt::QueuedConnection,
      Q_ARG(qint64, requestId), Q_ARG(QString, query),
      Q_ARG(qint64, rootFilter_->currentData().toLongLong()),
      Q_ARG(QString, QString()),
      Q_ARG(qint64, modifiedAfterMs), Q_ARG(SearchSort, searchSort_),
      Q_ARG(int, kMaxCachedSearchResults),
      Q_ARG(int, 0));
}

void MainWindow::handleSearchFinished(qint64 requestId,
                                      const QList<SearchResult>& results,
                                      int totalCount, const QString& error,
                                      qint64 elapsedMs) {
  if (requestId != latestSearchRequestId_) return;
  searchProgress_->hide();
  searchButton_->setEnabled(true);
  if (!error.isEmpty()) {
    if (error.contains(QStringLiteral("interrupted"), Qt::CaseInsensitive)) return;
    statusLabel_->setText(QStringLiteral("搜索失败：%1").arg(error));
    resultTitleLabel_->setText(QStringLiteral("搜索失败"));
    resultHintLabel_->setText(error);
    return;
  }
  cachedSearchResults_ = results;
  lastSearchQuery_ = searchEdit_->text().trimmed();
  lastSearchElapsedMs_ = elapsedMs;
  applyResultFilters();
  if (totalCount > kMaxCachedSearchResults) {
    resultHintLabel_->setText(
        QStringLiteral("当前关键词：%1，命中较多，仅缓存前 %2 条用于快速筛选，耗时 %3 秒")
            .arg(lastSearchQuery_)
            .arg(kMaxCachedSearchResults)
            .arg(elapsedMs / 1000.0, 0, 'f', 2));
  }
}

void MainWindow::applyResultFilters() {
  filteredSearchResults_.clear();
  for (const SearchResult& result : cachedSearchResults_) {
    if (resultPassesTypeFilter(result)) filteredSearchResults_.append(result);
  }
  page_ = qMax(0, qMin(page_, filteredSearchResults_.isEmpty()
                               ? 0
                               : (filteredSearchResults_.size() - 1) / pageSize_));
  renderCurrentPage();
}

void MainWindow::renderCurrentPage() {
  const int totalCount = filteredSearchResults_.size();
  totalResultCount_ = totalCount;
  const int first = qMin(page_ * pageSize_, totalCount);
  const int count = qMin(pageSize_, totalCount - first);
  lastResultCount_ = count;
  resultsTable_->setRowCount(count);
  for (int row = 0; row < count; ++row) {
    const SearchResult& result = filteredSearchResults_.at(first + row);
    auto* filename = new QTableWidgetItem;
    filename->setData(Qt::UserRole, result.id);
    filename->setData(Qt::UserRole + 1, result.path);
    filename->setToolTip(result.path);
    resultsTable_->setItem(row, 0, filename);
    auto* highlightedFilename = new QLabel(this);
    highlightedFilename->setTextFormat(Qt::RichText);
    highlightedFilename->setText(
        highlightedHtml(result.highlightedFilename.isEmpty()
                            ? result.filename
                            : result.highlightedFilename,
                        QStringLiteral("【"), QStringLiteral("】")));
    highlightedFilename->setToolTip(result.path);
    highlightedFilename->setFont(resultsTable_->font());
    highlightedFilename->setMargin(8);
    resultsTable_->setCellWidget(row, 0, highlightedFilename);
    resultsTable_->setItem(row, 1, new QTableWidgetItem(formatFileSize(result.size)));
    auto* snippet = new QLabel(this);
    snippet->setTextFormat(Qt::RichText);
    snippet->setText(highlightedHtml(result.snippet, QStringLiteral("【"),
                                     QStringLiteral("】")));
    snippet->setFont(resultsTable_->font());
    snippet->setMargin(8);
    resultsTable_->setCellWidget(row, 2, snippet);
    resultsTable_->setItem(row, 3, new QTableWidgetItem(formatTime(result.mtimeMs)));

    auto* actions = new QWidget(this);
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(4, 0, 4, 0);
    actionsLayout->setSpacing(3);
    auto* previewButton = new QToolButton(actions);
    previewButton->setIcon(previewActionIcon());
    previewButton->setIconSize(QSize(22, 22));
    previewButton->setToolTip(QStringLiteral("预览"));
    previewButton->setFixedSize(36, 36);
    auto* openButton = new QToolButton(actions);
    openButton->setIcon(openActionIcon());
    openButton->setIconSize(QSize(22, 22));
    openButton->setToolTip(QStringLiteral("打开文件"));
    openButton->setFixedSize(36, 36);
    auto* folderButton = new QToolButton(actions);
    folderButton->setIcon(folderActionIcon());
    folderButton->setIconSize(QSize(22, 22));
    folderButton->setToolTip(QStringLiteral("打开文件夹"));
    folderButton->setFixedSize(36, 36);
    actionsLayout->addWidget(previewButton);
    actionsLayout->addWidget(openButton);
    actionsLayout->addWidget(folderButton);
    connect(previewButton, &QToolButton::clicked, this,
            [this, result]() {
              showDocumentPreview(result.id, result.filename, result.path);
            });
    connect(openButton, &QToolButton::clicked, this,
            [this, result]() { openDocument(result.path); });
    connect(folderButton, &QToolButton::clicked, this,
            [this, result]() { openContainingFolder(result.path); });
    resultsTable_->setCellWidget(row, 4, actions);
  }
  updatePagination();
  resultStatusPill_->setText(QStringLiteral("命中 %1").arg(totalCount));
  resultTitleLabel_->setText(lastSearchQuery_.isEmpty()
                                 ? QStringLiteral("准备搜索你的文档")
                                 : filteredSearchResults_.isEmpty()
                                       ? QStringLiteral("没有匹配结果")
                                       : QStringLiteral("找到 %1 个匹配文件")
                                             .arg(totalCount));
  resultHintLabel_->setText(lastSearchQuery_.isEmpty()
                                ? QStringLiteral("输入关键词后开始检索")
                                : QStringLiteral("当前关键词：%1，耗时 %2 秒")
                                      .arg(lastSearchQuery_)
                                      .arg(lastSearchElapsedMs_ / 1000.0, 0, 'f', 2));
  statusLabel_->setText(QStringLiteral("找到 %1 个文件，耗时 %2 秒。")
                            .arg(totalCount)
                            .arg(lastSearchElapsedMs_ / 1000.0, 0, 'f', 2));
}

void MainWindow::updatePagination() {
  const int pageCount =
      totalResultCount_ > 0 ? (totalResultCount_ + pageSize_ - 1) / pageSize_ : 0;
  if (pageCount == 0) page_ = 0;
  pageLabel_->setText(QStringLiteral("共 %1 页").arg(pageCount));
  previousButton_->setEnabled(page_ > 0);
  nextButton_->setEnabled(page_ + 1 < pageCount);
  while (QLayoutItem* item = pageButtonsLayout_->takeAt(0)) {
    if (item->widget()) item->widget()->deleteLater();
    delete item;
  }
  if (pageCount == 0) return;
  const int first = qMax(0, qMin(page_ - 2, pageCount - 5));
  const int last = qMin(pageCount, first + 5);
  for (int index = first; index < last; ++index) {
    auto* button = new QPushButton(QString::number(index + 1), this);
    button->setEnabled(index != page_);
    connect(button, &QPushButton::clicked, this, [this, index]() {
      page_ = index;
      renderCurrentPage();
    });
    pageButtonsLayout_->addWidget(button);
  }
}

void MainWindow::rebuildTypeFilterMenu() {
  if (!extensionFilterMenu_) return;
  extensionFilterMenu_->clear();
  extensionActions_.clear();

  auto* selectAll = extensionFilterMenu_->addAction(QStringLiteral("全选"));
  auto* clearAll = extensionFilterMenu_->addAction(QStringLiteral("全不选"));
  connect(selectAll, &QAction::triggered, this, [this]() { setAllTypeFilters(true); });
  connect(clearAll, &QAction::triggered, this, [this]() { setAllTypeFilters(false); });
  extensionFilterMenu_->addSeparator();

  for (const QString& extension : RootPolicy::enabledExtensions()) {
    auto* action = extensionFilterMenu_->addAction(extension.toUpper());
    action->setCheckable(true);
    action->setChecked(true);
    action->setData(extension);
    extensionActions_.append(action);
    connect(action, &QAction::triggered, this, [this]() {
      if (selectedTypeFilters().isEmpty()) {
        if (auto* action = qobject_cast<QAction*>(sender())) action->setChecked(true);
        QMessageBox::warning(this, QStringLiteral("文件类型筛选"),
                             QStringLiteral("搜索前至少需要选择一种文件类型。"));
        return;
      }
      extensionFilterButton_->setText(
          QStringLiteral("类型 %1/%2")
              .arg(selectedTypeFilters().size())
              .arg(extensionActions_.size()));
      applyResultFilters();
    });
  }
  extensionFilterButton_->setText(
      QStringLiteral("类型 %1/%2")
          .arg(selectedTypeFilters().size())
          .arg(extensionActions_.size()));
}

void MainWindow::setAllTypeFilters(bool checked) {
  if (!checked && extensionActions_.size() <= 1) {
    QMessageBox::warning(this, QStringLiteral("文件类型筛选"),
                         QStringLiteral("至少需要选择一种文件类型。"));
    return;
  }
  for (QAction* action : extensionActions_) action->setChecked(checked);
  if (selectedTypeFilters().isEmpty() && !extensionActions_.isEmpty()) {
    extensionActions_.first()->setChecked(true);
  }
  extensionFilterButton_->setText(
      QStringLiteral("类型 %1/%2")
          .arg(selectedTypeFilters().size())
          .arg(extensionActions_.size()));
  applyResultFilters();
}

QStringList MainWindow::selectedTypeFilters() const {
  QStringList extensions;
  for (QAction* action : extensionActions_) {
    if (action->isChecked()) extensions.append(action->data().toString());
  }
  return extensions;
}

bool MainWindow::resultPassesTypeFilter(const SearchResult& result) const {
  return selectedTypeFilters().contains(result.extension, Qt::CaseInsensitive);
}

void MainWindow::showDocumentPreview(qint64 documentId, const QString& filename,
                                     const QString& path) {
  QString error;
  QString text = database_.previewDocument(documentId, searchEdit_->text(), &error);
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("预览：%1").arg(filename));
  dialog.resize(1040, 800);
  auto* browser = new QTextBrowser(&dialog);
  auto* previousMatch = new QPushButton(QStringLiteral("上一处"), &dialog);
  auto* nextMatch = new QPushButton(QStringLiteral("下一处"), &dialog);
  auto* matchLabel = new QLabel(&dialog);
  int matchCount = 0;
  bool truncated = false;
  if (!error.isEmpty()) {
    browser->setPlainText(error);
  } else {
    if (text.size() > kPreviewCharacterLimit) {
      text.truncate(kPreviewCharacterLimit);
      truncated = true;
    }
    QString html = anchoredHighlightedHtml(text, QStringLiteral("[[["),
                                           QStringLiteral("]]]"), 0, &matchCount);
    html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    if (truncated) {
      html += QStringLiteral("<p><b>正文较长，预览已截断。</b></p>");
    }
    browser->setHtml(html);
  }
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  auto* layout = new QVBoxLayout(&dialog);
  auto* matchNavigation = new QHBoxLayout;
  matchNavigation->addWidget(previousMatch);
  matchNavigation->addWidget(nextMatch);
  matchNavigation->addWidget(matchLabel);
  matchNavigation->addStretch();
  int currentMatch = matchCount > 0 ? 0 : -1;
  const auto renderPreview = [=, &text, &currentMatch]() {
    if (!error.isEmpty()) return;
    int ignoredCount = 0;
    QString html = anchoredHighlightedHtml(text, QStringLiteral("[[["),
                                           QStringLiteral("]]]"), currentMatch,
                                           &ignoredCount);
    html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    if (truncated) html += QStringLiteral("<p><b>正文较长，预览已截断。</b></p>");
    browser->setHtml(html);
    if (currentMatch >= 0) {
      browser->scrollToAnchor(QStringLiteral("match-%1").arg(currentMatch));
    }
  };
  const auto updateMatchNavigation = [=, &currentMatch]() {
    const bool hasMatches = matchCount > 0;
    previousMatch->setEnabled(hasMatches && currentMatch > 0);
    nextMatch->setEnabled(hasMatches && currentMatch + 1 < matchCount);
    matchLabel->setText(hasMatches
                            ? QStringLiteral("第 %1 / %2 处").arg(currentMatch + 1).arg(matchCount)
                            : QStringLiteral("没有可跳转的命中位置"));
  };
  connect(previousMatch, &QPushButton::clicked, &dialog,
          [=, &currentMatch]() {
            if (currentMatch > 0) --currentMatch;
            renderPreview();
            updateMatchNavigation();
          });
  connect(nextMatch, &QPushButton::clicked, &dialog,
          [=, &currentMatch]() {
            if (currentMatch + 1 < matchCount) ++currentMatch;
            renderPreview();
            updateMatchNavigation();
          });
  updateMatchNavigation();
  auto* pathLabel = new QLabel(path, &dialog);
  pathLabel->setObjectName(QStringLiteral("muted"));
  pathLabel->setWordWrap(true);
  layout->addWidget(pathLabel);
  layout->addLayout(matchNavigation);
  layout->addWidget(browser);
  layout->addWidget(buttons);
  if (matchCount > 0) {
    QTimer::singleShot(0, browser, renderPreview);
  }
  dialog.exec();
}

void MainWindow::openSelectedDocument() {
  openDocument(selectedDocumentPath());
}

void MainWindow::openDocument(const QString& path) {
  if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::openContainingFolder(const QString& path) {
  if (!path.isEmpty()) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
  }
}

void MainWindow::handleIndexRunning(bool running) {
  indexStatusPill_->setText(running ? QStringLiteral("正在建立索引")
                                    : QStringLiteral("后台空闲"));
  if (!running) indexProgressLabel_->clear();
}

void MainWindow::handleIndexFinished(bool success, const QString& message) {
  Q_UNUSED(success)
  statusLabel_->setText(message);
  refreshRoots();
  runSearch();
}

void MainWindow::handleProgress(const QString& currentFile, int processed,
                                int failed, int total) {
  statusLabel_->setText(currentFile.isEmpty()
                            ? QStringLiteral("正在统计待处理文件...")
                            : QStringLiteral("正在处理：%1").arg(currentFile));
  indexProgressLabel_->setText(
      QStringLiteral("已处理 %1  |  失败 %2  |  总数 %3")
          .arg(processed)
          .arg(failed)
          .arg(total));
}

QString MainWindow::selectedDocumentPath() const {
  const int row = resultsTable_->currentRow();
  return row >= 0 && resultsTable_->item(row, 0)
             ? resultsTable_->item(row, 0)->data(Qt::UserRole + 1).toString()
             : QString();
}

}  // namespace wensousou
