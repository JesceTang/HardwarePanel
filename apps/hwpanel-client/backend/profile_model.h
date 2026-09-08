#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>

namespace hwpanel::client {

// QAbstractListModel view over the profile list fetched from the service
// (plan: QAbstractListModel exposed to QML).
class ProfileModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(QString activeId READ activeId NOTIFY activeChanged)
 public:
  enum Roles {
    IdRole = Qt::UserRole + 1,
    NameRole,
    DescriptionRole,
    ActiveRole,
  };

  explicit ProfileModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const { return static_cast<int>(rows_.size()); }
  QString activeId() const { return active_id_; }

 public slots:
  void SetProfiles(const QVariantList& profiles, const QString& active_id);

 signals:
  void countChanged();
  void activeChanged();

 private:
  struct Row {
    QString id;
    QString name;
    QString description;
  };
  QVector<Row> rows_;
  QString active_id_;
};

}  // namespace hwpanel::client
