// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applet_host/capability_policy.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace QindaQt::AppletHost {

struct CapabilityPolicyLoadResult final {
    bool ok = false;
    CapabilityPolicy policy;
    QString error;
};

class CapabilityPolicyLoader final {
public:
    [[nodiscard]] static CapabilityPolicyLoadResult fromFile(const QString &path);
    // Merges policy files, LOW to HIGH precedence. The first file is the base
    // and is strict: if it cannot be read or fails validation the load fails,
    // as fromFile does. Each later file overrides the base: its defaults
    // replace the base's, a rule replaces the earlier rule with the same
    // (trust, packageId, capability) selector, and a rule with a new selector
    // is appended. A later file that cannot be read or fails validation is
    // skipped and described in `skipped`, so a stale or hand-edited user file
    // cannot take the installed policy down. A rule in a lower layer that no
    // higher layer restates still applies: a higher layer overrides, it never
    // deletes. The merged policy is validated like any other.
    [[nodiscard]] static CapabilityPolicyLoadResult fromFiles(const QStringList &paths,
                                                              QStringList *skipped = nullptr);
    [[nodiscard]] static CapabilityPolicyLoadResult fromJson(const QByteArray &json,
                                                             const QString &origin);
};

} // namespace QindaQt::AppletHost
