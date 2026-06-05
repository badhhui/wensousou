#include "qml_models.h"

#include <QDateTime>
#include <QFileInfo>

namespace wensousou {
namespace {

QString formatTime(qint64 timestampMs) {
  return timestampMs > 0
             ? QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
             : QString();
}

QString formatFileSize(qint64 bytes) {
  if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
  if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  if (bytes < 1024LL * 1024 * 1024) {
    return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
  }
  return QStringLiteral("%1 GB").arg(bytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 1);
}

QString highlightedHtml(QString text) {
  text = text.toHtmlEscaped();
  text.replace(QStringLiteral("【"),
               QStringLiteral("<span style=\"background-color:#fff19a;"
                              "color:#172033;border-radius:3px;\">"));
  text.replace(QStringLiteral("】"), QStringLiteral("</span>"));
  return text;
}

}  // namespace

SearchResultsModel::SearchResultsModel(QObject* parent) : QAbstractListModel(parent) {}

int SearchResultsModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : results_.size();
}

QVariant SearchResultsModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= results_.size()) return {};
  const SearchResult& result = results_.at(index.row());
  switch (role) {
    case IdRole:
      return result.id;
    case FilenameRole:
      return result.filename;
    case FilenameHtmlRole:
      return highlightedHtml(result.highlightedFilename.isEmpty()
                                 ? result.filename
                                 : result.highlightedFilename);
    case SnippetHtmlRole:
      return highlightedHtml(result.snippet);
    case SizeRole:
      return formatFileSize(result.size);
    case ModifiedRole:
      return formatTime(result.mtimeMs);
    case PathRole:
      return result.path;
    case ExtensionRole:
      return result.extension.toUpper();
    default:
      return {};
  }
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const {
  return {{IdRole, "documentId"},
          {FilenameRole, "filename"},
          {FilenameHtmlRole, "filenameHtml"},
          {SnippetHtmlRole, "snippetHtml"},
          {SizeRole, "sizeText"},
          {ModifiedRole, "modifiedText"},
          {PathRole, "path"},
          {ExtensionRole, "extension"}};
}

void SearchResultsModel::setResults(const QList<SearchResult>& results) {
  beginResetModel();
  results_ = results;
  endResetModel();
}

SearchResult SearchResultsModel::resultAt(int row) const {
  return row >= 0 && row < results_.size() ? results_.at(row) : SearchResult{};
}

RootsModel::RootsModel(QObject* parent) : QAbstractListModel(parent) {}

int RootsModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : roots_.size();
}

QVariant RootsModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= roots_.size()) return {};
  const RootRecord& root = roots_.at(index.row());
  switch (role) {
    case IdRole:
      return root.id;
    case NameRole: {
      const QString name = QFileInfo(root.path).fileName();
      return name.isEmpty() ? root.path : name;
    }
    case PathRole:
      return root.path;
    case StatusRole:
      return root.lastScanStatus == QStringLiteral("ok")
                 ? QStringLiteral("正常")
                 : root.lastScanStatus.isEmpty() ? QStringLiteral("待更新")
                                                 : root.lastScanStatus;
    default:
      return {};
  }
}

QHash<int, QByteArray> RootsModel::roleNames() const {
  return {{IdRole, "rootId"},
          {NameRole, "name"},
          {PathRole, "path"},
          {StatusRole, "status"}};
}

void RootsModel::setRoots(const QList<RootRecord>& roots) {
  beginResetModel();
  roots_ = roots;
  endResetModel();
}

qint64 RootsModel::idAt(int row) const {
  return row >= 0 && row < roots_.size() ? roots_.at(row).id : 0;
}

QStringList RootsModel::paths() const {
  QStringList values;
  for (const RootRecord& root : roots_) values.append(root.path);
  return values;
}

}  // namespace wensousou
