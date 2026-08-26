// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellsurfaceprotocoltrace.h"

#include <QRegularExpression>

#include <array>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr qsizetype maximumLineBytes = 64 * 1024;
constexpr qsizetype maximumChunkBytes = 256 * 1024;
constexpr qsizetype maximumTraceBytes = 4 * 1024 * 1024;
constexpr qsizetype maximumSurfaceRecords = 16;
constexpr qsizetype maximumConfigureRecordsPerSurface = 16;
constexpr qsizetype maximumPreRoleSurfaceIds = 32;

bool parseInteger(const QString &text, int *value)
{
    bool ok = false;
    const qlonglong parsed = text.toLongLong(&ok);
    if (!ok || parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool isUnsigned32(const QString &text, bool allowZero)
{
    bool ok = false;
    const qulonglong parsed = text.toULongLong(&ok);
    return ok && parsed <= std::numeric_limits<quint32>::max() &&
        (allowZero || parsed != 0);
}

QString clientRequestPattern(const QString &message)
{
    return QStringLiteral(
               R"(^\[[^]\r\n]{1,64}\]\s{1,8}(?:\{[^}\r\n]{1,128}\}\s{1,8})?->\s{1,8})") +
        message + QStringLiteral(R"(\s*$)");
}

QString serverEventPattern(const QString &message)
{
    return QStringLiteral(
               R"(^\[[^]\r\n]{1,64}\]\s{1,8}(?:\{[^}\r\n]{1,128}\}\s{1,8})?)") +
        message + QStringLiteral(R"(\s*$)");
}

} // namespace

void ShellSurfaceProtocolTrace::invalidateInput()
{
    m_pending.clear();
    m_evidence.inputTruncated = true;
    m_ignoreInput = true;
}

void ShellSurfaceProtocolTrace::ingest(const QByteArray &chunk)
{
    if (m_ignoreInput || chunk.isEmpty()) {
        return;
    }
    if (chunk.size() > maximumChunkBytes ||
        m_totalAcceptedBytes > maximumTraceBytes - chunk.size()) {
        invalidateInput();
        return;
    }
    m_totalAcceptedBytes += chunk.size();

    qsizetype offset = 0;
    while (offset < chunk.size()) {
        const qsizetype newline = chunk.indexOf('\n', offset);
        const qsizetype end = newline >= 0 ? newline : chunk.size();
        const qsizetype segmentBytes = end - offset;
        if (segmentBytes > maximumLineBytes - m_pending.size()) {
            invalidateInput();
            return;
        }
        m_pending.append(chunk.constData() + offset, segmentBytes);
        if (newline < 0) {
            return;
        }
        parseLine(m_pending);
        m_pending.clear();
        offset = newline + 1;
    }
}

void ShellSurfaceProtocolTrace::finish()
{
    if (!m_ignoreInput && !m_pending.isEmpty()) {
        parseLine(m_pending);
    }
    m_pending.clear();
}

const ShellSurfaceProtocolEvidence &ShellSurfaceProtocolTrace::evidence() const noexcept
{
    return m_evidence;
}

LayerSurfaceProtocolEvidence *ShellSurfaceProtocolTrace::findSurface(const QString &roleId)
{
    const auto iterator = m_evidence.surfacesByRoleId.find(roleId);
    return iterator == m_evidence.surfacesByRoleId.end() ? nullptr : &iterator.value();
}

LayerSurfaceProtocolEvidence *ShellSurfaceProtocolTrace::findSurfaceByWaylandId(
    const QString &surfaceId)
{
    for (auto iterator = m_evidence.surfacesByRoleId.begin();
         iterator != m_evidence.surfacesByRoleId.end(); ++iterator) {
        if (iterator->waylandSurfaceId == surfaceId) {
            return &iterator.value();
        }
    }
    return nullptr;
}

LayerSurfaceProtocolEvidence *ShellSurfaceProtocolTrace::liveSurface(const QString &roleId)
{
    auto *surface = findSurface(roleId);
    if (surface == nullptr || surface->roleDestroyed || surface->surfaceDestroyed) {
        m_evidence.protocolAmbiguous = true;
        return nullptr;
    }
    return surface;
}

LayerSurfaceProtocolEvidence *ShellSurfaceProtocolTrace::liveSurfaceByWaylandId(
    const QString &surfaceId)
{
    auto *surface = findSurfaceByWaylandId(surfaceId);
    if (surface == nullptr) {
        recordPreRoleSurfaceActivity(surfaceId);
        return nullptr;
    }
    if (surface->roleDestroyed || surface->surfaceDestroyed) {
        m_evidence.protocolAmbiguous = true;
        return nullptr;
    }
    return surface;
}

LayerSurfaceProtocolEvidence *ShellSurfaceProtocolTrace::recordRequestedSurface(
    const QString &roleId, const QString &surfaceId)
{
    if (auto *existing = findSurface(roleId)) {
        if (existing->requestCount < std::numeric_limits<int>::max()) {
            ++existing->requestCount;
        }
        m_evidence.identityAmbiguous = true;
        return existing;
    }
    if (m_evidence.surfacesByRoleId.size() >= maximumSurfaceRecords) {
        invalidateInput();
        return nullptr;
    }
    if (findSurfaceByWaylandId(surfaceId) != nullptr ||
        m_evidence.surfacesByRoleId.contains(surfaceId) ||
        m_preRoleActivitySurfaceIds.contains(surfaceId)) {
        m_evidence.identityAmbiguous = true;
    }
    for (const auto &surface : std::as_const(m_evidence.surfacesByRoleId)) {
        if (surface.waylandSurfaceId == roleId) {
            m_evidence.identityAmbiguous = true;
            break;
        }
    }
    LayerSurfaceProtocolEvidence surface;
    surface.roleId = roleId;
    surface.waylandSurfaceId = surfaceId;
    surface.requestCount = 1;
    return &m_evidence.surfacesByRoleId.insert(roleId, std::move(surface)).value();
}

void ShellSurfaceProtocolTrace::recordPreRoleSurfaceActivity(const QString &surfaceId)
{
    if (m_preRoleActivitySurfaceIds.contains(surfaceId)) {
        return;
    }
    if (m_preRoleActivitySurfaceIds.size() >= maximumPreRoleSurfaceIds) {
        invalidateInput();
        return;
    }
    m_preRoleActivitySurfaceIds.insert(surfaceId);
}

int ShellSurfaceProtocolTrace::nextObservationOrder()
{
    if (m_observationOrder == std::numeric_limits<int>::max()) {
        invalidateInput();
        return 0;
    }
    return ++m_observationOrder;
}

void ShellSurfaceProtocolTrace::commitSurface(LayerSurfaceProtocolEvidence &surface,
                                              int commitOrder)
{
    if (surface.committedEpoch == std::numeric_limits<int>::max()) {
        invalidateInput();
        return;
    }
    ++surface.committedEpoch;
    surface.committedState = surface.pendingState;
    if (!surface.pendingAttachmentObserved) {
        return;
    }

    if (!surface.pendingBufferId) {
        surface.mapped = false;
        surface.activeBufferMapping.reset();
    } else {
        surface.mapped = true;
        LayerSurfaceMappingEvidence mapping;
        mapping.commitEpoch = surface.committedEpoch;
        mapping.attachOrder = surface.pendingAttachmentOrder;
        mapping.commitOrder = commitOrder;
        mapping.bufferId = *surface.pendingBufferId;
        mapping.committedState = surface.committedState;
        // AGENT-GUARD: Snapshot the acknowledged configure at attach time,
        // not commit time. An acknowledge between attach and commit cannot
        // retroactively establish which configure the attached buffer obeyed.
        if (surface.pendingConfigureSerial) {
            mapping.configureSerial = *surface.pendingConfigureSerial;
            const auto configuration = surface.configurationsBySerial.constFind(
                mapping.configureSerial);
            if (configuration != surface.configurationsBySerial.cend() &&
                configuration->wasAcknowledgedAfterConfigure()) {
                mapping.configureCommittedEpoch = configuration->committedEpoch;
            }
        }
        surface.activeBufferMapping = std::move(mapping);
        if (!surface.activeBufferMapping->isCausallyMapped()) {
            m_evidence.protocolAmbiguous = true;
        }
    }
    surface.pendingAttachmentObserved = false;
    surface.pendingBufferId.reset();
    surface.pendingConfigureSerial.reset();
    surface.pendingAttachmentOrder = 0;
}

void ShellSurfaceProtocolTrace::parseLine(const QByteArray &line)
{
    const QString text = QString::fromUtf8(line);
    if (parseRoleCreation(text) || parseRoleStateRequest(text) ||
        parseConfigureHandshake(text) || parseSurfaceTransaction(text)) {
        return;
    }
    if (parseDestruction(text)) {
        return;
    }

    // Lines for methods in this proof vocabulary must either match a bounded,
    // direction-specific grammar above or invalidate the proof. Otherwise an
    // oversized capture or malformed setter could be silently ignored after a
    // previously valid mapped epoch and create a false pass.
    static constexpr std::array<std::string_view, 8> layerSurfaceMethods = {
        ".set_size(",
        ".set_anchor(",
        ".set_exclusive_edge(",
        ".set_exclusive_zone(",
        ".set_layer(",
        ".configure(",
        ".ack_configure(",
        ".destroy(",
    };
    const std::string_view lineView(line.constData(), static_cast<std::size_t>(line.size()));
    const auto contains = [lineView](std::string_view value) {
        return lineView.find(value) != std::string_view::npos;
    };
    const bool layerSurfaceMethod = contains("zwlr_layer_surface_v1#") &&
        std::ranges::any_of(layerSurfaceMethods, [contains](std::string_view method) {
            return contains(method);
        });
    const bool roleCreation = contains("zwlr_layer_shell_v1#") &&
        contains(".get_layer_surface(");
    const bool surfaceTransaction = contains("wl_surface#") &&
        (contains(".attach(") || contains(".commit(") || contains(".destroy("));
    if (roleCreation || layerSurfaceMethod || surfaceTransaction) {
        m_evidence.protocolAmbiguous = true;
    }
}

bool ShellSurfaceProtocolTrace::parseRoleCreation(const QString &text)
{
    static const QRegularExpression rolePattern(clientRequestPattern(QStringLiteral(
        R"REGEX(zwlr_layer_shell_v1#[0-9]{1,10}\.get_layer_surface\(new id zwlr_layer_surface_v1#([0-9]{1,10}),\s*wl_surface#([0-9]{1,10}),\s*(?:wl_output#([0-9]{1,10})|nil),\s*([0-9]{1,10}),\s*"([^"\r\n]{0,128})"\))REGEX")));
    const auto match = rolePattern.match(text);
    if (!match.hasMatch()) {
        return false;
    }
    int layer = 0;
    if (!isUnsigned32(match.captured(1), false) ||
        !isUnsigned32(match.captured(2), false) ||
        (!match.captured(3).isEmpty() && !isUnsigned32(match.captured(3), false)) ||
        !parseInteger(match.captured(4), &layer)) {
        m_evidence.protocolAmbiguous = true;
        return true;
    }
    auto *surface = recordRequestedSurface(match.captured(1), match.captured(2));
    if (surface != nullptr && surface->requestCount == 1) {
        surface->outputId = match.captured(3);
        surface->scope = match.captured(5);
        surface->initialLayer = layer;
        surface->pendingState.layer = layer;
    }
    return true;
}

bool ShellSurfaceProtocolTrace::parseRoleStateRequest(const QString &text)
{
    static const QRegularExpression sizePattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.set_size\(([0-9]{1,10}),\s*([0-9]{1,10})\))")));
    static const QRegularExpression anchorPattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.set_anchor\(([0-9]{1,10})\))")));
    static const QRegularExpression edgePattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.set_exclusive_edge\(([0-9]{1,10})\))")));
    static const QRegularExpression zonePattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.set_exclusive_zone\((-?[0-9]{1,10})\))")));
    static const QRegularExpression layerPattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.set_layer\(([0-9]{1,10})\))")));
    auto match = sizePattern.match(text);
    if (match.hasMatch()) {
        int width = 0;
        int height = 0;
        auto *surface = liveSurface(match.captured(1));
        if (surface != nullptr && parseInteger(match.captured(2), &width) &&
            parseInteger(match.captured(3), &height)) {
            surface->pendingState.desiredSize = QSize(width, height);
        } else if (surface != nullptr) {
            m_evidence.protocolAmbiguous = true;
        }
        return true;
    }
    match = anchorPattern.match(text);
    if (match.hasMatch()) {
        int anchors = 0;
        auto *surface = liveSurface(match.captured(1));
        if (surface != nullptr && parseInteger(match.captured(2), &anchors)) {
            surface->pendingState.anchors = anchors;
        } else if (surface != nullptr) {
            m_evidence.protocolAmbiguous = true;
        }
        return true;
    }
    match = edgePattern.match(text);
    if (match.hasMatch()) {
        int edge = 0;
        auto *surface = liveSurface(match.captured(1));
        if (surface != nullptr && parseInteger(match.captured(2), &edge)) {
            surface->pendingState.exclusiveEdge = edge;
        } else if (surface != nullptr) {
            m_evidence.protocolAmbiguous = true;
        }
        return true;
    }
    match = zonePattern.match(text);
    if (match.hasMatch()) {
        int zone = 0;
        auto *surface = liveSurface(match.captured(1));
        if (surface != nullptr && parseInteger(match.captured(2), &zone)) {
            surface->pendingState.exclusiveZone = zone;
        } else if (surface != nullptr) {
            m_evidence.protocolAmbiguous = true;
        }
        return true;
    }
    match = layerPattern.match(text);
    if (match.hasMatch()) {
        int layer = 0;
        auto *surface = liveSurface(match.captured(1));
        if (surface != nullptr && parseInteger(match.captured(2), &layer)) {
            surface->pendingState.layer = layer;
        } else if (surface != nullptr) {
            m_evidence.protocolAmbiguous = true;
        }
        return true;
    }
    return false;
}

