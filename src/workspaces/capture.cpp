// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layout_binding_p.h"
#include "qindaqt/workspaces/workspace.h"
#include <QSet>
namespace QindaQt::Workspaces {
namespace {
void collect(const Core::LayoutNode &node, QStringList &members) {
  if (node.isLeaf())
    members.append(node.windowId());
  else {
    collect(*node.firstChild(), members);
    collect(*node.secondChild(), members);
  }
}
} // namespace
std::optional<Workspace>
capture(const QString &id, const QString &name, const QString &color,
        const Core::WindowContainer &liveLayout,
        const QMap<QString, ApplicationSlot> &applicationsByWindow,
        QString *error) {
  if (error)
    error->clear();
  auto fail = [error](const QString &message) -> std::optional<Workspace> {
    if (error)
      *error = message;
    return std::nullopt;
  };
  if (!liveLayout.validate().valid)
    return fail(QStringLiteral("The current container layout is invalid"));
  QStringList members;
  for (const auto &page : liveLayout.pages())
    collect(page.root(), members);
  if (applicationsByWindow.size() != members.size())
    return fail(QStringLiteral(
        "Choose an application for every window in the container"));
  Workspace result;
  result.id = id;
  result.name = name;
  result.color = color;
  // Keep the layout's structural ID namespace intact. The container ID is
  // descriptive data here; adoption always supplies a fresh live container ID.
  result.layout = Core::WindowContainer(liveLayout.id());
  QMap<QString, QString> bindings;
  for (const auto &member : members) {
    const auto application = applicationsByWindow.constFind(member);
    if (application == applicationsByWindow.cend())
      return fail(QStringLiteral(
          "An application choice no longer matches the container"));
    bindings.insert(member, application->id);
    result.applicationSlots.append(*application);
  }
  for (const auto &page : liveLayout.pages())
    if (!result.layout.addPage(
            Core::ContainerPage(page.id(),
                                Detail::bindNode(page.root(), bindings)),
            error))
      return std::nullopt;
  if (!result.layout.activatePage(liveLayout.activePageId(), error))
    return std::nullopt;
  return result.validate(error) ? std::optional<Workspace>{result}
                                : std::nullopt;
}
} // namespace QindaQt::Workspaces
