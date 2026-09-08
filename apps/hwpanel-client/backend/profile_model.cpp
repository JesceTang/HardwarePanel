#include "apps/hwpanel-client/backend/profile_model.h"

#include <QVariantMap>

namespace hwpanel::client {

ProfileModel::ProfileModel(QObject* parent) : QAbstractListModel(parent) {}

int ProfileModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant ProfileModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
    return {};
  }
  const Row& row = rows_.at(index.row());
  switch (role) {
    case IdRole:
      return row.id;
    case NameRole:
      return row.name;
    case DescriptionRole:
      return row.description;
    case ActiveRole:
      return row.id == active_id_;
    default:
      return {};
  }
}

QHash<int, QByteArray> ProfileModel::roleNames() const {
  return {
      {IdRole, "profileId"},
      {NameRole, "profileName"},
      {DescriptionRole, "profileDescription"},
      {ActiveRole, "isActive"},
  };
}

void ProfileModel::SetProfiles(const QVariantList& profiles, const QString& active_id) {
  beginResetModel();
  rows_.clear();
  for (const auto& entry : profiles) {
    const QVariantMap map = entry.toMap();
    rows_.push_back({map.value("id").toString(), map.value("name").toString(),
                     map.value("description").toString()});
  }
  active_id_ = active_id;
  endResetModel();
  emit countChanged();
  emit activeChanged();
}

}  // namespace hwpanel::client
