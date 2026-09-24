// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/session/window_management/window_management_preferences.h"

#include <QString>

namespace QindaQt::Session::WindowManagement {

struct KWinWriteOutcome final {
    bool ok = false;
    // True when at least one kwinrc entry actually changed on disk (a
    // reconfigure is worth asking for); false when the file already agreed.
    bool changed = false;
    QString error;
};

struct KWinReadbackOutcome final {
    bool matches = false;
    // Nonempty when an owned key is absent or differs from the requested value.
    QString error;
};

// Writes the preferences into one kwinrc file with KConfig, the format KWin
// re-reads on `reconfigure`. KWin-native knobs go to `[Windows]`
// (FocusPolicy, BorderSnapZone, WindowSnapZone); QindaQt-owned ones to a
// `[QindaQt]` group (DockingModifier, CloseContainerPolicy, SessionRestore)
// the compositor plugin reads on the same reconfigure. Only differing
// entries are written; every other group and key in the file is preserved.
class KWinWindowManagementWriter final {
public:
    explicit KWinWindowManagementWriter(QString kwinrcPath);

    [[nodiscard]] KWinWriteOutcome write(const WindowManagementPreferences &preferences) const;
    [[nodiscard]] KWinReadbackOutcome readback(
        const WindowManagementPreferences &preferences) const;
    [[nodiscard]] const QString &path() const noexcept { return m_path; }

    static constexpr auto WindowsGroup = "Windows";
    static constexpr auto QindaQtGroup = "QindaQt";

private:
    QString m_path;
};

} // namespace QindaQt::Session::WindowManagement
