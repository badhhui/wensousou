#include "search_worker.h"

#include <QMetaType>
#include <QMetaObject>
#include <QSignalSpy>
#include <QtTest>

namespace wensousou {

class SearchMetaTypeTest : public QObject {
  Q_OBJECT

 private slots:
  void registersMocSignalTypeName();
  void invokesSearchWithSortMode();
};

void SearchMetaTypeTest::registersMocSignalTypeName() {
  registerSearchMetaTypes();
  QVERIFY(QMetaType::type("QList<SearchResult>") != QMetaType::UnknownType);
  QVERIFY(QMetaType::type("SearchSort") != QMetaType::UnknownType);
}

void SearchMetaTypeTest::invokesSearchWithSortMode() {
  registerSearchMetaTypes();
  SearchWorker worker;
  QSignalSpy finished(&worker, &SearchWorker::finished);
  worker.cancelBefore(1);
  QVERIFY(QMetaObject::invokeMethod(
      &worker, "search", Qt::DirectConnection,
      Q_ARG(qint64, 1), Q_ARG(QString, QStringLiteral("党费")),
      Q_ARG(qint64, 0), Q_ARG(QStringList, QStringList()),
      Q_ARG(qint64, 0), Q_ARG(SearchSort, SearchSort::Relevance),
      Q_ARG(int, 20), Q_ARG(int, 0), Q_ARG(bool, false),
      Q_ARG(int, 0x3)));
  QCOMPARE(finished.count(), 1);
}

}  // namespace wensousou

QTEST_MAIN(wensousou::SearchMetaTypeTest)
#include "search_meta_type_test.moc"
