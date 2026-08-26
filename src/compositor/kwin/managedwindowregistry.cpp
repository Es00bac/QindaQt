// SPDX-License-Identifier: GPL-3.0-or-later
#include "managedwindowregistry.h"

#include "hybridwindowadmission.h"

#include <core/output.h>
#include <window.h>
#include <workspace.h>

#include <KDecoration3/Decoration>

#include <QJsonObject>
#include <QSet>
#include <QTimer>
#include <QUuid>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

QJsonObject rectJson(const QRectF &rect)
{
    return {{QStringLiteral("x"), rect.x()},
            {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()},
            {QStringLiteral("height"), rect.height()}};
}

QString transformName(KWin::OutputTransform::Kind transform)
{
    switch (transform) {
    case KWin::OutputTransform::Normal:
        return QStringLiteral("normal");
    case KWin::OutputTransform::Rotate90:
        return QStringLiteral("rotate-90");
    case KWin::OutputTransform::Rotate180:
        return QStringLiteral("rotate-180");
    case KWin::OutputTransform::Rotate270:
        return QStringLiteral("rotate-270");
    case KWin::OutputTransform::FlipX:
        return QStringLiteral("flip-x");
    case KWin::OutputTransform::FlipX90:
        return QStringLiteral("flip-x-90");
    case KWin::OutputTransform::FlipX180:
        return QStringLiteral("flip-x-180");
    case KWin::OutputTransform::FlipX270:
        return QStringLiteral("flip-x-270");
    }
    Q_UNREACHABLE_RETURN(QStringLiteral("normal"));
}

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

ManagedWindowRegistry::ManagedWindowRegistry(QObject *parent)
    : QObject(parent)
{
    auto *const compositorWorkspace = KWin::workspace();
    Q_ASSERT(compositorWorkspace);
    connect(compositorWorkspace, &KWin::Workspace::windowAdded,
            this, &ManagedWindowRegistry::addWindow);
    connect(compositorWorkspace, &KWin::Workspace::windowRemoved,
            this, &ManagedWindowRegistry::removeWindow);
    connect(compositorWorkspace, &KWin::Workspace::outputsChanged,
            this, &ManagedWindowRegistry::outputsChanged);
    // AGENT-CONTRACT: `Windows` exposes active and absolute stacking state.
    // Their workspace-level changes therefore invalidate the same snapshot as
    // per-client frame/minimize/task-policy changes.
    connect(compositorWorkspace, &KWin::Workspace::windowActivated,
            this, [this](KWin::Window *) { Q_EMIT windowsChanged(); });
    connect(compositorWorkspace, &KWin::Workspace::stackingOrderChanged,
            this, &ManagedWindowRegistry::windowsChanged);
    synchronize();
    // AGENT-GUARD: --exit-with-session can map its first clients while KWin is
    // still loading default plugins. Reconcile once after startup so a client
    // whose windowAdded signal preceded this plugin is not permanently missed.
    QTimer::singleShot(0, this, &ManagedWindowRegistry::synchronize);
}

bool ManagedWindowRegistry::isManageable(const KWin::Window *window)
{
    return admitsHybridTopologyWindow({
        .exists = window != nullptr,
        .deleted = window && window->isDeleted(),
        .internal = window && window->isInternal(),
        .popup = window && window->isPopupWindow(),
        .normal = window && window->isNormalWindow(),
        .transient = window && window->isTransient(),
        .dialog = window && window->isDialog(),
    });
}

QString ManagedWindowRegistry::windowId(const KWin::Window *window) const
{
    return window ? window->internalId().toString(QUuid::WithoutBraces) : QString{};
}

void ManagedWindowRegistry::synchronize()
{
    for (auto *window : KWin::workspace()->windows()) {
        addWindow(window);
    }
}

void ManagedWindowRegistry::addWindow(KWin::Window *window)
{
    if (!isManageable(window)) {
        return;
    }
    const auto id = windowId(window);
    if (id.isEmpty() || m_windows.contains(id)) {
        return;
    }
    m_windows.insert(id, window);
    connect(window, &KWin::Window::captionChanged, this, &ManagedWindowRegistry::windowsChanged);
    connect(window, &KWin::Window::frameGeometryChanged,
            this, [this](const KWin::RectF &) { Q_EMIT windowsChanged(); });
    connect(window, &KWin::Window::minimizedChanged,
            this, &ManagedWindowRegistry::windowsChanged);
    connect(window, &KWin::Window::skipTaskbarChanged,
            this, &ManagedWindowRegistry::windowsChanged);
    connect(window, &KWin::Window::skipSwitcherChanged,
            this, &ManagedWindowRegistry::windowsChanged);
    connect(window, &KWin::Window::keepAboveChanged,
            this, [this](bool) { Q_EMIT windowsChanged(); });
    connect(window, &KWin::Window::keepBelowChanged,
            this, [this](bool) { Q_EMIT windowsChanged(); });
    Q_EMIT managedWindowAdded(id);
    Q_EMIT windowsChanged();
}

