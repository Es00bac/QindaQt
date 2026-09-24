// SPDX-License-Identifier: GPL-3.0-or-later

// Compiled QML harness for the desktop menu keyboard rows (ADR-0260): the
// real global menu facade, the real desktop menu controller publishing the
// real desktop tree, and a recording targets port, rendered by the compiled
// QindaQt.Shell.GlobalMenu module.
//
// AGENT-NOTE: Qt 6.11 QuickTest invokes no engine-created setup hook, so the
// harness is a singleton created by the test engine on first use (the
// clipboard applet rows' precedent).

#include "desktop_menu_test_support.h"

#include "qindaqt/shell/desktop_menu/desktop_menu_controller.h"

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

using namespace QindaQt::Shell::DesktopMenu;

namespace {

QString commandName(DesktopCommand kind)
{
    switch (kind) {
    case DesktopCommand::SystemSettings:
        return QStringLiteral("SystemSettings");
    case DesktopCommand::ShutDown:
        return QStringLiteral("ShutDown");
    case DesktopCommand::LockScreen:
        return QStringLiteral("LockScreen");
    case DesktopCommand::AboutComputer:
        return QStringLiteral("AboutComputer");
    case DesktopCommand::NewFileManagerWindow:
        return QStringLiteral("NewFileManagerWindow");
    default:
        return QStringLiteral("Other");
    }
}

} // namespace

class DesktopMenuHarness final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *access READ access CONSTANT)
    Q_PROPERTY(QStringList performed READ performed NOTIFY performedChanged)

public:
    DesktopMenuHarness()
        : m_controller(m_access, m_targets)
    {
        connect(&m_controller, &DesktopMenuController::commandPerformed, this,
                [this](const DesktopMenuCommand &command, bool) {
                    m_performed.append(commandName(command.kind));
                    Q_EMIT performedChanged();
                });
        m_controller.setEnabled(true);
        m_controller.setPresence(DesktopMenuController::Presence::NoApplication);
    }

    [[nodiscard]] QObject *access() { return &m_access; }
    [[nodiscard]] QStringList performed() const { return m_performed; }

    Q_INVOKABLE void reset()
    {
        m_performed.clear();
        m_controller.setPresence(DesktopMenuController::Presence::NoApplication);
        Q_EMIT performedChanged();
    }
    Q_INVOKABLE void applicationBecameActive()
    {
        m_controller.setPresence(DesktopMenuController::Presence::ApplicationActive);
    }

Q_SIGNALS:
    void performedChanged();

private:
    QindaQt::Shell::GlobalMenu::GlobalMenuAppletAccess m_access;
    QindaQt::Tests::DesktopMenu::RecordingTargets m_targets;
    DesktopMenuController m_controller;
    QStringList m_performed;
};

class DesktopMenuQmlSetup final : public QObject {
    Q_OBJECT

public:
    DesktopMenuQmlSetup()
    {
        qmlRegisterSingletonType<DesktopMenuHarness>(
            "QindaQt.Shell.DesktopMenu.Tests", 1, 0, "Harness",
            [](QQmlEngine *, QJSEngine *) { return new DesktopMenuHarness; });
    }
};

QUICK_TEST_MAIN_WITH_SETUP(desktop_menu_keyboard, DesktopMenuQmlSetup)

#include "desktop_menu_qml_main.moc"
