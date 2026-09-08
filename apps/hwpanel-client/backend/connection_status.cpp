#include "apps/hwpanel-client/backend/connection_status.h"

#include "hwpanel/core/channel_manager.h"

namespace hwpanel::client {

ConnectionStatus::ConnectionStatus(QObject* parent) : QObject(parent) {}

void ConnectionStatus::SetState(int state) {
  const auto value = static_cast<core::ConnectionState>(state);
  connected_ = value == core::ConnectionState::kConnected;
  text_ = QString::fromLatin1(core::ToString(value));
  color_ = connected_ ? "#3ddc84" : (value == core::ConnectionState::kConnecting
                                         ? "#f0a030"
                                         : "#e5484d");
  emit changed();
}

}  // namespace hwpanel::client
