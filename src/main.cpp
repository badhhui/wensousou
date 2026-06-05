#include "main_window.h"
#include "index_diagnostics.h"
#include "logging.h"
#include "self_check.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QRect>
#include <QScreen>
#include <QSize>
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
  QCoreApplication::setApplicationVersion(QStringLiteral("1.0.18"));
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

  QApplication app(argc, argv);
  QFont appFont = app.font();
  appFont.setPointSizeF(appFont.pointSizeF() * 1.3);
  app.setFont(appFont);
  app.setStyleSheet(QStringLiteral(
      "QMainWindow, QWidget#appShell { background: #f7f8fa; color: #1f2937; }"
      "QFrame#brandMark { background: #0f766e; border-radius: 10px; }"
      "QLabel#brandGlyph { color: white; font-size: 30px; font-weight: 700; }"
      "QLabel#brandTitle { color: #1f2937; font-size: 31px; font-weight: 700; }"
      "QLabel#muted, QLabel#resultHint { color: #64748b; }"
      "QLabel#sectionLabel { color: #64748b; font-size: 16px; font-weight: 700; }"
      "QLabel#resultTitle { color: #111827; font-size: 22px; font-weight: 700; }"
      "QLabel#statusPill { background: rgba(255,255,255,0.88); color: #374151;"
      "  border: 1px solid #e5e7eb; border-radius: 10px; padding: 10px 14px; }"
      "QFrame#controlCard, QFrame#searchPanel { background: rgba(255,255,255,0.98);"
      "  border: 1px solid #cbd5e1; border-radius: 10px; }"
      "QLineEdit, QComboBox, QTableWidget, QTextBrowser {"
      "  background: white; border: 1px solid #dcdfe6; border-radius: 7px; }"
      "QLineEdit { padding: 12px; }"
      "QLineEdit#heroSearch { border: 2px solid #0f766e; border-radius: 7px;"
      "  padding: 12px; background: white; }"
      "QComboBox { padding: 9px 12px; min-width: 125px; }"
      "QComboBox QAbstractItemView, QMenu { background: white; color: #1f2937;"
      "  selection-background-color: #ccfbf1; selection-color: #111827;"
      "  border: 1px solid #dcdfe6; }"
      "QComboBox QAbstractItemView::item, QMenu::item { color: #1f2937;"
      "  padding: 8px 18px; }"
      "QComboBox QAbstractItemView::item:hover, QMenu::item:selected {"
      "  background: #ccfbf1; color: #111827; }"
      "QPushButton { background: white; color: #374151; border: 1px solid #dcdfe6;"
      "  border-radius: 7px; padding: 9px 14px; }"
      "QPushButton:hover { color: #0f766e; border-color: #5eead4; background: #f0fdfa; }"
      "QPushButton#primaryButton { background: #0f766e; color: white; border-color: #0f766e; }"
      "QPushButton#primaryButton:hover { background: #115e59; border-color: #115e59; }"
      "QPushButton#dangerButton { color: #be123c; }"
      "QPushButton:disabled { color: #94a3b8; background: #f1f5f9; border-color: #e2e8f0; }"
      "QHeaderView::section { background: #f1f5f9; color: #334155;"
      "  padding: 9px; border: 0; border-bottom: 1px solid #e2e8f0; }"
      "QTableWidget { gridline-color: #edf0f5; alternate-background-color: #fbf7ef; }"
      "QTableWidget::item:selected { background: #ccfbf1; color: #134e4a; }"
      "QToolButton { background: white; border: 1px solid #dcdfe6;"
      "  border-radius: 5px; padding: 5px; }"
      "QToolButton:hover { background: #ecfeff; border-color: #5eead4; }"
      "QProgressBar { background: #e2e8f0; border: 0; border-radius: 4px; min-height: 8px; }"
      "QProgressBar::chunk { background: #0f766e; border-radius: 4px; }"
      "QStatusBar { background: #ffffff; color: #64748b; border-top: 1px solid #e5e7eb; }"));
  wensousou::MainWindow window;
  if (!window.ready()) return 1;
  if (QScreen* screen = app.primaryScreen()) {
    const QRect available = screen->availableGeometry();
    window.resize(defaultWindowSize(available));
    window.move(available.center() - window.rect().center());
  }
  window.show();
  qInfo("Main window shown.");
  QTimer::singleShot(0, &window, [&window]() {
    qInfo("Main window geometry: %dx%d; central widget: %s",
          window.width(), window.height(), window.centralWidget() ? "yes" : "no");
  });
  return app.exec();
}
