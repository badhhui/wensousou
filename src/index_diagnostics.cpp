#include "index_diagnostics.h"

#include "app_paths.h"
#include "database.h"

#include <QFileInfo>
#include <QStringList>

namespace wensousou {

bool runIndexDiagnostics(const QString& query, QString* report) {
  QStringList lines;
  lines.append(QStringLiteral("数据库：%1").arg(AppPaths::databasePath()));
  lines.append(QStringLiteral("数据库文件：%1")
                   .arg(QFileInfo::exists(AppPaths::databasePath())
                            ? QStringLiteral("存在")
                            : QStringLiteral("不存在")));

  Database database;
  QString error;
  if (!database.open(AppPaths::databasePath(), AppPaths::simpleLibraryPath(), &error)) {
    *report = lines.join(QLatin1Char('\n')) +
              QStringLiteral("\n打开数据库失败：%1").arg(error);
    return false;
  }

  const IndexDiagnostics diagnostics = database.diagnostics(&error);
  if (!error.isEmpty()) {
    *report = lines.join(QLatin1Char('\n')) +
              QStringLiteral("\n读取索引状态失败：%1").arg(error);
    return false;
  }
  lines.append(QStringLiteral("索引目录：%1").arg(diagnostics.rootCount));
  lines.append(QStringLiteral("文档总数：%1").arg(diagnostics.documentCount));
  lines.append(QStringLiteral("成功文档：%1").arg(diagnostics.okCount));
  lines.append(QStringLiteral("失败文档：%1").arg(diagnostics.failedCount));
  lines.append(QStringLiteral("空正文文档：%1").arg(diagnostics.emptyContentCount));
  lines.append(QStringLiteral("FTS 行数：%1").arg(diagnostics.ftsRowCount));
  if (!diagnostics.recentFailures.isEmpty()) {
    lines.append(QStringLiteral("最近失败："));
    for (const QString& failure : diagnostics.recentFailures) {
      lines.append(QStringLiteral("- %1").arg(failure));
    }
  }

  if (!query.trimmed().isEmpty()) {
    const QList<SearchResult> results =
        database.search(query, 0, QString(), 0, SearchSort::Relevance, 20, 0, &error);
    if (!error.isEmpty()) {
      lines.append(QStringLiteral("诊断搜索失败：%1").arg(error));
      *report = lines.join(QLatin1Char('\n'));
      return false;
    }
    lines.append(QStringLiteral("诊断关键词：%1").arg(query));
    lines.append(QStringLiteral("命中数量：%1").arg(results.size()));
    for (const SearchResult& result : results) {
      lines.append(QStringLiteral("- %1").arg(result.path));
    }
  }

  *report = lines.join(QLatin1Char('\n'));
  return true;
}

}  // namespace wensousou
