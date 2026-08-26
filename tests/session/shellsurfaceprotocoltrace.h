// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QSize>
#include <QString>

#include <optional>

namespace QindaQt::Test {

struct LayerSurfaceRoleState {
    std::optional<int> layer;
    std::optional<int> anchors;
    std::optional<int> exclusiveEdge;
    std::optional<int> exclusiveZone;
    std::optional<QSize> desiredSize;

    [[nodiscard]] bool isComplete() const noexcept;
    [[nodiscard]] QJsonObject toJson() const;

    friend bool operator==(const LayerSurfaceRoleState &,
                           const LayerSurfaceRoleState &) = default;
};

struct LayerSurfaceConfigureEvidence {
    QString serial;
    QSize configuredSize;
    int committedEpoch = 0;
    LayerSurfaceRoleState committedState;
    int configureOrder = 0;
    std::optional<int> acknowledgeOrder;

    [[nodiscard]] bool wasAcknowledgedAfterConfigure() const noexcept;
    [[nodiscard]] QJsonObject toJson() const;
};

struct LayerSurfaceMappingEvidence {
    int commitEpoch = 0;
    int attachOrder = 0;
    int commitOrder = 0;
    QString bufferId;
    QString configureSerial;
    int configureCommittedEpoch = 0;
    LayerSurfaceRoleState committedState;

    [[nodiscard]] bool isCausallyMapped() const noexcept;
    [[nodiscard]] QJsonObject toJson() const;
};

struct LayerSurfaceProtocolEvidence {
    QString roleId;
    QString waylandSurfaceId;
    QString outputId;
    QString scope;
    int requestCount = 0;
    std::optional<int> initialLayer;
    LayerSurfaceRoleState pendingState;
    LayerSurfaceRoleState committedState;
    int committedEpoch = 0;
    QHash<QString, LayerSurfaceConfigureEvidence> configurationsBySerial;
    std::optional<QString> lastAcknowledgedConfigureSerial;
    bool pendingAttachmentObserved = false;
    std::optional<QString> pendingBufferId;
    std::optional<QString> pendingConfigureSerial;
    int pendingAttachmentOrder = 0;
    bool mapped = false;
    std::optional<LayerSurfaceMappingEvidence> activeBufferMapping;
    bool roleDestroyed = false;
    bool surfaceDestroyed = false;

    [[nodiscard]] bool hasMappedBufferEpoch() const noexcept;
    [[nodiscard]] bool isCompleteMappedRole() const noexcept;
    [[nodiscard]] QJsonObject toJson() const;
};

struct ShellSurfaceProtocolEvidence {
    QHash<QString, LayerSurfaceProtocolEvidence> surfacesByRoleId;
    bool inputTruncated = false;
    bool identityAmbiguous = false;
    bool protocolAmbiguous = false;

    [[nodiscard]] bool isUsable() const noexcept;
    [[nodiscard]] bool provesMappedSurfaces(int expectedCount) const;
    [[nodiscard]] QJsonObject toJson() const;
};

class ShellSurfaceProtocolTrace final {
public:
    // WAYLAND_DEBUG is a test-only wire observer. This parser recognizes
    // exact role-state commits and configure/ack/buffer-map causality. Raw
    // input, captures, identities, configurations, and counters are bounded.
    void ingest(const QByteArray &chunk);
    void finish();

    [[nodiscard]] const ShellSurfaceProtocolEvidence &evidence() const noexcept;

private:
    void invalidateInput();
    void parseLine(const QByteArray &line);
    bool parseRoleCreation(const QString &text);
    bool parseRoleStateRequest(const QString &text);
    bool parseConfigureHandshake(const QString &text);
    bool parseSurfaceTransaction(const QString &text);
    bool parseDestruction(const QString &text);
    LayerSurfaceProtocolEvidence *findSurface(const QString &roleId);
    LayerSurfaceProtocolEvidence *findSurfaceByWaylandId(const QString &surfaceId);
    LayerSurfaceProtocolEvidence *liveSurface(const QString &roleId);
    LayerSurfaceProtocolEvidence *liveSurfaceByWaylandId(const QString &surfaceId);
    LayerSurfaceProtocolEvidence *recordRequestedSurface(const QString &roleId,
                                                         const QString &surfaceId);
    void recordPreRoleSurfaceActivity(const QString &surfaceId);
    void commitSurface(LayerSurfaceProtocolEvidence &surface, int commitOrder);
    [[nodiscard]] int nextObservationOrder();

    QByteArray m_pending;
    QSet<QString> m_preRoleActivitySurfaceIds;
    ShellSurfaceProtocolEvidence m_evidence;
    qsizetype m_totalAcceptedBytes = 0;
    int m_observationOrder = 0;
    bool m_ignoreInput = false;
};

} // namespace QindaQt::Test
