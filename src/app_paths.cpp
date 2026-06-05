#include "app_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace wensousou {
namespace {

QString fromEnvironment(const char* key) {
  return QString::fromLocal8Bit(qgetenv(key));
}

QString defaultInstallRoot() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString adjacentRoot = QDir(appDir).absoluteFilePath(QStringLiteral(".."));
  if (QFileInfo::exists(QDir(adjacentRoot).filePath(QStringLiteral("parser/parser-worker.jar")))) {
    return QDir(adjacentRoot).absolutePath();
  }
  return QStringLiteral("/opt/wensousou");
}

QString ensureDirectory(const QString& path) {
  QDir().mkpath(path);
  return path;
}

}  // namespace

QString AppPaths::installRoot() {
  const QString overridden = fromEnvironment("WENSOUSOU_HOME");
  return overridden.isEmpty() ? defaultInstallRoot() : QDir(overridden).absolutePath();
}

QString AppPaths::databasePath() {
  const QString overridden = fromEnvironment("WENSOUSOU_DB");
  if (!overridden.isEmpty()) {
    return overridden;
  }
  const QString data = ensureDirectory(
      QDir::home().filePath(QStringLiteral(".local/share/wensousou")));
  return QDir(data).filePath(QStringLiteral("index.db"));
}

QString AppPaths::stateDirectory() {
  const QString overridden = fromEnvironment("WENSOUSOU_STATE_DIR");
  if (!overridden.isEmpty()) {
    return ensureDirectory(overridden);
  }
  return ensureDirectory(
      QDir::home().filePath(QStringLiteral(".local/state/wensousou")));
}

QString AppPaths::simpleLibraryPath() {
  const QString overridden = fromEnvironment("WENSOUSOU_SIMPLE_LIB");
  return overridden.isEmpty()
             ? QDir(installRoot()).filePath(QStringLiteral("lib/libsimple.so"))
             : overridden;
}

QString AppPaths::javaExecutable() {
  const QString overridden = fromEnvironment("WENSOUSOU_JAVA");
  return overridden.isEmpty()
             ? QDir(installRoot()).filePath(QStringLiteral("runtime/jre/bin/java"))
             : overridden;
}

QString AppPaths::parserWorkerJar() {
  const QString overridden = fromEnvironment("WENSOUSOU_PARSER_JAR");
  return overridden.isEmpty()
             ? QDir(installRoot()).filePath(QStringLiteral("parser/parser-worker.jar"))
             : overridden;
}

}  // namespace wensousou