void ManagedWindowRegistry::removeWindow(KWin::Window *window)
{
    const auto id = windowId(window);
    if (id.isEmpty() || !m_windows.remove(id)) {
        return;
    }
    const auto containerId = m_owners.take(id);
    m_targetFrames.remove(id);
    Q_EMIT managedWindowClosed(id, containerId);
    Q_EMIT windowsChanged();
}

KWin::Window *ManagedWindowRegistry::window(const QString &windowId) const
{
    return m_windows.value(windowId).data();
}

QStringList ManagedWindowRegistry::windowIds() const
{
    auto result = m_windows.keys();
    result.sort();
    return result;
}

QString ManagedWindowRegistry::owner(const QString &windowId) const
{
    return m_owners.value(windowId);
}

QRectF ManagedWindowRegistry::targetFrame(const QString &windowId) const
{
    const auto *managedWindow = window(windowId);
    return managedWindow
        ? m_targetFrames.value(windowId, managedWindow->moveResizeGeometry())
        : QRectF{};
}

bool ManagedWindowRegistry::setOwner(const QString &windowId,
                                     const QString &containerId,
                                     QString *error)
{
    if (!window(windowId)) {
        return fail(error, QStringLiteral("unknown managed window '%1'").arg(windowId));
    }
    const auto existing = owner(windowId);
    if (!existing.isEmpty() && existing != containerId) {
        return fail(error,
                    QStringLiteral("window '%1' already belongs to '%2'")
                        .arg(windowId, existing));
    }
    if (existing != containerId) {
        m_owners.insert(windowId, containerId);
        m_targetFrames.insert(windowId, window(windowId)->moveResizeGeometry());
        Q_EMIT windowsChanged();
    }
    return true;
}

void ManagedWindowRegistry::clearOwner(const QString &windowId, const QString &containerId)
{
    if (m_owners.value(windowId) == containerId) {
        m_owners.remove(windowId);
        m_targetFrames.remove(windowId);
        Q_EMIT windowsChanged();
    }
}

bool ManagedWindowRegistry::transitionOwners(const QString &containerId,
                                             const QSet<QString> &ownedWindowIds,
                                             const QSet<QString> &releasedWindowIds,
                                             const QHash<QString, QRectF> &targetFrames,
                                             QString *error)
{
    for (const auto &id : ownedWindowIds) {
        if (!window(id)) {
            return fail(error, QStringLiteral("unknown managed window '%1'").arg(id));
        }
        const auto existing = owner(id);
        if (!existing.isEmpty() && existing != containerId) {
            return fail(error,
                        QStringLiteral("window '%1' already belongs to '%2'")
                            .arg(id, existing));
        }
        if (!targetFrames.contains(id) || !targetFrames.value(id).isValid()) {
            return fail(error,
                        QStringLiteral("window '%1' has no valid planned frame").arg(id));
        }
    }

    bool changed = false;
    for (const auto &id : ownedWindowIds) {
        if (m_owners.value(id) != containerId) {
            m_owners.insert(id, containerId);
            changed = true;
        }
        if (m_targetFrames.value(id) != targetFrames.value(id)) {
            m_targetFrames.insert(id, targetFrames.value(id));
            changed = true;
        }
    }
    for (const auto &id : releasedWindowIds) {
        if (m_owners.value(id) == containerId) {
            m_owners.remove(id);
            m_targetFrames.remove(id);
            changed = true;
        }
    }
    if (changed) {
        Q_EMIT windowsChanged();
    }
    return true;
}