bool ShellSurfaceProtocolTrace::parseConfigureHandshake(const QString &text)
{
    static const QRegularExpression configurePattern(serverEventPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.configure\(([0-9]{1,10}),\s*([0-9]{1,10}),\s*([0-9]{1,10})\))")));
    static const QRegularExpression acknowledgePattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.ack_configure\(([0-9]{1,10})\))")));
    auto match = configurePattern.match(text);
    if (match.hasMatch()) {
        int width = 0;
        int height = 0;
        auto *surface = liveSurface(match.captured(1));
        if (surface == nullptr || !isUnsigned32(match.captured(2), true) ||
            !parseInteger(match.captured(3), &width) ||
            !parseInteger(match.captured(4), &height)) {
            if (surface != nullptr) {
                m_evidence.protocolAmbiguous = true;
            }
            return true;
        }
        const QString serial = match.captured(2);
        if (surface->configurationsBySerial.contains(serial)) {
            m_evidence.protocolAmbiguous = true;
            return true;
        }
        if (surface->configurationsBySerial.size() >= maximumConfigureRecordsPerSurface) {
            invalidateInput();
            return true;
        }
        LayerSurfaceConfigureEvidence configuration;
        configuration.serial = serial;
        configuration.configuredSize = QSize(width, height);
        configuration.committedEpoch = surface->committedEpoch;
        configuration.committedState = surface->committedState;
        configuration.configureOrder = nextObservationOrder();
        if (configuration.committedEpoch <= 0) {
            m_evidence.protocolAmbiguous = true;
        }
        surface->configurationsBySerial.insert(serial, std::move(configuration));
        return true;
    }

    match = acknowledgePattern.match(text);
    if (match.hasMatch()) {
        auto *surface = liveSurface(match.captured(1));
        if (surface == nullptr || !isUnsigned32(match.captured(2), true)) {
            if (surface != nullptr) {
                m_evidence.protocolAmbiguous = true;
            }
            return true;
        }
        auto configuration = surface->configurationsBySerial.find(match.captured(2));
        if (configuration == surface->configurationsBySerial.end() ||
            configuration->acknowledgeOrder.has_value()) {
            m_evidence.protocolAmbiguous = true;
            return true;
        }
        configuration->acknowledgeOrder = nextObservationOrder();
        surface->lastAcknowledgedConfigureSerial = match.captured(2);
        return true;
    }
    return false;
}

