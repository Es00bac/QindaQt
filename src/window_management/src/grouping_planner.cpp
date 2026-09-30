// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/window_management/grouping_planner.h>
#include <QUuid>
#include <cmath>
namespace QindaQt::WindowManagement {
namespace {
using namespace Hybrid;
GroupingPlan reject(const QString &message) { return {{},std::nullopt,message}; }
bool boundedId(const QString &id) {
    return !id.isEmpty() && id.toUtf8().size() <= 128 && !id.contains(QChar(0));
}
bool structuralId(const Core::WindowContainer &container,const QString &id) {
    return container.id()==id || container.page(id) || container.findNode(id);
}
const Core::ContainerPage *pageFor(const Core::WindowContainer &container,const QString &window) {
    for(const auto &page:container.pages()) if(page.root().findWindow(window)) return &page;
    return nullptr;
}
struct Ids {
    QString container,targetPage,movedPage,targetLeaf,movedLeaf,split;
    explicit Ids(const QString &nonce) {
        const auto prefix=QStringLiteral("wm-group-")+nonce+QLatin1Char('-');
        container=prefix+"container";targetPage=prefix+"target-page";movedPage=prefix+"moved-page";
        targetLeaf=prefix+"target-leaf";movedLeaf=prefix+"moved-leaf";split=prefix+"split";
    }
    QStringList all() const {return {container,targetPage,movedPage,targetLeaf,movedLeaf,split};}
};
}
GroupingPlan planGrouping(const Hybrid::WindowTopology &topology,const GroupingRequest &request) {
    using namespace Hybrid;
    if(!topology.validate().valid) return reject(QStringLiteral("Topology is not valid"));
    if(!boundedId(request.movedWindowId) || !boundedId(request.targetWindowId)
        || request.movedWindowId==request.targetWindowId) return reject(QStringLiteral("Grouping requires two distinct bounded window IDs"));
    const auto uuid=QUuid(request.nonce);
    if(uuid.isNull() || uuid.toString(QUuid::WithoutBraces)!=request.nonce)
        return reject(QStringLiteral("Grouping requires a canonical UUID nonce"));
    if(request.mode!=GroupingMode::Tab && request.mode!=GroupingMode::Tile)
        return reject(QStringLiteral("Unknown grouping mode"));
    Core::SplitOrientation orientation;
    Core::InsertPosition position;
    switch(request.direction) {
    case GroupingDirection::Left: orientation=Core::SplitOrientation::Horizontal;position=Core::InsertPosition::First;break;
    case GroupingDirection::Right: orientation=Core::SplitOrientation::Horizontal;position=Core::InsertPosition::Second;break;
    case GroupingDirection::Above: orientation=Core::SplitOrientation::Vertical;position=Core::InsertPosition::First;break;
    case GroupingDirection::Below: orientation=Core::SplitOrientation::Vertical;position=Core::InsertPosition::Second;break;
    default: return reject(QStringLiteral("Unknown grouping direction"));
    }
    if(!std::isfinite(request.ratio) || request.ratio<=0 || request.ratio>=1)
        return reject(QStringLiteral("Split ratio must be finite and strictly between zero and one"));
    const auto sourceOwner=topology.ownerOf(request.movedWindowId),targetOwner=topology.ownerOf(request.targetWindowId);
    const bool sourceIndependent=topology.isIndependent(request.movedWindowId),targetIndependent=topology.isIndependent(request.targetWindowId);
    if((!sourceIndependent && !sourceOwner) || (!targetIndependent && !targetOwner))
        return reject(QStringLiteral("A resolved window no longer has topology ownership"));
    const Ids ids(request.nonce);
    for(const auto &containerId:topology.containerIds()) {
        const auto *container=topology.container(containerId);
        for(const auto &id:ids.all()) if(structuralId(*container,id))
            return reject(QStringLiteral("Grouping nonce collides with a current structural ID"));
    }
    const auto *target=targetOwner?topology.container(*targetOwner):nullptr;
    const auto *targetPage=target?pageFor(*target,request.targetWindowId):nullptr;
    if(target && !targetPage) return reject(QStringLiteral("Destination leaf is no longer present"));
    if(sourceOwner && targetOwner && sourceOwner!=targetOwner) {
        const auto *source=topology.container(*sourceOwner);
        const auto *leaf=source->findWindow(request.movedWindowId);
        if(!leaf || structuralId(*target,leaf->id()))
            return reject(QStringLiteral("Retained source leaf ID collides in the destination"));
    }
    const bool tab=request.mode==GroupingMode::Tab;
    const auto destinationIndex=[&] {
        for(qsizetype index=0;index<target->pages().size();++index)
            if(target->pages()[index].id()==targetPage->id()) return index+1;
        return qsizetype{-1};
    };
    if(sourceIndependent && targetIndependent) {
        if(tab) return {ids.container,GroupIndependentWindowsAsPages{ids.container,request.targetWindowId,ids.targetPage,ids.targetLeaf,request.movedWindowId,ids.movedPage,ids.movedLeaf,true},{}};
        return {ids.container,DockIndependentWindows{ids.container,ids.targetPage,request.targetWindowId,ids.targetLeaf,request.movedWindowId,ids.movedLeaf,ids.split,orientation,request.ratio,position},{}};
    }
    if(sourceIndependent) {
        MemberDestination destination=tab?MemberDestination{MoveAsPage{ids.movedPage,destinationIndex()}}:MemberDestination{MoveAsSplit{request.targetWindowId,ids.split,orientation,request.ratio,position}};
        return {*targetOwner,InsertIndependentWindow{*targetOwner,request.movedWindowId,ids.movedLeaf,std::move(destination),tab},{}};
    }
    if(targetIndependent) {
        RegroupLayout layout=tab?RegroupLayout{RegroupAsPages{ids.targetPage,ids.movedPage,ids.targetLeaf}}:RegroupLayout{RegroupAsSplit{ids.targetPage,ids.targetLeaf,ids.split,orientation,request.ratio,position}};
        return {ids.container,RegroupMemberWithIndependent{*sourceOwner,request.movedWindowId,request.targetWindowId,ids.container,std::move(layout),tab},{}};
    }
    if(sourceOwner!=targetOwner) {
        MemberDestination destination=tab?MemberDestination{MoveAsPage{ids.movedPage,destinationIndex()}}:MemberDestination{MoveAsSplit{request.targetWindowId,ids.split,orientation,request.ratio,position}};
        return {*targetOwner,MoveMember{*sourceOwner,*targetOwner,request.movedWindowId,std::move(destination),tab},{}};
    }
    if(tab) {
        const auto *sourcePage=pageFor(*target,request.movedWindowId);
        if(!sourcePage || (sourcePage->id()==targetPage->id() && !sourcePage->root().isSplit()))
            return reject(QStringLiteral("The destination page must retain another leaf"));
        return {*targetOwner,MoveMemberToPage{*targetOwner,request.movedWindowId,ids.movedPage,targetPage->id(),true},{}};
    }
    return {*targetOwner,ReparentMember{*targetOwner,request.movedWindowId,request.targetWindowId,ids.split,orientation,request.ratio,position},{}};
}
}
