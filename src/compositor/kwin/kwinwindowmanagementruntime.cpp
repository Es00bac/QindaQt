// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinwindowmanagementruntime.h"
#include "kwinhybridsession.h"
#include "managedwindowregistry.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <qindaqt/hybrid/windowtopology.h>
#include <wayland/clientconnection.h>
#include <wayland/surface.h>
#include <window.h>
#include <workspace.h>
namespace QindaQt::Compositor::KWinIntegration {
using namespace WindowManagement;
namespace {
QString firstWindow(const Core::LayoutNode &node) {
  if (node.isLeaf())
    return node.windowId();
  return node.firstChild() ? firstWindow(*node.firstChild()) : QString{};
}
Target targetFromObject(const QJsonObject &object) {
  const auto kind = object.value(QStringLiteral("kind")).toString();
  return {kind == QStringLiteral("container") ? Target::Kind::Container
          : kind == QStringLiteral("window")  ? Target::Kind::Window
                                              : Target::Kind::Current,
          object.value(QStringLiteral("id")).toString(),
          object.value(QStringLiteral("name")).toString()};
}
} // namespace
KWinWindowManagementRuntime::KWinWindowManagementRuntime(
    ManagedWindowRegistry &registry, KWinHybridSession &session,
    std::function<std::optional<qint64>()> providerPid)
    : m_registry(registry), m_session(session),
      m_providerPid(std::move(providerPid)) {}
QByteArray KWinWindowManagementRuntime::fingerprint(const QString &id) const {
  const auto *window = m_registry.window(id);
  if (!window || window->isDeleted())
    return {};
  const auto owner = m_registry.owner(id);
  const auto frame = m_registry.targetFrame(id);
  QJsonObject value{
      {QStringLiteral("owner"), owner},
      {QStringLiteral("minimized"), window->isMinimized()},
      {QStringLiteral("fullscreen"), window->isFullScreen()},
      {QStringLiteral("frame"),
       QJsonArray{frame.x(), frame.y(), frame.width(), frame.height()}},
      {QStringLiteral("iconified"), m_session.isWindowIconified(id)},
      {QStringLiteral("shaded"), m_session.isContainerShaded(owner)}};
  if (!owner.isEmpty()) {
    const auto snapshot = m_session.publicSnapshot(owner);
    if (!snapshot)
      return {};
    value.insert(QStringLiteral("topology"),
                 snapshot->value(QStringLiteral("snapshot")));
  }
  // Scoped structural state, not the global topology revision: mapping the
  // provider's command popup must not invalidate the hotkey it just captured.
  return QJsonDocument(value).toJson(QJsonDocument::Compact);
}
std::optional<ContextSnapshot> KWinWindowManagementRuntime::capture() const {
  if (!m_session.ready() || !KWin::workspace())
    return std::nullopt;
  const auto *active = KWin::workspace()->activeWindow();
  if (!active || !active->isNormalWindow() || active->isDeleted())
    return ContextSnapshot{};
  const auto id = m_registry.windowId(active);
  if (id.isEmpty())
    return std::nullopt;
  return ContextSnapshot{id, m_registry.owner(id), m_session.topologyRevision(),
                         fingerprint(id)};
}
bool KWinWindowManagementRuntime::current(
    const ContextSnapshot &snapshot) const {
  if (!m_session.ready() || !KWin::workspace())
    return false;
  const auto *active = KWin::workspace()->activeWindow();
  const auto provider = m_providerPid ? m_providerPid() : std::nullopt;
  const auto *surface = active ? active->surface() : nullptr;
  const bool providerActive = surface && surface->client() && provider &&
                              surface->client()->processId() == *provider;
  if (snapshot.windowId.isEmpty())
    return !active || !active->isNormalWindow() || providerActive;
  if (snapshot.sceneFingerprint.isEmpty() ||
      fingerprint(snapshot.windowId) != snapshot.sceneFingerprint)
    return false;
  return (active && m_registry.windowId(active) == snapshot.windowId) ||
         providerActive;
}
Resolution
KWinWindowManagementRuntime::resolve(const Target &target,
                                     const ContextSnapshot &snapshot) const {
  if (target.kind == Target::Kind::Current) {
    if (snapshot.windowId.isEmpty() || !m_registry.window(snapshot.windowId))
      return {Status::Unavailable,
              {},
              QStringLiteral("no current normal window"),
              {}};
    return {Status::Accepted,
            {snapshot.windowId, m_registry.owner(snapshot.windowId)},
            {},
            {}};
  }
  QStringList ids, labels;
  if (target.kind == Target::Kind::Window) {
    for (const auto &id : m_registry.windowIds()) {
      const auto *window = m_registry.window(id);
      if (!window || window->isDeleted() || !window->isNormalWindow())
        continue;
      if (!target.id.isEmpty()
              ? id == target.id
              : (window->caption().compare(target.name, Qt::CaseInsensitive) ==
                     0 ||
                 window->resourceClass().compare(target.name,
                                                 Qt::CaseInsensitive) == 0)) {
        ids.append(id);
        labels.append(window->caption());
      }
    }
  } else {
    for (const auto &entry : m_session.publicContainers()) {
      const auto row = entry.toObject();
      const auto id = row.value(QStringLiteral("id")).toString();
      const auto name = row.value(QStringLiteral("displayName")).toString();
      if (!target.id.isEmpty()
              ? id == target.id
              : name.compare(target.name, Qt::CaseInsensitive) == 0) {
        ids.append(id);
        labels.append(name);
      }
    }
  }
  if (ids.isEmpty())
    return {
        Status::Unavailable, {}, QStringLiteral("target was not found"), {}};
  if (ids.size() != 1)
    return {Status::Ambiguous,
            {},
            QStringLiteral("target name is ambiguous"),
            labels.mid(0, 8)};
  if (target.kind == Target::Kind::Window)
    return {
        Status::Accepted, {ids.front(), m_registry.owner(ids.front())}, {}, {}};
  const auto snapshotValue = m_session.publicSnapshot(ids.front());
  const auto container =
      snapshotValue
          ? Core::WindowContainer::fromJson(
                snapshotValue->value(QStringLiteral("snapshot")).toObject())
          : std::nullopt;
  const auto *page =
      container ? container->page(container->activePageId()) : nullptr;
  if (!page)
    return {
        Status::Stale, {}, QStringLiteral("container has no active page"), {}};
  return {Status::Accepted, {firstWindow(page->root()), ids.front()}, {}, {}};
}
QStringList KWinWindowManagementRuntime::capabilities() const {
  return {QStringLiteral("focus"),        QStringLiteral("raise"),
          QStringLiteral("minimize"),     QStringLiteral("restore"),
          QStringLiteral("close"),        QStringLiteral("shade"),
          QStringLiteral("unshade"),      QStringLiteral("iconify"),
          QStringLiteral("uniconify"),    QStringLiteral("maximize"),
          QStringLiteral("fullscreen"),   QStringLiteral("rename"),
          QStringLiteral("color"),        QStringLiteral("place"),
          QStringLiteral("detach"),       QStringLiteral("next-tab"),
          QStringLiteral("previous-tab"), QStringLiteral("activate-tab"),
          QStringLiteral("reorder-tab"),  QStringLiteral("resize-split"),
          QStringLiteral("group-tab"),    QStringLiteral("group-tile")};
}
Result KWinWindowManagementRuntime::execute(const Command &command,
                                            const ResolvedTarget &target) {
  ResolvedTarget destination;
  if (command.operation == Operation::GroupTab ||
      command.operation == Operation::GroupTile) {
    const auto requested = targetFromObject(
        command.arguments.value(QStringLiteral("destination")).toObject());
    const auto resolved = resolve(
        requested, {target.contextWindowId, target.contextContainerId, 0, {}});
    if (resolved.status != Status::Accepted)
      return {
          resolved.status, resolved.message, resolved.candidates, {}, {}, {}};
    destination = resolved.target;
  }
  return m_session.executeWindowManagementCommand(command, target, destination);
}
} // namespace QindaQt::Compositor::KWinIntegration
