// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"

#include <QString>
#include <QStringList>

class QProcess;

namespace QindaQt::SessionSupervisor {

class TokenizedProcessLauncher final {
public:
    static constexpr int StartTimeoutMilliseconds = 5'000;

    // Starts one child with a one-shot, inherited descriptor appended as
    // --presentation-token-fd. The secret itself never enters argv or env.
    // The child also receives a race-closed kernel parent-death signal so
    // notification authority cannot outlive the supervising session process.
    [[nodiscard]] static bool start(
        QProcess &process, const QString &program, QStringList arguments,
        const Services::NotificationPresentation::PresentationAccessToken &token,
        QString *error = nullptr);
};

} // namespace QindaQt::SessionSupervisor
