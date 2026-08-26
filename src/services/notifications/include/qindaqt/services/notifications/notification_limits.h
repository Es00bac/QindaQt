// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

namespace QindaQt::Services::Notifications {

// These are admission limits, not presentation truncation limits. A caller
// that exceeds one is rejected atomically so no shell process has to retain an
// attacker-controlled, partially normalized payload.
struct NotificationLimits final {
    static constexpr qsizetype MaximumSourceServiceBytes = 255;
    static constexpr qsizetype MaximumApplicationNameBytes = 1'024;
    static constexpr qsizetype MaximumIconBytes = 4'096;
    static constexpr qsizetype MaximumSummaryBytes = 8'192;
    static constexpr qsizetype MaximumBodyBytes = 262'144;
    static constexpr qsizetype MaximumMetadataTextBytes = 4'096;
    static constexpr qsizetype MaximumActionCount = 32;
    static constexpr qsizetype MaximumActionKeyBytes = 256;
    static constexpr qsizetype MaximumActionLabelBytes = 2'048;
    static constexpr qsizetype MaximumActivationTokenBytes = 4'096;
    static constexpr qsizetype MaximumImageBytes = 16 * 1'024 * 1'024;
    static constexpr int MaximumImageDimension = 8'192;
    static constexpr qsizetype MaximumConfiguredActiveNotifications = 4'096;
    static constexpr qsizetype MaximumConfiguredActiveNotificationsPerSource = 4'096;
    static constexpr qsizetype MaximumConfiguredRetainedPayloadBytes = 512 * 1'024 * 1'024;
    static constexpr qsizetype MaximumConfiguredRetainedPayloadBytesPerSource =
        512 * 1'024 * 1'024;
};

} // namespace QindaQt::Services::Notifications
