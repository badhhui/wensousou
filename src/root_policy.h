#pragma once

#include <QString>
#include <QStringList>

namespace wensousou {

class RootPolicy {
 public:
  static QStringList availableExtensions();
  static QStringList defaultExtensions();
  static QStringList enabledExtensions();
  static QString normalize(const QString& path);
  static bool overlaps(const QString& left, const QString& right);
  static bool canAdd(const QString& candidate, const QStringList& existing,
                     QString* error);
  static bool isSupportedDocument(const QString& path);
  static bool shouldSkipPath(const QString& path, const QString& root);
};

}  // namespace wensousou
