// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applet_host/capability_policy.h"

#include <QByteArray>
#include <QString>

namespace QindaQt::AppletHost {

struct CapabilityPolicyLoadResult final {
    bool ok = false;
    CapabilityPolicy policy;
    QString error;
};

class CapabilityPolicyLoader final {
public:
    [[nodiscard]] static CapabilityPolicyLoadResult fromFile(const QString &path);
    [[nodiscard]] static CapabilityPolicyLoadResult fromJson(const QByteArray &json,
                                                             const QString &origin);
};

} // namespace QindaQt::AppletHost
