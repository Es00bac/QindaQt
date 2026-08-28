// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QSet>
#include <QtCore/QVariantList>
#include <optional>

namespace QindaQt::Shell::AudioApplet {

// Presentation bounds are deliberately far below the Audio1 protocol limits
// (128 devices / 256 streams). The panel applet is a quick surface, not the
// complete Settings route; anything beyond these rows is summarized as an
// overflow count instead of scrolling forever.
inline constexpr int kMaxDeviceRows = 8;
inline constexpr int kMaxStreamRows = 8;

enum class Phase {
    Loading,
    Ready,
    Degraded,
    Unavailable,
};

// One bounded, QML-readable output-device row. Values are presentation
// projections of one validated Audio1 Device; they never carry handles,
// D-Bus objects, or client authority.
class DeviceRow {
    Q_GADGET
    Q_PROPERTY(quint64 serial READ serial CONSTANT)
    Q_PROPERTY(QString label READ label CONSTANT)
    Q_PROPERTY(bool isOutput READ isOutput CONSTANT)
    Q_PROPERTY(bool isDefault READ isDefault CONSTANT)
    Q_PROPERTY(double volume READ volume CONSTANT)
    Q_PROPERTY(bool volumeKnown READ volumeKnown CONSTANT)
    Q_PROPERTY(bool muted READ muted CONSTANT)
    Q_PROPERTY(bool muteKnown READ muteKnown CONSTANT)
    Q_PROPERTY(bool canSetVolume READ canSetVolume CONSTANT)
    Q_PROPERTY(bool canSetMute READ canSetMute CONSTANT)
    Q_PROPERTY(bool pending READ pending CONSTANT)

public:
    quint64 m_serial = 0;
    QString m_label;
    bool m_isOutput = true;
    bool m_isDefault = false;
    double m_volume = 0.0;
    bool m_volumeKnown = false;
    bool m_muted = false;
    bool m_muteKnown = false;
    bool m_canSetVolume = false;
    bool m_canSetMute = false;
    bool m_pending = false;

    [[nodiscard]] quint64 serial() const noexcept { return m_serial; }
    [[nodiscard]] const QString &label() const noexcept { return m_label; }
    [[nodiscard]] bool isOutput() const noexcept { return m_isOutput; }
    [[nodiscard]] bool isDefault() const noexcept { return m_isDefault; }
    [[nodiscard]] double volume() const noexcept { return m_volume; }
    [[nodiscard]] bool volumeKnown() const noexcept { return m_volumeKnown; }
    [[nodiscard]] bool muted() const noexcept { return m_muted; }
    [[nodiscard]] bool muteKnown() const noexcept { return m_muteKnown; }
    [[nodiscard]] bool canSetVolume() const noexcept { return m_canSetVolume; }
    [[nodiscard]] bool canSetMute() const noexcept { return m_canSetMute; }
    [[nodiscard]] bool pending() const noexcept { return m_pending; }
};

// One bounded, QML-readable application-stream row. Move targets are not part
// of this slice, so no target identity is projected.
class StreamRow {
    Q_GADGET
    Q_PROPERTY(quint64 serial READ serial CONSTANT)
    Q_PROPERTY(QString label READ label CONSTANT)
    Q_PROPERTY(bool isPlayback READ isPlayback CONSTANT)
    Q_PROPERTY(double volume READ volume CONSTANT)
    Q_PROPERTY(bool volumeKnown READ volumeKnown CONSTANT)
    Q_PROPERTY(bool muted READ muted CONSTANT)
    Q_PROPERTY(bool muteKnown READ muteKnown CONSTANT)
    Q_PROPERTY(bool canSetVolume READ canSetVolume CONSTANT)
    Q_PROPERTY(bool canSetMute READ canSetMute CONSTANT)
    Q_PROPERTY(bool pending READ pending CONSTANT)

public:
    quint64 m_serial = 0;
    QString m_label;
    bool m_isPlayback = true;
    double m_volume = 0.0;
    bool m_volumeKnown = false;
    bool m_muted = false;
    bool m_muteKnown = false;
    bool m_canSetVolume = false;
    bool m_canSetMute = false;
    bool m_pending = false;

    [[nodiscard]] quint64 serial() const noexcept { return m_serial; }
    [[nodiscard]] const QString &label() const noexcept { return m_label; }
    [[nodiscard]] bool isPlayback() const noexcept { return m_isPlayback; }
    [[nodiscard]] double volume() const noexcept { return m_volume; }
    [[nodiscard]] bool volumeKnown() const noexcept { return m_volumeKnown; }
    [[nodiscard]] bool muted() const noexcept { return m_muted; }
    [[nodiscard]] bool muteKnown() const noexcept { return m_muteKnown; }
    [[nodiscard]] bool canSetVolume() const noexcept { return m_canSetVolume; }
    [[nodiscard]] bool canSetMute() const noexcept { return m_canSetMute; }
    [[nodiscard]] bool pending() const noexcept { return m_pending; }
};

// Pure projection of one Audio1 snapshot plus the controller's pending-request
// set into the bounded applet presentation state. No QObject, no transport,
// no snapshot retention beyond one build.
class AudioAppletModel {
public:
    // AGENT-GUARD: A snapshot whose wireValid flag is false must never reach
    // rows or labels; the projection forces Unavailable regardless of the
    // caller-supplied phase. The Audio1 client already validates snapshots,
    // but this projection is the last consumer-side barrier.
    [[nodiscard]] static AudioAppletModel
    project(Phase phase, const QString &phaseReasonCode,
            const Audio::Snapshot *snapshot,
            const QSet<quint64> &pendingSerials);

    // Non-finite values are rejected (nullopt) rather than clamped, matching
    // the Audio1 rule that out-of-domain levels fail closed. Finite values are
    // clamped into the inclusive [0.0, 1.0] presentation range.
    [[nodiscard]] static std::optional<double>
    clampVolumeLevel(double volume) noexcept;

    [[nodiscard]] Phase phase() const noexcept { return m_phase; }
    [[nodiscard]] const QString &phaseReasonCode() const noexcept
    {
        return m_phaseReasonCode;
    }
    [[nodiscard]] const QString &defaultOutputLabel() const noexcept
    {
        return m_defaultOutputLabel;
    }
    [[nodiscard]] const QString &defaultInputLabel() const noexcept
    {
        return m_defaultInputLabel;
    }
    [[nodiscard]] const QList<DeviceRow> &deviceRows() const noexcept
    {
        return m_deviceRows;
    }
    [[nodiscard]] const QList<StreamRow> &streamRows() const noexcept
    {
        return m_streamRows;
    }
    [[nodiscard]] int overflowDeviceCount() const noexcept
    {
        return m_overflowDeviceCount;
    }
    [[nodiscard]] int overflowStreamCount() const noexcept
    {
        return m_overflowStreamCount;
    }

private:
    Phase m_phase = Phase::Loading;
    QString m_phaseReasonCode;
    QString m_defaultOutputLabel;
    QString m_defaultInputLabel;
    QList<DeviceRow> m_deviceRows;
    QList<StreamRow> m_streamRows;
    int m_overflowDeviceCount = 0;
    int m_overflowStreamCount = 0;
};

} // namespace QindaQt::Shell::AudioApplet

Q_DECLARE_METATYPE(QindaQt::Shell::AudioApplet::DeviceRow)
Q_DECLARE_METATYPE(QindaQt::Shell::AudioApplet::StreamRow)
