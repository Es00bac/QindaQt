// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/window_management/grouping_planner.h>
#include "../../hybrid/testfixtures.h"
#include <QJsonArray>
namespace GroupingTest {
using namespace QindaQt;
using namespace Hybrid;
using namespace WindowManagement;
inline const QString Nonce=QStringLiteral("01234567-89ab-4cde-8fab-0123456789ab");
inline WindowTopology scenario(int kind) {
    using namespace Hybrid::Test;
    const auto source=splitContainer("source","source","moved","source-peer");
    const auto target=splitContainer("target-group","target","target","target-peer");
    switch(kind) {
    case 0: return topology({"moved","target"},{},7);
    case 1: return topology({"moved"},{target},7);
    case 2: return topology({"target"},{source},7);
    case 3: return topology({}, {source,target},7);
    default: return topology({}, {source},7);
    }
}
inline GroupingRequest request(int kind,GroupingMode mode) {
    return {"moved",kind==4?"source-peer":"target",mode,GroupingDirection::Above,0.37,Nonce};
}
inline const Core::ContainerPage *memberPage(const Core::WindowContainer &container,const QString &window) {
    for(const auto &page:container.pages()) if(page.root().findWindow(window)) return &page;
    return nullptr;
}
inline QJsonObject fingerprint(const WindowTopology &topology) {
    QJsonArray containers,independent;
    for(const auto &id:topology.containerIds()) containers.append(topology.container(id)->toJson());
    for(const auto &id:topology.independentWindowIds()) independent.append(id);
    return {{"revision",static_cast<double>(topology.revision())},{"containers",containers},{"independent",independent}};
}
inline bool roundTrip(const WindowTopology &value) {
    QVector<Core::WindowContainer> restored;
    for(const auto &id:value.containerIds()) {
        auto container=Core::WindowContainer::fromJson(value.container(id)->toJson());
        if(!container) return false;
        restored.append(std::move(*container));
    }
    const auto copy=WindowTopology::create(value.independentWindowIds(),restored,value.revision());
    return copy && fingerprint(*copy)==fingerprint(value);
}
}
