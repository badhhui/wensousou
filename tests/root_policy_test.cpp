#include "root_policy.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using wensousou::RootPolicy;

class RootPolicyTest : public QObject {
  Q_OBJECT

 private slots:
  void rejectsOverlappingRoots() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString parent = temporary.path();
    const QString child = QDir(parent).filePath(QStringLiteral("child"));
    QVERIFY(QDir().mkpath(child));
    QString error;
    QVERIFY(!RootPolicy::canAdd(child, {parent}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(RootPolicy::overlaps(parent, child));
  }

  void supportsRequestedExtensions() {
    QVERIFY(RootPolicy::isSupportedDocument(QStringLiteral("/tmp/report.wps")));
    QVERIFY(RootPolicy::isSupportedDocument(QStringLiteral("/tmp/table.ET")));
    QVERIFY(RootPolicy::isSupportedDocument(QStringLiteral("/tmp/slides.pptx")));
    QVERIFY(!RootPolicy::isSupportedDocument(QStringLiteral("/tmp/archive.zip")));
  }

  void skipsHiddenAndTemporaryPaths() {
    QVERIFY(RootPolicy::shouldSkipPath(QStringLiteral("/tmp/root/.cache/a.doc"),
                                       QStringLiteral("/tmp/root")));
    QVERIFY(RootPolicy::shouldSkipPath(QStringLiteral("/tmp/root/~$report.docx"),
                                       QStringLiteral("/tmp/root")));
    QVERIFY(!RootPolicy::shouldSkipPath(QStringLiteral("/tmp/root/report.docx"),
                                        QStringLiteral("/tmp/root")));
  }
};

QTEST_GUILESS_MAIN(RootPolicyTest)
#include "root_policy_test.moc"

