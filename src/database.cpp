#include "database.h"

#include "root_policy.h"
#include "text_normalizer.h"

#include <QFileInfo>
#include <QStringList>
#include <QVariant>

#include <sqlite3.h>

namespace wensousou {
namespace {

QString sqliteError(sqlite3* db) {
  return db ? QString::fromUtf8(sqlite3_errmsg(db))
            : QStringLiteral("SQLite 数据库未打开。");
}

class Statement {
 public:
  Statement(sqlite3* db, const QString& sql, QString* error) : db_(db) {
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db, utf8.constData(), -1, &statement_, nullptr) != SQLITE_OK &&
        error) {
      *error = sqliteError(db);
    }
  }
  ~Statement() { sqlite3_finalize(statement_); }
  sqlite3_stmt* get() const { return statement_; }
  bool valid() const { return statement_ != nullptr; }

 private:
  sqlite3* db_ = nullptr;
  sqlite3_stmt* statement_ = nullptr;
};

void bindText(sqlite3_stmt* statement, int index, const QString& value) {
  const QByteArray utf8 = value.toUtf8();
  sqlite3_bind_text(statement, index, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
}

QString columnText(sqlite3_stmt* statement, int index) {
  const unsigned char* text = sqlite3_column_text(statement, index);
  return text ? QString::fromUtf8(reinterpret_cast<const char*>(text)) : QString();
}

bool hasColumn(sqlite3* db, const QString& table, const QString& column) {
  Statement statement(db, QStringLiteral("PRAGMA table_info(%1);").arg(table),
                      nullptr);
  if (!statement.valid()) return false;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    if (columnText(statement.get(), 1) == column) return true;
  }
  return false;
}

qint64 nowMs() {
  return QDateTime::currentMSecsSinceEpoch();
}

QString buildFtsQuery(const QString& query) {
  QStringList terms;
  for (QString term : normalizeSearchText(query).split(QLatin1Char(' '),
                                                       Qt::SkipEmptyParts)) {
    term.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    terms.append(QStringLiteral("\"%1\"").arg(term));
  }
  return terms.join(QStringLiteral(" AND "));
}

QString applySearchScope(const QString& ftsQuery, bool searchFilenames,
                         bool searchContents) {
  if (ftsQuery.isEmpty() || (!searchFilenames && !searchContents)) return QString();
  if (searchFilenames && searchContents) return ftsQuery;
  return QStringLiteral("%1 : (%2)")
      .arg(searchFilenames ? QStringLiteral("filename") : QStringLiteral("content"),
           ftsQuery);
}

}  // namespace

Database::Database() = default;
Database::~Database() { close(); }

bool Database::open(const QString& databasePath, const QString& simpleLibraryPath,
                    QString* error) {
  return openInternal(databasePath, simpleLibraryPath, false, error);
}

bool Database::openReadOnly(const QString& databasePath,
                            const QString& simpleLibraryPath, QString* error) {
  return openInternal(databasePath, simpleLibraryPath, true, error);
}

bool Database::openInternal(const QString& databasePath,
                            const QString& simpleLibraryPath, bool readOnly,
                            QString* error) {
  close();
  const QByteArray path = QFileInfo(databasePath).absoluteFilePath().toUtf8();
  if (sqlite3_open_v2(path.constData(), &db_,
                      (readOnly ? SQLITE_OPEN_READONLY
                                : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) |
                          SQLITE_OPEN_FULLMUTEX,
                      nullptr) != SQLITE_OK) {
    if (error) *error = sqliteError(db_);
    close();
    return false;
  }
  sqlite3_busy_timeout(db_, 5000);
  if (sqlite3_enable_load_extension(db_, 1) != SQLITE_OK) {
    if (error) *error = QStringLiteral("无法启用 SQLite 扩展：%1").arg(sqliteError(db_));
    close();
    return false;
  }
  char* extensionError = nullptr;
  const QByteArray libraryPath = QFileInfo(simpleLibraryPath).absoluteFilePath().toUtf8();
  if (sqlite3_load_extension(db_, libraryPath.constData(), "sqlite3_simple_init",
                             &extensionError) != SQLITE_OK) {
    if (error) {
      *error = QStringLiteral("无法加载 simple 分词扩展：%1")
                   .arg(QString::fromUtf8(extensionError ? extensionError : ""));
    }
    sqlite3_free(extensionError);
    close();
    return false;
  }
  if (readOnly &&
      (!execute(QStringLiteral("PRAGMA query_only=ON;"), error) ||
       !execute(QStringLiteral("PRAGMA temp_store=MEMORY;"), error) ||
       !execute(QStringLiteral("PRAGMA cache_size=-32768;"), error))) {
    close();
    return false;
  }
  if (!readOnly && !initializeSchema(error)) {
    close();
    return false;
  }
  return true;
}

