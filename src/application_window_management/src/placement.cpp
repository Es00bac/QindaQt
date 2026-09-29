// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/application_window_management/placement.h>
namespace QindaQt::ApplicationWindowManagement {
PlacementPlan planPlacement(const Hybrid::WindowTopology &topology,
    const QString &source, const QString &created, Placement placement, const QString &nonce)
{
    if (source.isEmpty() || created.isEmpty() || source == created || nonce.isEmpty()
        || static_cast<quint32>(placement) > static_cast<quint32>(Placement::TileUp))
        return {{}, {}, QStringLiteral("invalid placement request")};
    if (!topology.isIndependent(created))
        return {{}, {}, QStringLiteral("new window must be independent")};
    const auto owner = topology.ownerOf(source);
    if (!owner && !topology.isIndependent(source))
        return {{}, {}, QStringLiteral("source window is not managed")};
    const auto id = [&nonce](const char *role) { return QStringLiteral("app-%1-%2").arg(nonce, QString::fromLatin1(role)); };
    const auto orientation = placement == Placement::TileDown || placement == Placement::TileUp
        ? Core::SplitOrientation::Vertical : Core::SplitOrientation::Horizontal;
    const auto position = placement == Placement::TileLeft || placement == Placement::TileUp
        ? Core::InsertPosition::First : Core::InsertPosition::Second;
    PlacementPlan result;
    result.containerId = owner.value_or(id("container"));
    if (!owner) {
        if (placement == Placement::Tab) {
            result.command = Hybrid::GroupIndependentWindowsAsPages{
                .containerId=result.containerId, .firstWindowId=source,
                .firstPageId=id("source-page"), .firstLeafNodeId=id("source-leaf"),
                .secondWindowId=created, .secondPageId=id("new-page"),
                .secondLeafNodeId=id("new-leaf"), .activateSecondPage=true};
        } else {
            result.command = Hybrid::DockIndependentWindows{
                .containerId=result.containerId, .pageId=id("source-page"),
                .firstWindowId=source, .firstLeafNodeId=id("source-leaf"),
                .secondWindowId=created, .secondLeafNodeId=id("new-leaf"),
                .splitNodeId=id("split"), .orientation=orientation, .ratio=0.5,
                .secondPosition=position};
        }
        return result;
    }
    const auto *container = topology.container(*owner);
    if (!container) return {{}, {}, QStringLiteral("source ownership is inconsistent")};
    qsizetype pageIndex = 0;
    for (const auto &page : container->pages()) {
        if (page.root().findWindow(source)) break;
        ++pageIndex;
    }
    if (pageIndex == container->pages().size())
        return {{}, {}, QStringLiteral("source page is missing")};
    Hybrid::MemberDestination destination;
    if (placement == Placement::Tab)
        destination = Hybrid::MoveAsPage{.pageId=id("new-page"), .destinationPageIndex=pageIndex+1};
    else
        destination = Hybrid::MoveAsSplit{.targetWindowId=source, .splitNodeId=id("split"),
            .orientation=orientation, .ratio=0.5, .position=position};
    result.command = Hybrid::InsertIndependentWindow{
        .targetContainerId=*owner, .windowId=created, .leafNodeId=id("new-leaf"),
        .destination=std::move(destination), .activateInsertedPage=placement==Placement::Tab};
    return result;
}
} // namespace QindaQt::ApplicationWindowManagement
