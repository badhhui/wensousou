#pragma once

#include <QString>

namespace wensousou {

class AppPaths {
 public:
  static QString installRoot();
  static QString databasePath();
  static QString stateDirectory();
  static QString simpleLibraryPath();
  static QString javaExecutable();
  static QString parserWorkerJar();
};

}  // namespace wensousou