void Database::close() {
  if (db_) sqlite3_close_v2(db_);
  db_ = nullptr;
}

bool Database::isOpen() const { return db_ != nullptr; }

bool Database::initializeSchema(QString* error) {
  if (!execute(QStringLiteral("PRAGMA journal_mode=WAL;"), error) ||
      !execute(QStringLiteral("PRAGMA foreign_keys=ON;"), error) ||
      !execute(QStringLiteral("PRAGMA synchronous=NORMAL;"), error) ||
      !execute(QStringLiteral(R"SQL(
CREATE TABLE IF NOT EXISTS roots (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  path TEXT NOT NULL UNIQUE,
  created_at_ms INTEGER NOT NULL,
  last_scan_at_ms INTEGER NOT NULL DEFAULT 0,
  last_scan_status TEXT NOT NULL DEFAULT '',
  last_error TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS scan_runs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  root_id INTEGER NOT NULL REFERENCES roots(id) ON DELETE CASCADE,
  started_at_ms INTEGER NOT NULL,
  finished_at_ms INTEGER NOT NULL DEFAULT 0,
  status TEXT NOT NULL DEFAULT 'running',
  message TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS documents (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  root_id INTEGER NOT NULL REFERENCES roots(id) ON DELETE CASCADE,
  path TEXT NOT NULL UNIQUE,
  filename TEXT NOT NULL,
  extension TEXT NOT NULL,
  size INTEGER NOT NULL,
  mtime_ms INTEGER NOT NULL,
  content TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'ok',
  error TEXT NOT NULL DEFAULT '',
  last_seen_scan_id INTEGER,
  indexed_at_ms INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS documents_root_id ON documents(root_id);
CREATE INDEX IF NOT EXISTS documents_status ON documents(status);
CREATE INDEX IF NOT EXISTS documents_mtime_ms ON documents(mtime_ms);
CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts USING fts5(
  filename,
  content,
  content='documents',
  content_rowid='id',
  tokenize='simple 0'
);
CREATE TRIGGER IF NOT EXISTS documents_ai AFTER INSERT ON documents BEGIN
  INSERT INTO documents_fts(rowid, filename, content)
  VALUES (new.id, new.filename, new.content);
END;
CREATE TRIGGER IF NOT EXISTS documents_ad AFTER DELETE ON documents BEGIN
  INSERT INTO documents_fts(documents_fts, rowid, filename, content)
  VALUES ('delete', old.id, old.filename, old.content);
END;
CREATE TRIGGER IF NOT EXISTS documents_au AFTER UPDATE OF filename, content ON documents BEGIN
  INSERT INTO documents_fts(documents_fts, rowid, filename, content)
  VALUES ('delete', old.id, old.filename, old.content);
  INSERT INTO documents_fts(rowid, filename, content)
  VALUES (new.id, new.filename, new.content);
END;
)SQL"), error)) {
    return false;
  }
  return migrateSchema(error);
}

bool Database::migrateSchema(QString* error) {
  constexpr int kSchemaVersion = 3;
  Statement version(db_, QStringLiteral("PRAGMA user_version;"), error);
  if (!version.valid() || sqlite3_step(version.get()) != SQLITE_ROW) {
    if (error && error->isEmpty()) *error = sqliteError(db_);
    return false;
  }
  const int currentVersion = sqlite3_column_int(version.get(), 0);
  if (currentVersion < 2) {
    // Version 2 normalizes extracted Chinese text before indexing. Existing
    // fingerprints cannot reveal whether a document needs reparsing, so retain
    // configured roots and rebuild document rows during the next background scan.
    if (!execute(QStringLiteral(R"SQL(
BEGIN IMMEDIATE;
DELETE FROM documents;
INSERT INTO documents_fts(documents_fts) VALUES('rebuild');
PRAGMA user_version=2;
COMMIT;
)SQL"), error)) {
      rollback();
      return false;
    }
  }
  if (currentVersion < 3) {
    // Cache root statistics so opening index management does not scan the
    // potentially large documents table on every visit.
    if (!begin(error)) return false;
    if ((!hasColumn(db_, QStringLiteral("roots"), QStringLiteral("document_count")) &&
         !execute(QStringLiteral(
             "ALTER TABLE roots ADD COLUMN document_count INTEGER NOT NULL DEFAULT 0;"),
                  error)) ||
        (!hasColumn(db_, QStringLiteral("roots"), QStringLiteral("failed_count")) &&
         !execute(QStringLiteral(
             "ALTER TABLE roots ADD COLUMN failed_count INTEGER NOT NULL DEFAULT 0;"),
                  error)) ||
        (!hasColumn(db_, QStringLiteral("roots"), QStringLiteral("total_size")) &&
         !execute(QStringLiteral(
             "ALTER TABLE roots ADD COLUMN total_size INTEGER NOT NULL DEFAULT 0;"),
                  error)) ||
        !execute(QStringLiteral(R"SQL(
UPDATE roots SET
  document_count=(SELECT COUNT(*) FROM documents d WHERE d.root_id=roots.id),
  failed_count=(SELECT COUNT(*) FROM documents d WHERE d.root_id=roots.id AND d.status='failed'),
  total_size=COALESCE((SELECT SUM(d.size) FROM documents d WHERE d.root_id=roots.id),0);
PRAGMA user_version=3;
)SQL"),
                 error) ||
        !commit(error)) {
      rollback();
      return false;
    }
  }
  return true;
}

bool Database::execute(const QString& sql, QString* error) const {
  char* message = nullptr;
  const QByteArray utf8 = sql.toUtf8();
  if (sqlite3_exec(db_, utf8.constData(), nullptr, nullptr, &message) != SQLITE_OK) {
    if (error) *error = QString::fromUtf8(message ? message : "");
    sqlite3_free(message);
    return false;
  }
  return true;
}

bool Database::begin(QString* error) const {
  return execute(QStringLiteral("BEGIN IMMEDIATE;"), error);
}
bool Database::commit(QString* error) const {
  return execute(QStringLiteral("COMMIT;"), error);
}
void Database::rollback() const { execute(QStringLiteral("ROLLBACK;"), nullptr); }

QList<RootRecord> Database::roots(QString* error) const {
  QList<RootRecord> records;
  Statement statement(db_, QStringLiteral(
      "SELECT id,path,created_at_ms,last_scan_at_ms,last_scan_status,last_error "
      "FROM roots ORDER BY path;"), error);
  if (!statement.valid()) return records;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    RootRecord root;
    root.id = sqlite3_column_int64(statement.get(), 0);
    root.path = columnText(statement.get(), 1);
    root.createdAtMs = sqlite3_column_int64(statement.get(), 2);
    root.lastScanAtMs = sqlite3_column_int64(statement.get(), 3);
    root.lastScanStatus = columnText(statement.get(), 4);
    root.lastError = columnText(statement.get(), 5);
    records.append(root);
  }
  return records;
}

bool Database::addRoot(const QString& path, QString* error) {
  const QList<RootRecord> existing = roots(error);
  QStringList paths;
  for (const RootRecord& root : existing) paths.append(root.path);
  if (!RootPolicy::canAdd(path, paths, error)) return false;
  Statement statement(db_, QStringLiteral(
      "INSERT INTO roots(path,created_at_ms) VALUES(?,?);"), error);
  if (!statement.valid()) return false;
  bindText(statement.get(), 1, RootPolicy::normalize(path));
  sqlite3_bind_int64(statement.get(), 2, nowMs());
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    return false;
  }
  return true;
}

bool Database::removeRoot(qint64 rootId, QString* error) {
  Statement statement(db_, QStringLiteral("DELETE FROM roots WHERE id=?;"), error);
  if (!statement.valid()) return false;
  sqlite3_bind_int64(statement.get(), 1, rootId);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    return false;
  }
  return true;
}

bool Database::removeDocument(qint64 documentId, QString* error) {
  if (!begin(error)) return false;
  qint64 rootId = 0;
  {
    Statement lookup(db_, QStringLiteral("SELECT root_id FROM documents WHERE id=?;"),
                     error);
    if (!lookup.valid()) {
      rollback();
      return false;
    }
    sqlite3_bind_int64(lookup.get(), 1, documentId);
    const int lookupResult = sqlite3_step(lookup.get());
    if (lookupResult == SQLITE_ROW) {
      rootId = sqlite3_column_int64(lookup.get(), 0);
    } else if (lookupResult != SQLITE_DONE) {
      if (error) *error = sqliteError(db_);
      rollback();
      return false;
    }
  }

  Statement remove(db_, QStringLiteral("DELETE FROM documents WHERE id=?;"), error);
  if (!remove.valid()) {
    rollback();
    return false;
  }
  sqlite3_bind_int64(remove.get(), 1, documentId);
  if (sqlite3_step(remove.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    rollback();
    return false;
  }

  if (rootId > 0) {
    Statement root(db_, QStringLiteral(
        "UPDATE roots SET "
        "document_count=(SELECT COUNT(*) FROM documents WHERE root_id=?),"
        "failed_count=(SELECT COUNT(*) FROM documents WHERE root_id=? AND status='failed'),"
        "total_size=COALESCE((SELECT SUM(size) FROM documents WHERE root_id=?),0) "
        "WHERE id=?;"),
        error);
    if (!root.valid()) {
      rollback();
      return false;
    }
    sqlite3_bind_int64(root.get(), 1, rootId);
    sqlite3_bind_int64(root.get(), 2, rootId);
    sqlite3_bind_int64(root.get(), 3, rootId);
    sqlite3_bind_int64(root.get(), 4, rootId);
    if (sqlite3_step(root.get()) != SQLITE_DONE) {
      if (error) *error = sqliteError(db_);
      rollback();
      return false;
    }
  }
  return commit(error);
}

QList<RootIndexSummary> Database::rootIndexSummaries(QString* error) const {
  QList<RootIndexSummary> records;
  Statement statement(db_, QStringLiteral(R"SQL(
SELECT r.id,r.path,
       r.document_count,
       r.failed_count,
       r.total_size,
       r.last_scan_at_ms,r.last_scan_status,r.last_error
FROM roots r
ORDER BY r.path;
)SQL"), error);
  if (!statement.valid()) return records;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    RootIndexSummary record;
    record.id = sqlite3_column_int64(statement.get(), 0);
    record.path = columnText(statement.get(), 1);
    record.documentCount = sqlite3_column_int(statement.get(), 2);
    record.failedCount = sqlite3_column_int(statement.get(), 3);
    record.totalSize = sqlite3_column_int64(statement.get(), 4);
    record.lastScanAtMs = sqlite3_column_int64(statement.get(), 5);
    record.lastScanStatus = columnText(statement.get(), 6);
    record.lastError = columnText(statement.get(), 7);
    records.append(record);
  }
  return records;
}

QList<FailureRecord> Database::failures(qint64 rootId, QString* error) const {
  QList<FailureRecord> records;
  QString sql = QStringLiteral(
      "SELECT id,root_id,path,filename,size,mtime_ms,indexed_at_ms,error "
      "FROM documents WHERE status='failed' ");
  if (rootId > 0) sql += QStringLiteral("AND root_id=? ");
  sql += QStringLiteral("ORDER BY indexed_at_ms DESC,path;");
  Statement statement(db_, sql, error);
  if (!statement.valid()) return records;
  if (rootId > 0) sqlite3_bind_int64(statement.get(), 1, rootId);
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    FailureRecord record;
    record.id = sqlite3_column_int64(statement.get(), 0);
    record.rootId = sqlite3_column_int64(statement.get(), 1);
    record.path = columnText(statement.get(), 2);
    record.filename = columnText(statement.get(), 3);
    record.size = sqlite3_column_int64(statement.get(), 4);
    record.mtimeMs = sqlite3_column_int64(statement.get(), 5);
    record.indexedAtMs = sqlite3_column_int64(statement.get(), 6);
    record.error = columnText(statement.get(), 7);
    records.append(record);
  }
  return records;
}

qint64 Database::beginScan(qint64 rootId, QString* error) {
  Statement statement(db_, QStringLiteral(
      "INSERT INTO scan_runs(root_id,started_at_ms) VALUES(?,?);"), error);
  if (!statement.valid()) return 0;
  sqlite3_bind_int64(statement.get(), 1, rootId);
  sqlite3_bind_int64(statement.get(), 2, nowMs());
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    return 0;
  }
  return sqlite3_last_insert_rowid(db_);
}

