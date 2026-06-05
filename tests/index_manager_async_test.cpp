#include "database.h"
#include "index_manager_dialog.h"

#include <QElapsedTimer>
#include <QMessageBox>
#include <QMetaObject>
#include <QApplication>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace wensousou {

class IndexManagerAsyncTest : public QObject {
  Q_OBJECT

 private slots:
  void opensBeforeBackgroundSummaryCompletes();
  void removalRunsAsynchronously();
};

void IndexManagerAsyncTest::opensBeforeBackgroundSummaryCompletes() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString databasePath = temporary.filePath(QStringLiteral("index.db"));
  qputenv("WENSOUSOU_DB", databasePath.toUtf8());
  qputenv("WENSOUSOU_SIMPLE_LIB", QByteArrayLiteral(WENSOUSOU_TEST_SIMPLE_LIB));

  Database database;
  QString error;
  QVERIFY2(database.open(databasePath, QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB),
                         &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));

  QElapsedTimer timer;
  timer.start();
  IndexManagerDialog dialog(&database);
  QVERIFY2(timer.elapsed() < 500, "Index manager construction blocked the UI thread.");
  dialog.show();
  QTest::qWait(100);
}

void IndexManagerAsyncTest::removalRunsAsynchronously() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString databasePath = temporary.filePath(QStringLiteral("index.db"));
  qputenv("WENSOUSOU_DB", databasePath.toUtf8());
  qputenv("WENSOUSOU_SIMPLE_LIB", QByteArrayLiteral(WENSOUSOU_TEST_SIMPLE_LIB));

  Database database;
  QString error;
  QVERIFY2(database.open(databasePath, QStringLiteral(WENSOUSOU_TEST_SIMPLE_LIB),
                         &error),
           qPrintable(error));
  QVERIFY2(database.addRoot(temporary.path(), &error), qPrintable(error));
  const RootRecord root = database.roots(&error).first();
  const qint64 scanId = database.beginScan(root.id, &error);
  QVERIFY(scanId > 0);
  for (int index = 0; index < 50; ++index) {
    QVERIFY2(database.upsertDocument(
                 root.id,
                 temporary.filePath(QStringLiteral("doc-%1.txt").arg(index)),
                 100, 1000 + index, QStringLiteral("党费 文搜搜"), scanId, &error),
             qPrintable(error));
  }
  QVERIFY2(database.finishScan(scanId, root.id, true, QString(), &error),
           qPrintable(error));

  IndexManagerDialog dialog(&database);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  auto* table = dialog.findChild<QTableWidget*>();
  QVERIFY(table);
  QTRY_VERIFY(table->rowCount() == 1);
  QTRY_VERIFY(table->item(0, 0) != nullptr);
  QVERIFY(table->item(0, 0)->data(Qt::UserRole).toLongLong() > 0);
  table->selectRow(0);
  table->setCurrentItem(table->item(0, 0));
  QSignalSpy removalRequested(&dialog, SIGNAL(rootRemovalRequested(qint64,QString)));
  QSignalSpy rootsChanged(&dialog, SIGNAL(rootsChanged()));

  QElapsedTimer timer;
  timer.start();
  dialog.startRootRemovalForTest(
      table->item(0, 0)->data(Qt::UserRole).toLongLong(),
      table->item(0, 0)->text());
  QTest::qWait(50);
  QVERIFY2(timer.elapsed() < 500, "removeSelectedRoot blocked the UI thread.");
  QTRY_COMPARE(removalRequested.count(), 1);
  QTRY_COMPARE(rootsChanged.count(), 1);
  QCOMPARE(database.roots(&error).size(), 0);
}

}  // namespace wensousou

QTEST_MAIN(wensousou::IndexManagerAsyncTest)
#include "index_manager_async_test.moc"
