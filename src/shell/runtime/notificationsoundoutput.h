// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QtTypes>

#include <functional>

namespace QindaQt::Services::NotificationPresentationModel {
class NotificationPresentationController;
}

namespace QindaQt::Shell {

// Binds presenter sound requests to a shell-owned platform alert output. The
// context owns the connection lifetime; the output receives only the
// notification ID and may not inspect presenter or Settings internals.
void connectNotificationSoundOutput(
    Services::NotificationPresentationModel::NotificationPresentationController &presenter,
    QObject &context, std::function<void(quint32)> platformAlert);

} // namespace QindaQt::Shell
