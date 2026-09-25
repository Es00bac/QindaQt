// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility_protocol/wire_limits.h"

#include <QByteArray>
#include <QMetaObject>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

namespace KWin {
class LogicalOutput;
}

namespace QindaQt::Compositor::KWinIntegration {

// One advertised hardware mode in untransformed pixels. Size plus refresh is
// the identity Display1's writer resolves back to a KWin output-device mode.
struct OutputInventoryMode final
{
    QSize pixelSize;
    quint32 refreshRateMilliHz = 0;
    bool preferred = false;

    friend bool operator==(const OutputInventoryMode &,
                           const OutputInventoryMode &) = default;
};

struct OutputInventoryEntry final
{
    QString name;
    QRectF geometry;
    QRect visibilityGeometry;
    qreal scale = 1.0;
    quint32 refreshRateMilliHz = 0;
    QString transform;
    // BackendOutput owns the live enabled state. Keep it in the immutable
    // inventory so a connected but disabled connector can be enabled through
    // Display1 instead of being presented as an active screen.
    bool enabled = true;
    bool internal = false;
    QString uuid;
    quint32 priority = 0;
    QSize physicalSizeMillimeters;
    QString manufacturer;
    QString model;
    // Current hardware mode in untransformed pixels. A mirrored output's
    // logical geometry is its source's, and KWin fits it with a synthetic
    // scale, so geometry * scale does not recover the real mode.
    QSize modePixelSize;
    // Advertised modes in KWin order, deduplicated by size and refresh.
    QVector<OutputInventoryMode> modes;
    // Connector name of the output this one mirrors; empty when extended.
    // Must name another entry in the same projection.
    QString replicationSource;

    friend bool operator==(const OutputInventoryEntry &,
                           const OutputInventoryEntry &) = default;
};

enum class OutputInventoryPublishResult {
    Published,
    Unchanged,
    Rejected,
    GenerationExhausted,
};

struct OutputGenerationSeed final
{
    quint64 value = 0;
};

// Retains one validated output projection and advances its generation only
// when the complete canonical projection changes. The store owns values only;
// callers serialize access on one thread and decide when to emit IPC hints.
class OutputInventoryStore final
{
public:
    // AGENT-CONTRACT: Outputs and ShellVisibilitySnapshot observe one KWin
    // projection. Their count/scale bounds must stay identical or one method
    // can publish a generation the other can never represent.
    static constexpr qsizetype MaxOutputs =
        ShellVisibilityProtocol::WireLimits::MaxOutputs;
    static constexpr qsizetype MaxNameCharacters =
        ShellVisibilityProtocol::WireLimits::MaxIdentifierCharacters;
    static constexpr qsizetype MaxNameUtf8Bytes = MaxNameCharacters * 4;
    static constexpr qsizetype MaxUuidUtf8Bytes = 128;
    static constexpr qsizetype MaxManufacturerUtf8Bytes = 128;
    static constexpr qsizetype MaxModelUtf8Bytes = 256;
    static constexpr int MaxPhysicalDimensionMillimeters = 10'000;
    static constexpr qreal MaxLogicalCoordinateMagnitude = 1'000'000.0;
    static constexpr qreal MaximumScale =
        ShellVisibilityProtocol::WireLimits::MaxOutputScale;
    // AGENT-CONTRACT: equals Display1 kMaxModesPerOutput
    // (display_protocol/display_limits.h). The sampler truncates to it while
    // keeping the current mode, so Display1 never rejects a long EDID list.
    static constexpr qsizetype MaxModes = 128;
    // AGENT-CONTRACT: equals Display1 kMaxPixelDimension/kMaxRefreshMilliHertz.
    static constexpr int MaxModePixelDimension = 16'384;
    static constexpr quint32 MaxRefreshRateMilliHz = 1'000'000;

    explicit OutputInventoryStore(OutputGenerationSeed seed = {});

    [[nodiscard]] OutputInventoryPublishResult publish(
        const QVector<OutputInventoryEntry> &candidate,
        QString *error = nullptr);
    [[nodiscard]] const QByteArray &responseJson() const noexcept;
    [[nodiscard]] const QVector<OutputInventoryEntry> &entries() const noexcept;
    [[nodiscard]] quint64 generation() const noexcept;
    [[nodiscard]] bool available() const noexcept;

private:
    QByteArray m_responseJson;
    QVector<OutputInventoryEntry> m_entries;
    quint64 m_generation = 0;
    bool m_available = false;
};

// GUI-thread adapter over KWin's borrowed LogicalOutput/BackendOutput objects.
// It owns no KWin output; returned entries and JSON are immutable value copies
// valid until the next accepted inventory generation. visibilityGeometry is
// KWin's exact integral shell boundary, not an independent QRectF rounding.
class KWinOutputInventory final : public QObject
{
    Q_OBJECT

public:
    explicit KWinOutputInventory(QObject *parent = nullptr);

    [[nodiscard]] const QByteArray &responseJson() const noexcept;
    [[nodiscard]] const QVector<OutputInventoryEntry> &entries() const noexcept;
    [[nodiscard]] quint64 generation() const noexcept;
    [[nodiscard]] bool available() const noexcept;

Q_SIGNALS:
    void inventoryChanged();

private:
    void rebuildOutputConnections();
    void scheduleRefresh();
    void refresh();
    [[nodiscard]] QVector<OutputInventoryEntry> sample(QString *error) const;

    OutputInventoryStore m_store;
    QVector<QMetaObject::Connection> m_outputConnections;
    bool m_refreshScheduled = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