bool ShellSurfaceProtocolTrace::parseSurfaceTransaction(const QString &text)
{
    static const QRegularExpression attachPattern(clientRequestPattern(QStringLiteral(
        R"(wl_surface#([0-9]{1,10})\.attach\((?:wl_buffer#([0-9]{1,10})|nil),\s*-?[0-9]{1,10},\s*-?[0-9]{1,10}\))")));
    static const QRegularExpression commitPattern(clientRequestPattern(QStringLiteral(
        R"(wl_surface#([0-9]{1,10})\.commit\(\))")));
    auto match = attachPattern.match(text);
    if (match.hasMatch()) {
        if (!isUnsigned32(match.captured(1), false) ||
            (!match.captured(2).isEmpty() && !isUnsigned32(match.captured(2), false))) {
            m_evidence.protocolAmbiguous = true;
            return true;
        }
        auto *surface = liveSurfaceByWaylandId(match.captured(1));
        if (surface != nullptr) {
            surface->pendingAttachmentObserved = true;
            surface->pendingBufferId = match.captured(2).isEmpty()
                ? std::optional<QString>{}
                : std::optional<QString>{match.captured(2)};
            surface->pendingAttachmentOrder = nextObservationOrder();
            surface->pendingConfigureSerial = surface->pendingBufferId
                ? surface->lastAcknowledgedConfigureSerial
                : std::optional<QString>{};
        }
        return true;
    }
    match = commitPattern.match(text);
    if (match.hasMatch()) {
        auto *surface = liveSurfaceByWaylandId(match.captured(1));
        if (surface != nullptr) {
            commitSurface(*surface, nextObservationOrder());
        }
        return true;
    }
    return false;
}

