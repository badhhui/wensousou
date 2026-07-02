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
  QCoreApplication::setApplicationVersion(QStringLiteral("1.1.2"));
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
  const double fontScale = qBound(
      0.85, QSettings().value(QStringLiteral("ui/fontScale"), 1.0).toDouble(), 1.40);
  appFont.setPointSizeF(appFont.pointSizeF() * fontScale);
  app.setFont(appFont);
  app.setStyleSheet(QStringLiteral(
      "QMainWindow, QDialog, QWidget#appShell { background: #f5f8ff; color: #172033; }"
      "QFrame#brandMark { background: #3b82f6; border-radius: 9px; }"
      "QLabel#brandGlyph { color: white; font-size: 25px; font-weight: 700; }"
      "QLabel#brandTitle { color: #172033; font-size: 26px; font-weight: 700; }"
      "QLabel#muted, QLabel#resultHint { color: #64748b; }"
      "QLabel#sectionLabel { color: #64748b; font-size: 14px; font-weight: 700; }"
      "QLabel#resultTitle { color: #111827; font-size: 21px; font-weight: 700; }"
      "QLabel#panelTitle { color: #111827; font-size: 24px; font-weight: 700; }"
      "QLabel#statusPill { background: rgba(255,255,255,0.72); color: #475569;"
      "  border: 1px solid #e2e8f0; border-radius: 8px; padding: 7px 11px; }"
      "QLabel#summaryPill { background: transparent; color: #334155;"
      "  border: 0; padding: 2px 4px; }"
      "QFrame#searchPanel, QFrame#summaryBar { background: #ffffff;"
      "  border: 1px solid #e6eaf0; border-radius: 10px; }"
      "QFrame#filterBar { background: #f8fbff; border: 1px solid #edf2f7;"
      "  border-radius: 8px; }"
      "QLineEdit, QComboBox, QTableWidget, QTextBrowser, QSpinBox {"
      "  background: #ffffff; border: 1px solid #d9e1ea; border-radius: 7px; }"
      "QLineEdit { padding: 10px 12px; selection-background-color: #bfdbfe; }"
      "QLineEdit#heroSearch { border: 1px solid #60a5fa; border-radius: 8px;"
      "  padding: 14px 15px; background: #ffffff; font-size: 19px; }"
      "QLineEdit#heroSearch:focus { border: 2px solid #2563eb; padding: 13px 14px; }"
      "QComboBox { padding: 8px 12px; min-width: 128px; }"
      "QComboBox#filterCombo, QPushButton#filterButton { background: #ffffff;"
      "  border-color: #dbe4ee; color: #334155; }"
      "QComboBox QAbstractItemView, QMenu { background: #ffffff; color: #1f2937;"
      "  selection-background-color: #dbeafe; selection-color: #0f172a;"
      "  border: 1px solid #d9e1ea; }"
      "QComboBox QAbstractItemView::item, QMenu::item { color: #1f2937;"
      "  padding: 8px 18px; }"
      "QComboBox QAbstractItemView::item:hover, QMenu::item:selected {"
      "  background: #dbeafe; color: #0f172a; }"
      "QPushButton { background: #ffffff; color: #334155; border: 1px solid #d9e1ea;"
      "  border-radius: 7px; padding: 8px 13px; }"
      "QPushButton:hover { color: #2563eb; border-color: #93c5fd; background: #eff6ff; }"
      "QPushButton#primaryButton { background: #3b82f6; color: #ffffff; border-color: #3b82f6;"
      "  font-weight: 700; padding-left: 18px; padding-right: 18px; }"
      "QPushButton#primaryButton:hover { background: #2563eb; border-color: #2563eb; }"
      "QPushButton#quietButton { background: transparent; border-color: #dbe4ee; color: #475569; }"
      "QPushButton#activePageButton { background: #dbeafe; border-color: #bfdbfe;"
      "  color: #1d4ed8; font-weight: 700; }"
      "QPushButton#activePageButton:disabled { background: #dbeafe; border-color: #bfdbfe;"
      "  color: #1d4ed8; }"
      "QPushButton#dangerButton { background: #fff7f8; color: #be123c; border-color: #fecdd3; }"
      "QPushButton#dangerButton:hover { background: #fff1f2; border-color: #fda4af; color: #9f1239; }"
      "QPushButton:disabled { color: #94a3b8; background: #f1f5f9; border-color: #e2e8f0; }"
      "QHeaderView::section { background: #f8fafc; color: #475569;"
      "  padding: 10px; border: 0; border-bottom: 1px solid #e2e8f0; font-weight: 700; }"
      "QTableWidget { color: #1f2937; gridline-color: #eef2f7;"
      "  alternate-background-color: #f9fbfd; selection-background-color: #dbeafe;"
      "  selection-color: #1d4ed8; }"
      "QTableWidget::item { padding: 4px; }"
      "QTableWidget::item:selected { background: #dbeafe; color: #1d4ed8; }"
      "QToolButton { background: #ffffff; border: 1px solid #dbe4ee;"
      "  border-radius: 6px; padding: 5px; color: #334155; }"
      "QToolButton:hover { background: #eff6ff; border-color: #93c5fd; }"
      "QToolButton:disabled { color: #94a3b8; background: #f8fafc; border-color: #e2e8f0; }"
      "QCheckBox { color: #334155; spacing: 7px; }"
      "QProgressBar { background: #e2e8f0; border: 0; border-radius: 4px; min-height: 8px; }"
      "QProgressBar::chunk { background: #3b82f6; border-radius: 4px; }"
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
