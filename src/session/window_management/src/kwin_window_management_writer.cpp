// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/kwin_window_management_writer.h"

#include <KConfig>
#include <KConfigGroup>

#include <utility>

namespace QindaQt::Session::WindowManagement {

namespace {

template <typename Value>
bool writeIfDifferent(KConfigGroup &group, const char *key, const Value &value)
{
    if (group.hasKey(key) && group.readEntry(key, Value{}) == value) {
        return false;
    }
    group.writeEntry(key, value);
    return true;
}

} // namespace

KWinWindowManagementWriter::KWinWindowManagementWriter(QString kwinrcPath)
    : m_path(std::move(kwinrcPath))
{
}

KWinWriteOutcome KWinWindowManagementWriter::write(
    const WindowManagementPreferences &preferences) const
{
    // AGENT-CONTRACT: SimpleConfig writes exactly this file: no cascading
    // system defaults are merged in and none are copied out, so the user's
    // kwinrc gains only the entries the bridge owns.
    KConfig config(m_path, KConfig::SimpleConfig);
    if (config.accessMode() != KConfigBase::ReadWrite) {
        return {false, false, QStringLiteral("kwinrc at '%1' is not writable").arg(m_path)};
    }
    bool changed = false;
    KConfigGroup windows = config.group(QString::fromLatin1(WindowsGroup));
    changed = writeIfDifferent(windows, "FocusPolicy",
                               WindowManagementPreferences::kwinFocusPolicy(preferences.focusPolicy))
        || changed;
    changed = writeIfDifferent(windows, "BorderSnapZone", preferences.snapDistance) || changed;
    changed = writeIfDifferent(windows, "WindowSnapZone", preferences.snapDistance) || changed;
    KConfigGroup qindaqt = config.group(QString::fromLatin1(QindaQtGroup));
    changed = writeIfDifferent(qindaqt, "DockingModifier",
                               WindowManagementPreferences::dockingModifierName(
                                   preferences.dockingModifier))
        || changed;
    changed = writeIfDifferent(qindaqt, "CloseContainerPolicy",
                               WindowManagementPreferences::closeContainerPolicyName(
                                   preferences.closeContainerPolicy))
        || changed;
    changed = writeIfDifferent(qindaqt, "SessionRestore", preferences.sessionRestore) || changed;
    if (!changed) {
        return {true, false, {}};
    }
    if (!config.sync()) {
        return {false, false, QStringLiteral("kwinrc at '%1' could not be written").arg(m_path)};
    }
    return {true, true, {}};
}

} // namespace QindaQt::Session::WindowManagement