bool ShellSurfaceProtocolTrace::parseDestruction(const QString &text)
{
    static const QRegularExpression roleDestroyPattern(clientRequestPattern(QStringLiteral(
        R"(zwlr_layer_surface_v1#([0-9]{1,10})\.destroy\(\))")));
    static const QRegularExpression surfaceDestroyPattern(clientRequestPattern(QStringLiteral(
        R"(wl_surface#([0-9]{1,10})\.destroy\(\))")));
    auto match = roleDestroyPattern.match(text);
    if (match.hasMatch()) {
        auto *surface = findSurface(match.captured(1));
        if (surface == nullptr || surface->roleDestroyed) {
            m_evidence.protocolAmbiguous = true;
        } else {
            surface->roleDestroyed = true;
            surface->mapped = false;
        }
        return true;
    }
    match = surfaceDestroyPattern.match(text);
    if (match.hasMatch()) {
        auto *surface = findSurfaceByWaylandId(match.captured(1));
        if (surface == nullptr) {
            recordPreRoleSurfaceActivity(match.captured(1));
        } else if (surface->surfaceDestroyed) {
            m_evidence.protocolAmbiguous = true;
        } else {
            surface->surfaceDestroyed = true;
            surface->mapped = false;
        }
        return true;
    }
    return false;
}

} // namespace QindaQt::Test
