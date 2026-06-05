#pragma once

#include "database.h"

#include <QAbstractListModel>
#include <QList>

namespace wensousou {

class SearchResultsModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    FilenameRole,
    FilenameHtmlRole,
    SnippetHtmlRole,
    SizeRole,
    ModifiedRole,
    PathRole,
    ExtensionRole
  };

  explicit SearchResultsModel(QObject* parent = nullptr);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  void setResults(const QList<SearchResult>& results);
  SearchResult resultAt(int row) const;

 private:
  QList<SearchResult> results_;
};

class RootsModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    NameRole,
    PathRole,
    StatusRole
  };

  explicit RootsModel(QObject* parent = nullptr);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  void setRoots(const QList<RootRecord>& roots);
  Q_INVOKABLE qint64 idAt(int row) const;
  QStringList paths() const;

 private:
  QList<RootRecord> roots_;
};

}  // namespace wensousou