bool Database::finishScan(qint64 scanId, qint64 rootId, bool success,
                          const QString& message, QString* error) {
  if (!begin(error)) return false;
  if (success) {
    Statement cleanup(db_, QStringLiteral(
        "DELETE FROM documents WHERE root_id=? AND "
        "COALESCE(last_seen_scan_id,0)<>?;"), error);
    if (!cleanup.valid()) {
      rollback();
      return false;
    }
    sqlite3_bind_int64(cleanup.get(), 1, rootId);
    sqlite3_bind_int64(cleanup.get(), 2, scanId);
    if (sqlite3_step(cleanup.get()) != SQLITE_DONE) {
      if (error) *error = sqliteError(db_);
      rollback();
      return false;
    }
  }
  Statement run(db_, QStringLiteral(
      "UPDATE scan_runs SET finished_at_ms=?,status=?,message=? WHERE id=?;"), error);
  Statement root(db_, QStringLiteral(
      "UPDATE roots SET last_scan_at_ms=?,last_scan_status=?,last_error=?,"
      "document_count=(SELECT COUNT(*) FROM documents WHERE root_id=?),"
      "failed_count=(SELECT COUNT(*) FROM documents WHERE root_id=? AND status='failed'),"
      "total_size=COALESCE((SELECT SUM(size) FROM documents WHERE root_id=?),0) "
      "WHERE id=?;"),
      error);
  if (!run.valid() || !root.valid()) {
    rollback();
    return false;
  }
  const qint64 finished = nowMs();
  sqlite3_bind_int64(run.get(), 1, finished);
  bindText(run.get(), 2, success ? QStringLiteral("ok") : QStringLiteral("cancelled"));
  bindText(run.get(), 3, message);
  sqlite3_bind_int64(run.get(), 4, scanId);
  sqlite3_bind_int64(root.get(), 1, finished);
  bindText(root.get(), 2, success ? QStringLiteral("ok") : QStringLiteral("cancelled"));
  bindText(root.get(), 3, message);
  sqlite3_bind_int64(root.get(), 4, rootId);
  sqlite3_bind_int64(root.get(), 5, rootId);
  sqlite3_bind_int64(root.get(), 6, rootId);
  sqlite3_bind_int64(root.get(), 7, rootId);
  if (sqlite3_step(run.get()) != SQLITE_DONE || sqlite3_step(root.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    rollback();
    return false;
  }
  return commit(error);
}

DocumentFingerprint Database::fingerprint(const QString& path, QString* error) const {
  DocumentFingerprint fingerprint;
  Statement statement(db_, QStringLiteral(
      "SELECT size,mtime_ms,status FROM documents WHERE path=?;"), error);
  if (!statement.valid()) return fingerprint;
  bindText(statement.get(), 1, path);
  if (sqlite3_step(statement.get()) == SQLITE_ROW) {
    fingerprint.found = true;
    fingerprint.size = sqlite3_column_int64(statement.get(), 0);
    fingerprint.mtimeMs = sqlite3_column_int64(statement.get(), 1);
    fingerprint.status = columnText(statement.get(), 2);
  }
  return fingerprint;
}

bool Database::markSeen(const QString& path, qint64 scanId, QString* error) {
  Statement statement(db_, QStringLiteral(
      "UPDATE documents SET last_seen_scan_id=? WHERE path=?;"), error);
  if (!statement.valid()) return false;
  sqlite3_bind_int64(statement.get(), 1, scanId);
  bindText(statement.get(), 2, path);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    return false;
  }
  return true;
}

