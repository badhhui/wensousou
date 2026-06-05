#include "root_policy.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace wensousou {
namespace {

bool hasPathPrefix(const QString& child, const QString& parent) {
  if (child == parent) {
    return true;
  }
  return child.startsWith(parent + QDir::separator());
}

}  // namespace

QStringList RootPolicy::availableExtensions() {
  return {QStringLiteral("doc"),  QStringLiteral("docx"), QStringLiteral("wps"),
          QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("et"),
          QStringLiteral("ppt"),  QStringLiteral("pptx"), QStringLiteral("txt"),
          QStringLiteral("pdf")};
}

QStringList RootPolicy::defaultExtensions() {
  return {QStringLiteral("doc"),  QStringLiteral("docx"), QStringLiteral("wps"),
          QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("et"),
          QStringLiteral("ppt"),  QStringLiteral("pptx"), QStringLiteral("pdf")};
}

QStringList RootPolicy::enabledExtensions() {
  const QStringList available = availableExtensions();
  QStringList configured =
      QSettings().value(QStringLiteral("index/enabledExtensions"),
                        defaultExtensions()).toStringList();
  QStringList enabled;
  for (QString extension : configured) {
    extension = extension.trimmed().toLower();
    if (available.contains(extension) && !enabled.contains(extension)) {
      enabled.append(extension);
    }
  }
  if (enabled.isEmpty()) return defaultExtensions();
  return enabled;
}

QString RootPolicy::normalize(const QString& path) {
  QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

bool RootPolicy::overlaps(const QString& left, const QString& right) {
  const QString normalizedLeft = normalize(left);
  const QString normalizedRight = normalize(right);
  return hasPathPrefix(normalizedLeft, normalizedRight) ||
         hasPathPrefix(normalizedRight, normalizedLeft);
}

bool RootPolicy::canAdd(const QString& candidate, const QStringList& existing,
                        QString* error) {
  QFileInfo info(candidate);
  if (!info.exists() || !info.isDir()) {
    if (error) {
      *error = QStringLiteral("目录不存在或不是文件夹。");
    }
    return false;
  }
  const QString normalized = normalize(candidate);
  for (const QString& root : existing) {
    if (overlaps(normalized, root)) {
      if (error) {
        *error = QStringLiteral("所选目录与已有目录重叠：%1").arg(root);
      }
      return false;
    }
  }
  return true;
}

bool RootPolicy::isSupportedDocument(const QString& path) {
  return enabledExtensions().contains(QFileInfo(path).suffix().toLower());
}

bool RootPolicy::shouldSkipPath(const QString& path, const QString& root) {
  const QString normalized = QDir::fromNativeSeparators(QDir::cleanPath(path));
  const QString relative =
      QDir::fromNativeSeparators(QDir(normalize(root)).relativeFilePath(normalized));
  const QStringList parts = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString& part : parts) {
    if (part.startsWith(QLatin1Char('.')) ||
        part.compare(QStringLiteral("$RECYCLE.BIN"), Qt::CaseInsensitive) == 0 ||
        part.compare(QStringLiteral("System Volume Information"), Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  const QString name = QFileInfo(path).fileName();
  return name.startsWith(QStringLiteral("~$")) ||
         name.startsWith(QStringLiteral(".~")) ||
         name.endsWith(QStringLiteral(".tmp"), Qt::CaseInsensitive);
}

}  // namespace wensousou
