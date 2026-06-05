#include "logging.h"

#include "app_paths.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include <cstdlib>

namespace wensousou {
namespace {

QFile* logFile = nullptr;
QMutex logMutex;

void writeMessage(QtMsgType type, const QMessageLogContext&, const QString& message) {
  QMutexLocker locker(&logMutex);
  if (!logFile || !logFile->isOpen()) return;
  const char* level = "INFO";
  if (type == QtWarningMsg) level = "WARN";
  if (type == QtCriticalMsg || type == QtFatalMsg) level = "ERROR";
  if (type == QtDebugMsg) level = "DEBUG";
  QTextStream stream(logFile);
  stream << QDateTime::currentDateTime().toString(Qt::ISODate)
         << " [" << level << "] " << message << '\n';
  stream.flush();
  if (type == QtFatalMsg) abort();
}

}  // namespace

void initializeLogging() {
  if (logFile) return;
  logFile = new QFile(AppPaths::stateDirectory() + QStringLiteral("/wensousou.log"));
  if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    qInstallMessageHandler(writeMessage);
  }
}

}  // namespace wensousou