bool Database::upsertDocument(qint64 rootId, const QString& path, qint64 size,
                              qint64 mtimeMs, const QString& content, qint64 scanId,
                              QString* error) {
  Statement statement(db_, QStringLiteral(R"SQL(
INSERT INTO documents(root_id,path,filename,extension,size,mtime_ms,content,status,error,last_seen_scan_id,indexed_at_ms)
VALUES(?,?,?,?,?,?,?,'ok','',?,?)
ON CONFLICT(path) DO UPDATE SET
  root_id=excluded.root_id, filename=excluded.filename, extension=excluded.extension,
  size=excluded.size, mtime_ms=excluded.mtime_ms, content=excluded.content,
  status='ok', error='', last_seen_scan_id=excluded.last_seen_scan_id,
  indexed_at_ms=excluded.indexed_at_ms;
)SQL"), error);
  if (!statement.valid()) return false;
  const QFileInfo info(path);
  sqlite3_bind_int64(statement.get(), 1, rootId);
  bindText(statement.get(), 2, path);
  bindText(statement.get(), 3, info.fileName());
  bindText(statement.get(), 4, info.suffix().toLower());
  sqlite3_bind_int64(statement.get(), 5, size);
  sqlite3_bind_int64(statement.get(), 6, mtimeMs);
  bindText(statement.get(), 7, normalizeExtractedText(content));
  sqlite3_bind_int64(statement.get(), 8, scanId);
  sqlite3_bind_int64(statement.get(), 9, nowMs());
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    return false;
  }
  return true;
}

