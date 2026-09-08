#pragma once

#include <QAbstractListModel>
#include <QString>

#include "hwpanel/core/event_bus.h"

namespace hwpanel::client {

// Notification center backing model: newest first, capped at 50 entries.
class NotificationModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
 public:
  enum Roles {
    TimeRole = Qt::UserRole + 1,
    LevelRole,
    TitleRole,
    MessageRole,
  };

  explicit NotificationModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const { return static_cast<int>(rows_.size()); }

 public slots:
  void Append(const hwpanel::core::EventMessage& event);
  void Clear();

 signals:
  void countChanged();

 private:
  struct Row {
    QString time;
    QString level;
    QString title;
    QString message;
  };
  QVector<Row> rows_;
};

}  // namespace hwpanel::client
