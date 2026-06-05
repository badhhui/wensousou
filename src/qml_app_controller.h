#pragma once

#include "database.h"
#include "qml_models.h"

#include <QObject>
#include <QThread>

namespace wensousou {

class IndexWorker;

class QmlAppController : public QObject {
  Q_OBJECT
  Q_PROPERTY(wensousou::SearchResultsModel* resultsModel READ resultsModel CONSTANT)
  Q_PROPERTY(wensousou::RootsModel* rootsModel READ rootsModel CONSTANT)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
  Q_PROPERTY(QString progressText READ progressText NOTIFY progressTextChanged)
  Q_PROPERTY(QString resultSummary READ resultSummary NOTIFY resultSummaryChanged)
  Q_PROPERTY(bool indexRunning READ indexRunning NOTIFY indexRunningChanged)
  Q_PROPERTY(QStringList enabledExtensions READ enabledExtensions NOTIFY enabledExtensionsChanged)
  Q_PROPERTY(QStringList rootNames READ rootNames NOTIFY rootNamesChanged)

 public:
  explicit QmlAppController(QObject* parent = nullptr);
  ~QmlAppController() override;

  SearchResultsModel* resultsModel();
  RootsModel* rootsModel();
  QString statusText() const;
  QString progressText() const;
  QString resultSummary() const;
  bool indexRunning() const;
  QStringList enabledExtensions() const;
  QStringList rootNames() const;

  Q_INVOKABLE void refreshRoots();
  Q_INVOKABLE void search(const QString& query, qint64 rootId,
                          const QStringList& extensions, int modifiedDays);
  Q_INVOKABLE void addRoot();
  Q_INVOKABLE void updateAllRoots();
  Q_INVOKABLE void updateRoot(qint64 rootId);
  Q_INVOKABLE void openDocument(const QString& path);
  Q_INVOKABLE void openFolder(const QString& path);
  Q_INVOKABLE QString previewHtml(qint64 documentId, const QString& query);

 signals:
  void statusTextChanged();
  void progressTextChanged();
  void resultSummaryChanged();
  void indexRunningChanged();
  void enabledExtensionsChanged();
  void rootNamesChanged();

 private slots:
  void handleIndexRunning(bool running);
  void handleIndexFinished(bool success, const QString& message);
  void handleProgress(const QString& currentFile, int processed, int failed, int total);

 private:
  void setStatusText(const QString& value);
  void setProgressText(const QString& value);
  void setResultSummary(const QString& value);

  Database database_;
  SearchResultsModel resultsModel_;
  RootsModel rootsModel_;
  QThread indexThread_;
  IndexWorker* indexWorker_ = nullptr;
  QString statusText_ = QStringLiteral("准备就绪");
  QString progressText_;
  QString resultSummary_ = QStringLiteral("输入关键词后开始搜索");
  bool indexRunning_ = false;
};

}  // namespace wensousou