bool Database::markFailure(qint64 rootId, const QString& path, qint64 size,
                           qint64 mtimeMs, const QString& message, qint64 scanId,
                           QString* error) {
  Statement statement(db_, QStringLiteral(R"SQL(
INSERT INTO documents(root_id,path,filename,extension,size,mtime_ms,content,status,error,last_seen_scan_id,indexed_at_ms)
VALUES(?,?,?,?,?,?,'','failed',?,?,?)
ON CONFLICT(path) DO UPDATE SET
  root_id=excluded.root_id, filename=excluded.filename, extension=excluded.extension,
  size=excluded.size, mtime_ms=excluded.mtime_ms, content='', status='failed',
  error=excluded.error, last_seen_scan_id=excluded.last_seen_scan_id,
  indexed_at_ms=excluded.indexed_at_ms;
)SQL"), error);
  if (!statement.valid()) return false;
  const QFileInfo info(path);
  sqlite3_bind_int64(statement.get(), 1, rootId);
  bindText(statement.get(), 2, path);
  bindText(statement.get(), 3, info.fileName());
  bindText(statement.get(), 4, info.suffix().toLower());
  sqlite3_bind_int64(statement.get(), 5, size);
  sqlite3_bind_int64(statement.get(), 6, mtimeMs);
  bindText(statement.get(), 7, message);
  sqlite3_bind_int64(statement.get(), 8, scanId);
  sqlite3_bind_int64(statement.get(), 9, nowMs());
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    if (error) *error = sqliteError(db_);
    return false;
  }
  return true;
}

