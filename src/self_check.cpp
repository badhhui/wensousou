#include "self_check.h"

#include "app_paths.h"
#include "database.h"
#include "tika_worker.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>

namespace wensousou {

bool runSelfCheck(QString* report) {
  QStringList messages;
  QTemporaryDir temporary;
  if (!temporary.isValid()) {
    *report = QStringLiteral("失败：无法创建临时目录。");
    return false;
  }

  const QString sourcePath = temporary.filePath(QStringLiteral("中华文档.txt"));
  QFile source(sourcePath);
  if (!source.open(QIODevice::WriteOnly) ||
      source.write(QStringLiteral("中华人民共和国文档，支持中文全文检索。")
                       .toUtf8()) < 0) {
    *report = QStringLiteral("失败：无法写入临时测试文档。");
    return false;
  }
  source.close();

  Database database;
  QString error;
  if (!database.open(temporary.filePath(QStringLiteral("self-check.db")),
                     AppPaths::simpleLibraryPath(), &error)) {
    *report = QStringLiteral("失败：%1").arg(error);
    return false;
  }
  messages.append(QStringLiteral("SQLite FTS5 与 libsimple.so 加载成功"));
  if (!database.addRoot(temporary.path(), &error)) {
    *report = QStringLiteral("失败：%1").arg(error);
    return false;
  }
  const RootRecord root = database.roots().first();
  const qint64 scanId = database.beginScan(root.id, &error);
  if (scanId == 0 ||
      !database.upsertDocument(root.id, sourcePath, QFileInfo(sourcePath).size(),
                               QFileInfo(sourcePath).lastModified().toMSecsSinceEpoch(),
                               QStringLiteral("中华人民共和国文档，支持中文全文检索。"),
                               scanId, &error) ||
      !database.finishScan(scanId, root.id, true, QString(), &error)) {
    *report = QStringLiteral("失败：%1").arg(error);
    return false;
  }
  if (database.search(QStringLiteral("中华"), 0, QString(), 0, SearchSort::Relevance,
                      10, 0, &error).isEmpty()) {
    *report = QStringLiteral("失败：中文搜索未命中。%1").arg(error);
    return false;
  }
  if (!database.search(QStringLiteral("zhonghua"), 0, QString(), 0, SearchSort::Relevance,
                       10, 0, &error)
           .isEmpty()) {
    *report = QStringLiteral("失败：拼音搜索仍处于启用状态。%1").arg(error);
    return false;
  }
  const QString preview = database.previewDocument(
      database.search(QStringLiteral("中华"), 0, QString(), 0, SearchSort::Relevance,
                      10, 0, &error).first().id,
      QStringLiteral("中华"), &error);
  if (!preview.contains(QStringLiteral("[[["))) {
    *report = QStringLiteral("失败：高亮自检未通过。%1").arg(error);
    return false;
  }
  messages.append(QStringLiteral("中文搜索、高亮与非拼音模式成功"));

  TikaWorker parser;
  const ParseResult parsed = parser.parse(sourcePath, 10000, 30);
  if (!parsed.ok || !parsed.content.contains(QStringLiteral("中华人民共和国"))) {
    *report = QStringLiteral("失败：Tika Worker 自检未通过：%1").arg(parsed.error);
    return false;
  }
  messages.append(QStringLiteral("Tika Worker 文本抽取成功"));
  *report = QStringLiteral("文搜搜自检通过：\n- ") + messages.join(QStringLiteral("\n- "));
  return true;
}

}  // namespace wensousou
