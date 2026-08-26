// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellsurfaceprotocoltrace.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>

namespace QindaQt::Test {
namespace {

QJsonValue optionalInteger(const std::optional<int> &value)
{
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

QJsonValue optionalSize(const std::optional<QSize> &value)
{
    if (!value) {
        return QJsonValue(QJsonValue::Null);
    }
    return QJsonObject{
        {QStringLiteral("width"), value->width()},
        {QStringLiteral("height"), value->height()},
    };
}

QJsonValue optionalState(bool present, const LayerSurfaceRoleState &state)
{
    return present ? QJsonValue(state.toJson()) : QJsonValue(QJsonValue::Null);
}

} // namespace

bool LayerSurfaceRoleState::isComplete() const noexcept
{
    return layer.has_value() && anchors.has_value() && exclusiveEdge.has_value() &&
        exclusiveZone.has_value() && desiredSize.has_value();
}

QJsonObject LayerSurfaceRoleState::toJson() const
{
    return {
        {QStringLiteral("layer"), optionalInteger(layer)},
        {QStringLiteral("anchors"), optionalInteger(anchors)},
        {QStringLiteral("exclusiveEdge"), optionalInteger(exclusiveEdge)},
        {QStringLiteral("exclusiveZone"), optionalInteger(exclusiveZone)},
        {QStringLiteral("desiredSize"), optionalSize(desiredSize)},
    };
}

bool LayerSurfaceConfigureEvidence::wasAcknowledgedAfterConfigure() const noexcept
{
    return committedEpoch > 0 && configureOrder > 0 && acknowledgeOrder.has_value() &&
        *acknowledgeOrder > configureOrder;
}

QJsonObject LayerSurfaceConfigureEvidence::toJson() const
{
    return {
        {QStringLiteral("serial"), serial},
        {QStringLiteral("width"), configuredSize.width()},
        {QStringLiteral("height"), configuredSize.height()},
        {QStringLiteral("committedEpoch"), committedEpoch},
        {QStringLiteral("committedState"), optionalState(committedEpoch > 0, committedState)},
        {QStringLiteral("configureOrder"), configureOrder},
        {QStringLiteral("acknowledgeOrder"), optionalInteger(acknowledgeOrder)},
    };
}

bool LayerSurfaceMappingEvidence::isCausallyMapped() const noexcept
{
    return commitEpoch > configureCommittedEpoch && configureCommittedEpoch > 0 &&
        attachOrder > 0 && commitOrder > attachOrder && !bufferId.isEmpty() &&
        !configureSerial.isEmpty() && committedState.isComplete();
}

QJsonObject LayerSurfaceMappingEvidence::toJson() const
{
    return {
        {QStringLiteral("commitEpoch"), commitEpoch},
        {QStringLiteral("attachOrder"), attachOrder},
        {QStringLiteral("commitOrder"), commitOrder},
        {QStringLiteral("bufferId"), bufferId},
        {QStringLiteral("configureSerial"), configureSerial},
        {QStringLiteral("configureCommittedEpoch"), configureCommittedEpoch},
        {QStringLiteral("committedState"), committedState.toJson()},
    };
}

bool LayerSurfaceProtocolEvidence::hasMappedBufferEpoch() const noexcept
{
    if (!mapped || !activeBufferMapping || !activeBufferMapping->isCausallyMapped()) {
        return false;
    }
    const auto iterator = configurationsBySerial.constFind(
        activeBufferMapping->configureSerial);
    return iterator != configurationsBySerial.cend() &&
        iterator->wasAcknowledgedAfterConfigure() &&
        *iterator->acknowledgeOrder < activeBufferMapping->attachOrder &&
        iterator->committedEpoch == activeBufferMapping->configureCommittedEpoch &&
        activeBufferMapping->commitEpoch <= committedEpoch;
}

bool LayerSurfaceProtocolEvidence::isCompleteMappedRole() const noexcept
{
    return requestCount == 1 && !roleId.isEmpty() && !waylandSurfaceId.isEmpty() &&
        !outputId.isEmpty() && !roleDestroyed && !surfaceDestroyed &&
        committedEpoch > 0 && committedState.isComplete() && hasMappedBufferEpoch();
}

QJsonObject LayerSurfaceProtocolEvidence::toJson() const
{
    QStringList serials = configurationsBySerial.keys();
    std::sort(serials.begin(), serials.end());
    QJsonArray configurations;
    for (const auto &serial : serials) {
        configurations.append(configurationsBySerial.value(serial).toJson());
    }
    return {
        {QStringLiteral("roleId"), roleId},
        {QStringLiteral("waylandSurfaceId"), waylandSurfaceId},
        {QStringLiteral("outputId"), outputId},
        {QStringLiteral("scope"), scope},
        {QStringLiteral("requestCount"), requestCount},
        {QStringLiteral("initialLayer"), optionalInteger(initialLayer)},
        {QStringLiteral("pendingState"), pendingState.toJson()},
        {QStringLiteral("committedEpoch"), committedEpoch},
        {QStringLiteral("committedState"), optionalState(committedEpoch > 0, committedState)},
        {QStringLiteral("configurations"), configurations},
        {QStringLiteral("mapped"), mapped},
        {QStringLiteral("activeBufferMapping"),
         activeBufferMapping ? QJsonValue(activeBufferMapping->toJson())
                             : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("roleDestroyed"), roleDestroyed},
        {QStringLiteral("surfaceDestroyed"), surfaceDestroyed},
    };
}

bool ShellSurfaceProtocolEvidence::isUsable() const noexcept
{
    return !inputTruncated && !identityAmbiguous && !protocolAmbiguous;
}

bool ShellSurfaceProtocolEvidence::provesMappedSurfaces(int expectedCount) const
{
    if (expectedCount <= 0 || !isUsable()) {
        return false;
    }
    return std::count_if(surfacesByRoleId.cbegin(), surfacesByRoleId.cend(),
                         [](const auto &surface) {
                             return surface.isCompleteMappedRole();
                         }) >= expectedCount;
}

QJsonObject ShellSurfaceProtocolEvidence::toJson() const
{
    QList<QString> roleIds = surfacesByRoleId.keys();
    std::sort(roleIds.begin(), roleIds.end());
    QJsonArray surfaces;
    for (const auto &roleId : roleIds) {
        surfaces.append(surfacesByRoleId.value(roleId).toJson());
    }
    return {
        {QStringLiteral("surfaces"), surfaces},
        {QStringLiteral("inputTruncated"), inputTruncated},
        {QStringLiteral("identityAmbiguous"), identityAmbiguous},
        {QStringLiteral("protocolAmbiguous"), protocolAmbiguous},
    };
}

} // namespace QindaQt::Test