QList<SearchResult> Database::search(const QString& query, qint64 rootId,
                                     const QStringList& extensions,
                                     qint64 modifiedAfterMs, SearchSort sort,
                                     int limit, int offset, QString* error,
                                     int* totalCount, bool countTotal,
                                     bool searchFilenames,
                                     bool searchContents) const {
  QList<SearchResult> results;
  const QString ftsQuery =
      applySearchScope(buildFtsQuery(query), searchFilenames, searchContents);
  if (totalCount) *totalCount = 0;
  if (ftsQuery.isEmpty()) return results;
  QStringList normalizedExtensions;
  for (QString extension : extensions) {
    extension = extension.trimmed().toLower();
    if (!extension.isEmpty() && !normalizedExtensions.contains(extension)) {
      normalizedExtensions.append(extension);
    }
  }
  QString where = QStringLiteral(
      "FROM documents_fts JOIN documents d ON d.id=documents_fts.rowid "
      "WHERE documents_fts MATCH ? AND d.status='ok' ");
  if (rootId > 0) where += QStringLiteral("AND d.root_id=? ");
  if (!normalizedExtensions.isEmpty()) {
    where += QStringLiteral("AND d.extension IN (");
    for (int index = 0; index < normalizedExtensions.size(); ++index) {
      if (index > 0) where += QLatin1Char(',');
      where += QLatin1Char('?');
    }
    where += QStringLiteral(") ");
  }
  if (modifiedAfterMs > 0) where += QStringLiteral("AND d.mtime_ms>=? ");

  auto bindFilters = [&](sqlite3_stmt* statement) {
    int parameter = 1;
    bindText(statement, parameter++, ftsQuery);
    if (rootId > 0) sqlite3_bind_int64(statement, parameter++, rootId);
    for (const QString& extension : normalizedExtensions) {
      bindText(statement, parameter++, extension);
    }
    if (modifiedAfterMs > 0) sqlite3_bind_int64(statement, parameter++, modifiedAfterMs);
    return parameter;
  };

  if (totalCount && countTotal) {
    Statement count(db_, QStringLiteral("SELECT COUNT(*) ") + where + QLatin1Char(';'),
                    error);
    if (!count.valid()) return results;
    bindFilters(count.get());
    if (sqlite3_step(count.get()) != SQLITE_ROW) {
      if (error) *error = sqliteError(db_);
      return results;
    }
    *totalCount = sqlite3_column_int(count.get(), 0);
  }

  QString orderBy;
  switch (sort) {
    case SearchSort::ModifiedDescending:
      orderBy = QStringLiteral("ORDER BY d.mtime_ms DESC,documents_fts.rank ");
      break;
    case SearchSort::ModifiedAscending:
      orderBy = QStringLiteral("ORDER BY d.mtime_ms ASC,documents_fts.rank ");
      break;
    case SearchSort::Relevance:
      orderBy = QStringLiteral("ORDER BY documents_fts.rank,d.mtime_ms DESC ");
      break;
  }
  QString pagination;
  if (limit > 0) {
    pagination = QStringLiteral("LIMIT ? OFFSET ?;");
  } else if (offset > 0) {
    pagination = QStringLiteral("LIMIT -1 OFFSET ?;");
  } else {
    pagination = QStringLiteral(";");
  }
  const QString sql =
      QStringLiteral(
          "SELECT d.id,d.root_id,d.filename,d.path,d.extension,d.size,d.mtime_ms,"
          "documents_fts.rank ") +
      where + orderBy + pagination;
  Statement statement(db_, sql, error);
  if (!statement.valid()) return results;
  int parameter = bindFilters(statement.get());
  if (limit > 0) {
    sqlite3_bind_int(statement.get(), parameter++, limit);
    sqlite3_bind_int(statement.get(), parameter++, offset);
  } else if (offset > 0) {
    sqlite3_bind_int(statement.get(), parameter++, offset);
  }
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    SearchResult result;
    result.id = sqlite3_column_int64(statement.get(), 0);
    result.rootId = sqlite3_column_int64(statement.get(), 1);
    result.filename = columnText(statement.get(), 2);
    result.path = columnText(statement.get(), 3);
    result.extension = columnText(statement.get(), 4);
    result.size = sqlite3_column_int64(statement.get(), 5);
    result.mtimeMs = sqlite3_column_int64(statement.get(), 6);
    result.rank = sqlite3_column_double(statement.get(), 7);
    results.append(result);
  }
  if (sqlite3_errcode(db_) != SQLITE_OK && sqlite3_errcode(db_) != SQLITE_DONE &&
      error) {
    *error = sqliteError(db_);
  }
  if (totalCount && !countTotal) *totalCount = results.size();
  Statement snippet(db_, QStringLiteral(
      "SELECT highlight(documents_fts,0,'【','】'),"
      "simple_snippet(documents_fts,1,'【','】','...',40) "
      "FROM documents_fts WHERE rowid=? AND documents_fts MATCH ?;"),
                    error);
  if (!snippet.valid()) return results;
  for (SearchResult& result : results) {
    sqlite3_bind_int64(snippet.get(), 1, result.id);
    bindText(snippet.get(), 2, ftsQuery);
    if (sqlite3_step(snippet.get()) == SQLITE_ROW) {
      result.highlightedFilename = columnText(snippet.get(), 0);
      result.snippet = columnText(snippet.get(), 1);
    }
    sqlite3_reset(snippet.get());
    sqlite3_clear_bindings(snippet.get());
  }
  return results;
}

