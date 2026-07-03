#include "database.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <sqlite3.h>

namespace wensousou {

class DatabaseSearchTest : public QObject {
  Q_OBJECT

 private slots:
  void findsNormalizedChineseContent();
  void keepsLiteralChineseBracketsOutOfHighlightMarkers();
  void returnsTotalCountAndNewestDocumentsFirst();
  void filtersExtensionsAndCanSkipExactCount();
  void filtersBySearchScope();
  void reportsRootSummaryAndFailures();
  void clearsDocumentsFromOlderSchema();
};

void DatabaseSearchTest::findsNormalizedChineseContent() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());

  Database database;
  QString error;
  QVERIFY2(database.open(temporary.filePath(QStringLiteral("index.db")),
                         QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  QVERIFY2(error.isEmpty(), qPrintable(error));
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY2(scanId > 0, qPrintable(error));
  QVERIFY2(database.upsertDocument(
               root.id, temporary.filePath(QStringLiteral("document.txt")), 32, 1234,
               QStringLiteral("投 资 处 党 支 部 2026 年 应 交 党 费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  QCOMPARE(database.search(QStringLiteral("投资处"), 0, QStringList(), 0, SearchSort::Relevance,
                           10, 0, &error).size(), 1);
  const QList<SearchResult> filenameMatch =
      database.search(QStringLiteral("document"), 0, QStringList(), 0,
                      SearchSort::Relevance, 10, 0, &error);
  QCOMPARE(filenameMatch.size(), 1);
  QCOMPARE(filenameMatch.first().highlightedFilename,
           QStringLiteral("__WSS_HIT_START__document__WSS_HIT_END__.txt"));
  QCOMPARE(database.search(QStringLiteral("党费"), 0, QStringList(), 0, SearchSort::Relevance,
                           10, 0, &error).size(),
           1);
  QCOMPARE(database.search(QStringLiteral("投资处 党费"), 0, QStringList(), 0,
                           SearchSort::Relevance, 10, 0, &error)
               .size(),
           1);
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

void DatabaseSearchTest::keepsLiteralChineseBracketsOutOfHighlightMarkers() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());

  Database database;
  QString error;
  QVERIFY2(database.open(temporary.filePath(QStringLiteral("index.db")),
                         QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY2(scanId > 0, qPrintable(error));
  QVERIFY2(database.upsertDocument(
               root.id, temporary.filePath(QStringLiteral("brackets.doc")), 32, 1234,
               QStringLiteral("【非关键词】 党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  const QList<SearchResult> results =
      database.search(QStringLiteral("党费"), 0, QStringList(), 0,
                      SearchSort::Relevance, 10, 0, &error);
  QCOMPARE(results.size(), 1);
  QVERIFY(results.first().snippet.contains(QStringLiteral("【非关键词】")));
  QVERIFY(!results.first().snippet.contains(QStringLiteral("__WSS_HIT_START__非关键词")));
  QVERIFY(results.first().snippet.contains(
      QStringLiteral("__WSS_HIT_START__党费__WSS_HIT_END__")));
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

void DatabaseSearchTest::returnsTotalCountAndNewestDocumentsFirst() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());

  Database database;
  QString error;
  QVERIFY2(database.open(temporary.filePath(QStringLiteral("index.db")),
                         QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY2(scanId > 0, qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("old.txt")),
                                   10, 1000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("new.txt")),
                                   10, 3000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("middle.txt")),
                                   10, 2000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  int totalCount = 0;
  const QList<SearchResult> firstPage =
      database.search(QStringLiteral("党费"), 0, QStringList(), 0,
                      SearchSort::ModifiedDescending, 2, 0, &error, &totalCount);
  QCOMPARE(totalCount, 3);
  QCOMPARE(firstPage.size(), 2);
  QCOMPARE(firstPage.at(0).filename, QStringLiteral("new.txt"));
  QCOMPARE(firstPage.at(1).filename, QStringLiteral("middle.txt"));
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

void DatabaseSearchTest::filtersExtensionsAndCanSkipExactCount() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());

  Database database;
  QString error;
  QVERIFY2(database.open(temporary.filePath(QStringLiteral("index.db")),
                         QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY2(scanId > 0, qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("a.doc")),
                                   10, 1000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("b.pdf")),
                                   10, 2000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("c.xlsx")),
                                   10, 3000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  int totalCount = 0;
  const QList<SearchResult> limited =
      database.search(QStringLiteral("党费"), 0,
                      QStringList{QStringLiteral("doc"), QStringLiteral("xlsx")}, 0,
                      SearchSort::ModifiedDescending, 1, 0, &error, &totalCount,
                      false);
  QCOMPARE(totalCount, 1);
  QCOMPARE(limited.size(), 1);
  QCOMPARE(limited.first().filename, QStringLiteral("c.xlsx"));
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

void DatabaseSearchTest::filtersBySearchScope() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());

  Database database;
  QString error;
  QVERIFY2(database.open(temporary.filePath(QStringLiteral("index.db")),
                         QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY2(scanId > 0, qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("contract.doc")),
                                   10, 1000, QStringLiteral("普通正文"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("plain.doc")),
                                   10, 2000, QStringLiteral("contract body"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  const QList<SearchResult> filenameOnly =
      database.search(QStringLiteral("contract"), 0, QStringList(), 0,
                      SearchSort::ModifiedDescending, 10, 0, &error, nullptr,
                      true, true, false);
  QCOMPARE(filenameOnly.size(), 1);
  QCOMPARE(filenameOnly.first().filename, QStringLiteral("contract.doc"));

  const QList<SearchResult> contentOnly =
      database.search(QStringLiteral("contract"), 0, QStringList(), 0,
                      SearchSort::ModifiedDescending, 10, 0, &error, nullptr,
                      true, false, true);
  QCOMPARE(contentOnly.size(), 1);
  QCOMPARE(contentOnly.first().filename, QStringLiteral("plain.doc"));
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

void DatabaseSearchTest::reportsRootSummaryAndFailures() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());

  Database database;
  QString error;
  QVERIFY2(database.open(temporary.filePath(QStringLiteral("index.db")),
                         QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY2(scanId > 0, qPrintable(error));
  QVERIFY2(database.upsertDocument(root.id, temporary.filePath(QStringLiteral("ok.txt")),
                                   120, 1000, QStringLiteral("党费"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.markFailure(root.id, temporary.filePath(QStringLiteral("broken.pdf")),
                                80, 2000, QStringLiteral("PDF 文件损坏。"), scanId, &error),
           qPrintable(error));
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  const QList<RootIndexSummary> summaries = database.rootIndexSummaries(&error);
  QCOMPARE(summaries.size(), 1);
  QCOMPARE(summaries.first().documentCount, 2);
  QCOMPARE(summaries.first().failedCount, 1);
  QCOMPARE(summaries.first().totalSize, 200);
  const QList<FailureRecord> failures = database.failures(root.id, &error);
  QCOMPARE(failures.size(), 1);
  QCOMPARE(failures.first().filename, QStringLiteral("broken.pdf"));
  QCOMPARE(failures.first().error, QStringLiteral("PDF 文件损坏。"));
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

void DatabaseSearchTest::clearsDocumentsFromOlderSchema() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString databasePath = temporary.filePath(QStringLiteral("index.db"));
  QString error;

  {
    Database database;
    QVERIFY2(database.open(databasePath, QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
             qPrintable(error));
    QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
    const RootRecord root = database.roots(&error).first();
    const qint64 scanId = database.beginScan(root.id, &error);
    QVERIFY2(scanId > 0, qPrintable(error));
    QVERIFY2(database.upsertDocument(
                 root.id, temporary.filePath(QStringLiteral("old.txt")), 8, 1234,
                 QStringLiteral("旧索引内容"), scanId, &error),
             qPrintable(error));
    QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
             qPrintable(error));
    QCOMPARE(database.diagnostics(&error).documentCount, 1);
  }

  sqlite3* raw = nullptr;
  QCOMPARE(sqlite3_open(databasePath.toUtf8().constData(), &raw), SQLITE_OK);
  QCOMPARE(sqlite3_exec(raw, "PRAGMA user_version=1;", nullptr, nullptr, nullptr),
           SQLITE_OK);
  sqlite3_close(raw);

  Database migrated;
  QVERIFY2(migrated.open(databasePath, QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB), &error),
           qPrintable(error));
  QCOMPARE(migrated.roots(&error).size(), 1);
  QCOMPARE(migrated.diagnostics(&error).documentCount, 0);
  QVERIFY2(error.isEmpty(), qPrintable(error));
}

}  // namespace wensousou

QTEST_MAIN(wensousou::DatabaseSearchTest)
#include "database_search_test.moc"
