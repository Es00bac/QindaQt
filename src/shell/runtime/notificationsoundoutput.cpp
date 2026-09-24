// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationsoundoutput.h"

#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"

#include <QObject>

#include <utility>

namespace QindaQt::Shell {

void connectNotificationSoundOutput(
    Services::NotificationPresentationModel::NotificationPresentationController &presenter,
    QObject &context, std::function<void(quint32)> platformAlert)
{
    if (!platformAlert) {
        return;
    }
    QObject::connect(
        &presenter,
        &Services::NotificationPresentationModel::NotificationPresentationController::
            notificationSoundRequested,
        &context, std::move(platformAlert));
}

} // namespace QindaQt::Shell
