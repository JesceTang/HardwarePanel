#include "apps/hwpanel-client/backend/notification_model.h"

#include <QDateTime>

namespace hwpanel::client {

NotificationModel::NotificationModel(QObject* parent) : QAbstractListModel(parent) {}

int NotificationModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant NotificationModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
    return {};
  }
  const Row& row = rows_.at(index.row());
  switch (role) {
    case TimeRole:
      return row.time;
    case LevelRole:
      return row.level;
    case TitleRole:
      return row.title;
    case MessageRole:
      return row.message;
    default:
      return {};
  }
}

QHash<int, QByteArray> NotificationModel::roleNames() const {
  return {
      {TimeRole, "time"},
      {LevelRole, "level"},
      {TitleRole, "title"},
      {MessageRole, "message"},
  };
}

void NotificationModel::Append(const core::EventMessage& event) {
  Row row;
  row.time = QDateTime::fromMSecsSinceEpoch(event.timestamp_ms).toString("HH:mm:ss");
  row.level = QString::fromStdString(event.level);
  row.title = QString::fromStdString(event.title);
  row.message = QString::fromStdString(event.message);

  beginInsertRows(QModelIndex(), 0, 0);
  rows_.push_front(row);
  endInsertRows();
  if (rows_.size() > 50) {
    beginRemoveRows(QModelIndex(), rows_.size() - 1, rows_.size() - 1);
    rows_.removeLast();
    endRemoveRows();
  }
  emit countChanged();
}

void NotificationModel::Clear() {
  beginResetModel();
  rows_.clear();
  endResetModel();
  emit countChanged();
}

}  // namespace hwpanel::client