bool Database::warmUpSearch(QString* error) const {
  Statement statement(
      db_,
      QStringLiteral("SELECT rowid FROM documents_fts "
                     "WHERE documents_fts MATCH '\"文搜搜\"' LIMIT 1;"),
      error);
  if (!statement.valid()) return false;
  const int result = sqlite3_step(statement.get());
  return result == SQLITE_ROW || result == SQLITE_DONE;
}

QString Database::previewDocument(qint64 documentId, const QString& query,
                                  QString* error) const {
  const bool highlight = !query.trimmed().isEmpty();
  const QString ftsQuery = buildFtsQuery(query);
  const QString sql = highlight
      ? QStringLiteral(
            "SELECT simple_highlight(documents_fts,1,'[[[',']]]') "
            "FROM documents_fts WHERE rowid=? AND documents_fts MATCH ?;")
      : QStringLiteral("SELECT content FROM documents WHERE id=?;");
  Statement statement(db_, sql, error);
  if (!statement.valid()) return QString();
  sqlite3_bind_int64(statement.get(), 1, documentId);
  if (highlight) bindText(statement.get(), 2, ftsQuery);
  return sqlite3_step(statement.get()) == SQLITE_ROW
             ? columnText(statement.get(), 0)
             : QString();
}

int Database::failureCount(QString* error) const {
  Statement statement(db_, QStringLiteral(
      "SELECT COUNT(*) FROM documents WHERE status='failed';"), error);
  return statement.valid() && sqlite3_step(statement.get()) == SQLITE_ROW
             ? sqlite3_column_int(statement.get(), 0)
             : 0;
}

