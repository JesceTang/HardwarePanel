#pragma once

#include <QObject>
#include <QString>

namespace hwpanel::client {

// Connection status bar view-model fed by ChannelManager state transitions.
class ConnectionStatus : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool connected READ connected NOTIFY changed)
  Q_PROPERTY(QString text READ text NOTIFY changed)
  Q_PROPERTY(QString color READ color NOTIFY changed)
 public:
  explicit ConnectionStatus(QObject* parent = nullptr);

  bool connected() const { return connected_; }
  QString text() const { return text_; }
  QString color() const { return color_; }

 public slots:
  void SetState(int state);

 signals:
  void changed();

 private:
  bool connected_ = false;
  QString text_ = tr("connecting");
  QString color_ = "#f0a030";
};

}  // namespace hwpanel::client
