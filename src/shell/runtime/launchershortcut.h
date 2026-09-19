// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QKeySequence>
#include <QObject>

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;

// The global shortcut that opens the application launcher.
//
// AGENT-CONTRACT: the Meta key itself is not a KGlobalAccel shortcut -- a bare
// modifier has no key sequence. KWin resolves it through kwinrc's
// [ModifierOnlyShortcuts] Meta entry, which SessionDefaults seeds with a D-Bus
// call to org.kde.kglobalaccel's invokeShortcut for this action's stable id.
// Renaming the id therefore breaks the Meta key on every machine whose kwinrc
// already carries that seeded entry.
class LauncherShortcutProducer final : public QObject {
    Q_OBJECT

public:
    LauncherShortcutProducer(GlobalShortcutRegistrar &registrar,
                             QObject *parent = nullptr);
    ~LauncherShortcutProducer() override;

    LauncherShortcutProducer(const LauncherShortcutProducer &) = delete;
    LauncherShortcutProducer &operator=(const LauncherShortcutProducer &) = delete;

    [[nodiscard]] static QString stableActionId();
    [[nodiscard]] static QKeySequence defaultShortcut();

    [[nodiscard]] bool registrationRequestAccepted() const noexcept;
    [[nodiscard]] bool activeBindingPresent() const noexcept;

Q_SIGNALS:
    void openRequested();
    void activeBindingPresentChanged();

private:
    class Private;
    Private *m_private;
};

} // namespace QindaQt::Shell
