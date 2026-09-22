// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QStringList>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry final : public QObject
{
    Q_OBJECT

public:
    explicit ManagedWindowRegistry(QObject *parent = nullptr);

    // Reconcile clients that may have mapped during KWin plugin startup.
    void synchronize();
    [[nodiscard]] KWin::Window *window(const QString &windowId) const;
    [[nodiscard]] QString windowId(const KWin::Window *window) const;
    [[nodiscard]] QStringList windowIds() const;
    [[nodiscard]] QString owner(const QString &windowId) const;
    [[nodiscard]] QRectF targetFrame(const QString &windowId) const;
    [[nodiscard]] bool setOwner(const QString &windowId,
                                const QString &containerId,
                                QString *error = nullptr);
    void clearOwner(const QString &windowId, const QString &containerId);
    [[nodiscard]] bool transitionOwners(const QString &containerId,
                                        const QSet<QString> &ownedWindowIds,
                                        const QSet<QString> &releasedWindowIds,
                                        const QHash<QString, QRectF> &targetFrames,
                                        QString *error = nullptr);
    [[nodiscard]] bool transitionTopologyOwners(
        const QHash<QString, QString> &expectedOwners,
        const QHash<QString, QString> &candidateOwners,
        const QHash<QString, QRectF> &targetFrames,
        const QSet<QString> &allowedMissingWindowIds,
        QString *error = nullptr);
    [[nodiscard]] QJsonArray windowsJson() const;
    [[nodiscard]] QStringList containerIds() const;

Q_SIGNALS:
    void managedWindowAdded(const QString &windowId);
    void managedWindowClosed(const QString &windowId, const QString &containerId);
    void windowsChanged();
    void outputsChanged();

private:
    void addWindow(KWin::Window *window);
    void removeWindow(KWin::Window *window);
    // Coalesces windowsChanged to at most one emission per event-loop turn.
    //
    // AGENT-GUARD: every invalidation must go through this, never Q_EMIT
    // windowsChanged() directly. The signal is a "re-read the snapshot"
    // notification, and one of its consumers turns each emission into a D-Bus
    // broadcast; the per-window sources include frameGeometryChanged and
    // captionChanged, which fire once per frame while a window is dragged or
    // retitled. Emitting per source made every consumer pay for a burst that
    // described one settled change.
    void scheduleWindowsChanged();
    [[nodiscard]] static bool isManageable(const KWin::Window *window);

    QHash<QString, QPointer<KWin::Window>> m_windows;
    QHash<QString, QString> m_owners;
    // Compositor-planned logical frames survive KWin minimizing a Wayland
    // client, which may reset its transient moveResizeGeometry before the
    // client can acknowledge the configure.
    QHash<QString, QRectF> m_targetFrames;
    bool m_windowsChangedPending = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