bool ManagedWindowRegistry::transitionTopologyOwners(
    const QHash<QString, QString> &expectedOwners,
    const QHash<QString, QString> &candidateOwners,
    const QHash<QString, QRectF> &targetFrames,
    const QSet<QString> &allowedMissingWindowIds,
    QString *error)
{
    if (expectedOwners.size() != candidateOwners.size()) {
        return fail(error, QStringLiteral("owner transition key sets do not match"));
    }
    for (auto iterator = expectedOwners.cbegin(); iterator != expectedOwners.cend();
         ++iterator) {
        if (!candidateOwners.contains(iterator.key())) {
            return fail(error, QStringLiteral("owner transition key sets do not match"));
        }
    }

    for (auto iterator = candidateOwners.cbegin(); iterator != candidateOwners.cend();
         ++iterator) {
        const auto &windowId = iterator.key();
        const auto *managedWindow = window(windowId);
        if (!managedWindow) {
            if (allowedMissingWindowIds.contains(windowId) && iterator.value().isEmpty()) {
                continue;
            }
            return fail(error,
                        QStringLiteral("unknown managed window '%1'").arg(windowId));
        }
        if (owner(windowId) != expectedOwners.value(windowId)) {
            return fail(error,
                        QStringLiteral("window '%1' owner changed during transaction")
                            .arg(windowId));
        }
        if (!iterator.value().isEmpty()
            && (!targetFrames.contains(windowId)
                || !targetFrames.value(windowId).isValid())) {
            return fail(error,
                        QStringLiteral("window '%1' has no valid planned frame")
                            .arg(windowId));
        }
    }

    // AGENT-GUARD: All live objects, expected owners, and target frames are
    // validated before the first map edit. Cross-container moves must never be
    // observable as a transient release followed by a second acquisition.
    bool changed = false;
    for (auto iterator = candidateOwners.cbegin(); iterator != candidateOwners.cend();
         ++iterator) {
        const auto &windowId = iterator.key();
        const auto &containerId = iterator.value();
        if (!window(windowId) || containerId.isEmpty()) {
            changed = m_owners.remove(windowId) > 0 || changed;
            changed = m_targetFrames.remove(windowId) > 0 || changed;
            continue;
        }
        if (m_owners.value(windowId) != containerId) {
            m_owners.insert(windowId, containerId);
            changed = true;
        }
        if (m_targetFrames.value(windowId) != targetFrames.value(windowId)) {
            m_targetFrames.insert(windowId, targetFrames.value(windowId));
            changed = true;
        }
    }
    if (changed) {
        Q_EMIT windowsChanged();
    }
    return true;
}

QJsonArray ManagedWindowRegistry::windowsJson() const
{
    QJsonArray result;
    QHash<const KWin::Window *, int> stackIndices;
    const auto stackingOrder = KWin::workspace()->stackingOrder();
    for (qsizetype index = 0; index < stackingOrder.size(); ++index) {
        stackIndices.insert(stackingOrder[index], int(index));
    }
    auto ids = m_windows.keys();
    ids.sort();
    for (const auto &id : ids) {
        const auto *window = m_windows.value(id).data();
        if (!isManageable(window)) {
            continue;
        }
        const auto *const decoration = window->decoration();
        result.append(QJsonObject{{QStringLiteral("id"), id},
                                  {QStringLiteral("title"), window->caption()},
                                  {QStringLiteral("applicationId"), window->resourceClass()},
                                  // AGENT-CONTRACT: Discovery and kwinrc tests only prove that
                                  // the module can be selected. These fields expose the actual
                                  // server-side instance so nested workflows can reject a silent
                                  // fallback to another decoration implementation.
                                  {QStringLiteral("serverDecorated"), decoration != nullptr},
                                  {QStringLiteral("decorationClass"),
                                   decoration
                                       ? QString::fromLatin1(decoration->metaObject()->className())
                                       : QString{}},
                                  {QStringLiteral("geometry"), rectJson(window->frameGeometry())},
                                  // Wayland resize acknowledgement is
                                  // asynchronous, and a minimized client may
                                  // retain its old frame until reactivated.
                                  // Preserve both truthfully: geometry is
                                  // current; targetGeometry is KWin's latest
                                  // requested bounding frame.
                                  {QStringLiteral("targetGeometry"),
                                   rectJson(m_targetFrames.value(
                                       id, window->moveResizeGeometry()))},
                                  {QStringLiteral("minimized"), window->isMinimized()},
                                  {QStringLiteral("active"), window->isActive()},
                                  {QStringLiteral("skipTaskbar"), window->skipTaskbar()},
                                  {QStringLiteral("skipSwitcher"), window->skipSwitcher()},
                                  {QStringLiteral("keepAbove"), window->keepAbove()},
                                  {QStringLiteral("keepBelow"), window->keepBelow()},
                                  // Absolute Workspace::stackingOrder index,
                                  // bottom-to-top. Live group evidence filters
                                  // this inventory to current managed IDs.
                                  {QStringLiteral("stackIndex"),
                                   stackIndices.value(window, -1)},
                                  {QStringLiteral("containerId"), owner(id)}});
    }
    return result;
}

QJsonArray ManagedWindowRegistry::outputsJson() const
{
    QJsonArray result;
    for (const auto *output : KWin::workspace()->outputOrder()) {
        result.append(QJsonObject{{QStringLiteral("name"), output->name()},
                                  {QStringLiteral("geometry"), rectJson(output->geometryF())},
                                  {QStringLiteral("scale"), output->scale()},
                                  {QStringLiteral("refreshRateMilliHz"),
                                   static_cast<qint64>(output->refreshRate())},
                                  {QStringLiteral("transform"),
                                   transformName(output->transform().kind())},
                                  {QStringLiteral("internal"), output->isInternal()}});
    }
    return result;
}

QStringList ManagedWindowRegistry::containerIds() const
{
    QSet<QString> unique;
    for (const auto &containerId : m_owners) {
        if (!containerId.isEmpty()) {
            unique.insert(containerId);
        }
    }
    auto result = unique.values();
    result.sort();
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