IndexDiagnostics Database::diagnostics(QString* error) const {
  IndexDiagnostics diagnostics;
  Statement counts(db_, QStringLiteral(R"SQL(
SELECT
  (SELECT COUNT(*) FROM roots),
  COUNT(*),
  SUM(CASE WHEN status='ok' THEN 1 ELSE 0 END),
  SUM(CASE WHEN status='failed' THEN 1 ELSE 0 END),
  SUM(CASE WHEN status='ok' AND content='' THEN 1 ELSE 0 END)
FROM documents;
)SQL"), error);
  if (!counts.valid() || sqlite3_step(counts.get()) != SQLITE_ROW) {
    if (error && error->isEmpty()) *error = sqliteError(db_);
    return diagnostics;
  }
  diagnostics.rootCount = sqlite3_column_int(counts.get(), 0);
  diagnostics.documentCount = sqlite3_column_int(counts.get(), 1);
  diagnostics.okCount = sqlite3_column_int(counts.get(), 2);
  diagnostics.failedCount = sqlite3_column_int(counts.get(), 3);
  diagnostics.emptyContentCount = sqlite3_column_int(counts.get(), 4);

  Statement fts(db_, QStringLiteral("SELECT COUNT(*) FROM documents_fts;"), error);
  if (!fts.valid() || sqlite3_step(fts.get()) != SQLITE_ROW) {
    if (error && error->isEmpty()) *error = sqliteError(db_);
    return diagnostics;
  }
  diagnostics.ftsRowCount = sqlite3_column_int(fts.get(), 0);

  Statement failures(db_, QStringLiteral(
      "SELECT filename,error FROM documents WHERE status='failed' "
      "ORDER BY indexed_at_ms DESC LIMIT 10;"), error);
  if (!failures.valid()) return diagnostics;
  while (sqlite3_step(failures.get()) == SQLITE_ROW) {
    diagnostics.recentFailures.append(
        QStringLiteral("%1：%2").arg(columnText(failures.get(), 0),
                                    columnText(failures.get(), 1)));
  }
  return diagnostics;
}

void Database::interrupt() {
  if (db_) sqlite3_interrupt(db_);
}

}  // namespace wensousou
