#include "index_diagnostics.h"
#include "logging.h"
#include "qml_app_controller.h"
#include "self_check.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QSettings>
#include <QTextCodec>
#include <QTextStream>
#include <QTimer>

#include <clocale>

namespace {

QSize defaultWindowSize(const QRect& availableGeometry) {
  const int availableWidth = qMax(800, availableGeometry.width() - 40);
  const int availableHeight = qMax(600, availableGeometry.height() - 40);
  const int width = qMin(availableWidth,
                         qMin(1600, qMax(1024, int(availableGeometry.width() * 0.86))));
  const int height = qMin(availableHeight,
                          qMin(1000, qMax(680, int(availableGeometry.height() * 0.86))));
  return QSize(width, height);
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("LANG", QByteArrayLiteral("C.UTF-8"));
  qputenv("LC_ALL", QByteArrayLiteral("C.UTF-8"));
  std::setlocale(LC_ALL, "C.UTF-8");
  QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

  QCoreApplication::setOrganizationName(QStringLiteral("WenSouSou"));
  QCoreApplication::setApplicationName(QStringLiteral("wensousou"));
  QCoreApplication::setApplicationVersion(QStringLiteral("1.1.1"));
  wensousou::initializeLogging();

  bool selfCheck = false;
  bool diagnoseIndex = false;
  QString diagnoseQuery;
  for (int index = 1; index < argc; ++index) {
    const QString argument = QString::fromLocal8Bit(argv[index]);
    if (argument == QStringLiteral("--self-check")) {
      selfCheck = true;
    } else if (argument == QStringLiteral("--diagnose-index")) {
      diagnoseIndex = true;
      if (index + 1 < argc) diagnoseQuery = QString::fromLocal8Bit(argv[++index]);
    }
  }
  if (selfCheck) {
    QCoreApplication app(argc, argv);
    QString report;
    const bool ok = wensousou::runSelfCheck(&report);
    QTextStream stream(ok ? stdout : stderr);
    stream << report << Qt::endl;
    return ok ? 0 : 1;
  }
  if (diagnoseIndex) {
    QCoreApplication app(argc, argv);
    QString report;
    const bool ok = wensousou::runIndexDiagnostics(diagnoseQuery, &report);
    QTextStream stream(ok ? stdout : stderr);
    stream << report << Qt::endl;
    return ok ? 0 : 1;
  }

  qputenv("QT_QUICK_BACKEND", QByteArrayLiteral("software"));
  QApplication app(argc, argv);
  QFont appFont = app.font();
  const double fontScale = qBound(
      0.85, QSettings().value(QStringLiteral("ui/fontScale"), 1.0).toDouble(), 1.40);
  appFont.setPointSizeF(appFont.pointSizeF() * fontScale);
  app.setFont(appFont);
  qmlRegisterType<wensousou::SearchResultsModel>("WenSouSou", 1, 0, "SearchResultsModel");
  qmlRegisterType<wensousou::RootsModel>("WenSouSou", 1, 0, "RootsModel");

  wensousou::QmlAppController controller;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
  engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
  if (engine.rootObjects().isEmpty()) return 1;
  qInfo("QML main window shown.");
  return app.exec();
}
