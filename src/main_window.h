#pragma once

#include "database.h"

#include <QMainWindow>
#include <QPoint>
#include <QStringList>
#include <QThread>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QFrame;
class QCheckBox;
class QWidget;

namespace wensousou {

class IndexWorker;
class SearchWorker;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;
  bool ready() const;

 private slots:
  void updateAllRoots();
  void retryFailures();
  void cancelIndexing();
  void addIndexRoot();
  void showSettings();
  void showIndexManager();
  void toggleSearchPanel();
  void handleResultHeaderClicked(int column);
  void refreshRoots();
  void searchFirstPage();
  void searchPreviousPage();
  void searchNextPage();
  void changePageSize();
  void openSelectedDocument();
  void applyResultFilters();
  void showResultContextMenu(const QPoint& position);
  void handleIndexRunning(bool running);
  void handleIndexFinished(bool success, const QString& message);
  void handleProgress(const QString& currentFile, int processed, int failed, int total);
  void handleSearchFinished(qint64 requestId, const QList<SearchResult>& results,
                            int totalCount, const QString& error, qint64 elapsedMs);

 private:
  void setupUi();
  void scheduleStartupIndexUpdate();
  void runSearch();
  void renderCurrentPage();
  void updatePagination();
  void rebuildTypeFilterMenu();
  void setAllTypeFilters(bool checked);
  void invertTypeFilters();
  void applyFontRoles();
  void refreshSearchHistory();
  void addSearchHistory(const QString& keyword);
  void removeSearchHistory(const QString& keyword);
  void saveSearchHistory(const QStringList& history) const;
  QStringList searchHistory() const;
  void searchFromHistory(const QString& keyword);
  QStringList selectedTypeFilters() const;
  bool resultPassesTypeFilter(const SearchResult& result) const;
  bool searchesFilenames() const;
  bool searchesContents() const;
  int selectedSearchScope() const;
  int configuredSearchResultLimit() const;
  bool resultForRow(int row, SearchResult* result) const;
  void showDocumentPreview(qint64 documentId, const QString& filename,
                           const QString& path);
  void openDocument(const QString& path);
  void openContainingFolder(const QString& path);
  void copyTextToClipboard(const QString& text);
  void deleteDocumentFile(const SearchResult& result);
  QString selectedDocumentPath() const;

  Database database_;
  bool ready_ = false;
  QThread indexThread_;
  IndexWorker* indexWorker_ = nullptr;
  QThread searchThread_;
  SearchWorker* searchWorker_ = nullptr;

  QFrame* searchPanel_ = nullptr;
  QLineEdit* searchEdit_ = nullptr;
  QWidget* searchHistoryWidget_ = nullptr;
  QHBoxLayout* searchHistoryLayout_ = nullptr;
  QComboBox* rootFilter_ = nullptr;
  QCheckBox* searchFilenameCheck_ = nullptr;
  QCheckBox* searchContentCheck_ = nullptr;
  QWidget* extensionFilterWidget_ = nullptr;
  QHBoxLayout* extensionFilterLayout_ = nullptr;
  QList<QCheckBox*> extensionChecks_;
  QComboBox* modifiedFilter_ = nullptr;
  QTableWidget* resultsTable_ = nullptr;
  QLabel* rootsStatusPill_ = nullptr;
  QLabel* indexStatusPill_ = nullptr;
  QLabel* resultStatusPill_ = nullptr;
  QLabel* resultTitleLabel_ = nullptr;
  QLabel* resultHintLabel_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* indexProgressLabel_ = nullptr;
  QLabel* pageLabel_ = nullptr;
  QProgressBar* searchProgress_ = nullptr;
  QPushButton* previousButton_ = nullptr;
  QPushButton* nextButton_ = nullptr;
  QPushButton* searchButton_ = nullptr;
  QPushButton* collapseButton_ = nullptr;
  QComboBox* pageSizeCombo_ = nullptr;
  QHBoxLayout* pageButtonsLayout_ = nullptr;
  int page_ = 0;
  int pageSize_ = 50;
  int totalResultCount_ = 0;
  int lastResultCount_ = 0;
  qint64 latestSearchRequestId_ = 0;
  SearchSort searchSort_ = SearchSort::Relevance;
  QList<SearchResult> cachedSearchResults_;
  QList<SearchResult> filteredSearchResults_;
  QString lastSearchQuery_;
  qint64 lastSearchElapsedMs_ = 0;
  int lastSearchLimit_ = 0;
};

}  // namespace wensousou
