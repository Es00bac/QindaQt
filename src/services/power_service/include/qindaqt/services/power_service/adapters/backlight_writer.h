// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QString>
#include <QtCore/QtTypes>

namespace QindaQt::Power::Upstream {

enum class BacklightWriteStatus : quint32 {
    Succeeded = 0,
    Unsupported = 1,
    Rejected = 2,
    Failed = 3,
};

struct BacklightWriteOutcome {
    BacklightWriteStatus status = BacklightWriteStatus::Failed;
    QString reasonCode;
    QString diagnostic;
};

// AGENT-CONTRACT: the privileged-write seam behind SysfsBacklightSource
// (ADR-0186). The sysfs source observes brightness and owns admission; when
// the kernel's `brightness` attribute is not writable by this process, the
// injected writer is the only sanctioned way to change it. Implementations
// must never escalate privileges themselves: they delegate to a service that
// already holds the authority (logind owns the seat session's backlight).
//
// AGENT-GUARD: the transport fence in
// tests/services/power_service/check_boundary.cmake forbids
// sysfs_backlight_source.* from naming a bus or a daemon, which is why this
// interface exists at all. Keep it transport-free: no bus types, no paths,
// no daemon names in this header.
class BacklightWriter
{
public:
    virtual ~BacklightWriter() = default;

    BacklightWriter(const BacklightWriter &) = delete;
    BacklightWriter &operator=(const BacklightWriter &) = delete;

    // Whether a privileged write can be attempted right now. Implementations
    // cache a positive answer and re-probe a negative one on their own
    // schedule, because the sysfs source calls this on every rescan.
    [[nodiscard]] virtual bool available() = 0;

    // The stable token the sysfs source publishes as the device diagnostic
    // when available() is false, so the reason a read-only panel cannot be
    // written survives to the Settings route.
    [[nodiscard]] virtual QString unavailableDiagnostic() const = 0;

    // Writes the raw kernel value for the named sysfs backlight device. The
    // device name is the directory name under the backlight class (for
    // example `amdgpu_bl0`), never a path: the writer must not need to know
    // where the class is mounted.
    [[nodiscard]] virtual BacklightWriteOutcome write(const QString &deviceName,
                                                      quint32 value) = 0;

protected:
    BacklightWriter() = default;
};

} // namespace QindaQt::Power::Upstream
