// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

namespace QindaQt::Services::NotificationPresentation {

struct WireContract final {
    static constexpr quint32 SchemaVersion = 1;
    static constexpr auto DefaultServiceName = "org.freedesktop.Notifications";
    static constexpr auto ObjectPath = "/org/qindaqt/NotificationPresentation1";
    static constexpr auto InterfaceName = "org.qindaqt.NotificationPresentation1";

    static constexpr qsizetype MaximumNotifications = 256;
    static constexpr qsizetype MaximumApplicationNameBytes = 1'024;
    static constexpr qsizetype MaximumIconBytes = 4'096;
    static constexpr qsizetype MaximumSummaryBytes = 8'192;
    static constexpr qsizetype MaximumBodyBytes = 262'144;
    static constexpr qsizetype MaximumMetadataTextBytes = 4'096;
    static constexpr qsizetype MaximumActions = 32;
    static constexpr qsizetype MaximumActionKeyBytes = 256;
    static constexpr qsizetype MaximumActionLabelBytes = 2'048;
    static constexpr qsizetype MaximumActivationTokenBytes = 4'096;
    static constexpr qsizetype MaximumSnapshotTextBytes = 64 * 1'024 * 1'024;
};

} // namespace QindaQt::Services::NotificationPresentation
