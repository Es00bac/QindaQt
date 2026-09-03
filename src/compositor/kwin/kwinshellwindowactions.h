// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowactions.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QList>
#include <QObject>
#include <QPointer>

namespace KWin {
class LayerSurfaceV1Interface;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinHybridSession;
class KWinShellVisibilityPublisher;
class ManagedWindowRegistry;

class QtBusShellCredentialSource final : public ShellWindowCredentialSource
{
public:
    explicit QtBusShellCredentialSource(QDBusConnection connection);
    [[nodiscard]] std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const override;

private:
    QDBusConnection m_connection;
};

// Derives one live authority from committed production panel roles. Surface
// objects are borrowed QPointers and are never exposed outside this boundary.
class KWinShellPanelOwnerSource final : public QObject,
                                       public ShellPanelOwnerSource
{
    Q_OBJECT

public:
    explicit KWinShellPanelOwnerSource(QObject *parent = nullptr);
    [[nodiscard]] std::optional<qint64> shellPanelProcessId() const override;

private:
    void track(KWin::LayerSurfaceV1Interface *surface);
    QList<QPointer<KWin::LayerSurfaceV1Interface>> m_surfaces;
};

class KWinShellWindowRegistry final : public ShellWindowRegistry
{
public:
    KWinShellWindowRegistry(ManagedWindowRegistry &registry,
                            KWinShellVisibilityPublisher &visibility);
    [[nodiscard]] std::optional<ShellWindowGeneration>
    currentGeneration() const override;
    [[nodiscard]] std::optional<ShellWindowTarget> target(
        const QString &windowId) const override;

private:
    ManagedWindowRegistry &m_registry;
    KWinShellVisibilityPublisher &m_visibility;
};

class KWinShellWindowActionExecutor final : public ShellWindowActionExecutor
{
public:
    KWinShellWindowActionExecutor(ManagedWindowRegistry &registry,
                                  KWinHybridSession &hybridSession);
    [[nodiscard]] bool execute(ShellWindowAction action,
                               const ShellWindowTarget &target,
                               QString *error = nullptr) override;

private:
    ManagedWindowRegistry &m_registry;
    KWinHybridSession &m_hybridSession;
};

class KWinShellWindowActionsEndpoint final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.CompositorShell1")

public:
    explicit KWinShellWindowActionsEndpoint(
        ShellWindowActionController &controller,
        QObject *parent = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE [[nodiscard]] QByteArray ActivateWindow(
        const QString &windowId, const QString &epoch, const QString &revision);
    Q_SCRIPTABLE [[nodiscard]] QByteArray MinimizeWindow(
        const QString &windowId, const QString &epoch, const QString &revision);
    Q_SCRIPTABLE [[nodiscard]] QByteArray UnminimizeWindow(
        const QString &windowId, const QString &epoch, const QString &revision);
    Q_SCRIPTABLE [[nodiscard]] QByteArray CloseWindow(
        const QString &windowId, const QString &epoch, const QString &revision);
    Q_SCRIPTABLE [[nodiscard]] QByteArray RaiseWindow(
        const QString &windowId, const QString &epoch, const QString &revision);

private:
    [[nodiscard]] QByteArray submit(ShellWindowAction action,
                                    const QString &windowId,
                                    const QString &epoch,
                                    const QString &revision);
    ShellWindowActionController &m_controller;
};

} // namespace QindaQt::Compositor::KWinIntegration
