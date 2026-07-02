#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct sqlite3;

namespace wensousou {

struct RootRecord {
  qint64 id = 0;
  QString path;
  qint64 createdAtMs = 0;
  qint64 lastScanAtMs = 0;
  QString lastScanStatus;
  QString lastError;
};

struct DocumentFingerprint {
  bool found = false;
  qint64 size = 0;
  qint64 mtimeMs = 0;
  QString status;
};

struct SearchResult {
  qint64 id = 0;
  qint64 rootId = 0;
  QString filename;
  QString highlightedFilename;
  QString path;
  QString extension;
  qint64 size = 0;
  qint64 mtimeMs = 0;
  QString snippet;
  double rank = 0.0;
};

enum class SearchSort {
  Relevance,
  ModifiedDescending,
  ModifiedAscending
};

struct RootIndexSummary {
  qint64 id = 0;
  QString path;
  int documentCount = 0;
  int failedCount = 0;
  qint64 totalSize = 0;
  qint64 lastScanAtMs = 0;
  QString lastScanStatus;
  QString lastError;
};

struct FailureRecord {
  qint64 id = 0;
  qint64 rootId = 0;
  QString path;
  QString filename;
  qint64 size = 0;
  qint64 mtimeMs = 0;
  qint64 indexedAtMs = 0;
  QString error;
};

struct IndexDiagnostics {
  int rootCount = 0;
  int documentCount = 0;
  int okCount = 0;
  int failedCount = 0;
  int emptyContentCount = 0;
  int ftsRowCount = 0;
  QStringList recentFailures;
};

class Database {
 public:
  Database();
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  bool open(const QString& databasePath, const QString& simpleLibraryPath,
            QString* error);
  bool openReadOnly(const QString& databasePath, const QString& simpleLibraryPath,
                    QString* error);
  void close();
  bool isOpen() const;

  QList<RootRecord> roots(QString* error = nullptr) const;
  bool addRoot(const QString& path, QString* error);
  bool removeRoot(qint64 rootId, QString* error);
  QList<RootIndexSummary> rootIndexSummaries(QString* error = nullptr) const;
  QList<FailureRecord> failures(qint64 rootId = 0, QString* error = nullptr) const;

  qint64 beginScan(qint64 rootId, QString* error);
  bool finishScan(qint64 scanId, qint64 rootId, bool success,
                  const QString& message, QString* error);
  DocumentFingerprint fingerprint(const QString& path, QString* error) const;
  bool markSeen(const QString& path, qint64 scanId, QString* error);
  bool upsertDocument(qint64 rootId, const QString& path, qint64 size,
                      qint64 mtimeMs, const QString& content, qint64 scanId,
                      QString* error);
  bool markFailure(qint64 rootId, const QString& path, qint64 size,
                   qint64 mtimeMs, const QString& message, qint64 scanId,
                   QString* error);

  QList<SearchResult> search(const QString& query, qint64 rootId,
                             const QStringList& extensions, qint64 modifiedAfterMs,
                             SearchSort sort, int limit, int offset, QString* error,
                             int* totalCount = nullptr, bool countTotal = true,
                             bool searchFilenames = true,
                             bool searchContents = true) const;
  bool removeDocument(qint64 documentId, QString* error);
  bool warmUpSearch(QString* error) const;
  QString previewDocument(qint64 documentId, const QString& query,
                          QString* error) const;
  int failureCount(QString* error = nullptr) const;
  IndexDiagnostics diagnostics(QString* error = nullptr) const;
  void interrupt();

 private:
  bool openInternal(const QString& databasePath, const QString& simpleLibraryPath,
                    bool readOnly, QString* error);
  bool initializeSchema(QString* error);
  bool migrateSchema(QString* error);
  bool execute(const QString& sql, QString* error) const;
  bool begin(QString* error) const;
  bool commit(QString* error) const;
  void rollback() const;

  sqlite3* db_ = nullptr;
};

}  // namespace wensousou

Q_DECLARE_METATYPE(QList<wensousou::SearchResult>)
Q_DECLARE_METATYPE(QList<wensousou::RootIndexSummary>)
Q_DECLARE_METATYPE(wensousou::SearchSort)
