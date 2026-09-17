// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

enum class DevelopmentInputEventType {
    PointerAbsolute,
    PointerRelative,
    Key,
    Button,
    // A wheel step through KWin's normal axis pipeline (ADR-0191 rows). The
    // logical delta follows libinput's sign convention: negative turns the
    // wheel away from the user. deltaV120 is derived as delta * 8, so one
    // 15-unit notch is one 120-unit step.
    PointerAxis,
};

enum class DevelopmentInputAxis {
    Vertical,
    Horizontal,
};

enum class DevelopmentInputKey {
    LeftMeta,
    LeftAlt,
    LeftControl,
    LeftShift,
    F1,
    F11,
    C,
    N,
    Tab,
    Escape,
    Space,
    Up,
    Down,
    Left,
    Right,
    Enter,
    V,
    W,
    VolumeUp,
    Print,
};

enum class DevelopmentInputButton {
    Left,
    Right,
};

struct DevelopmentInputEvent final
{
    DevelopmentInputEventType type = DevelopmentInputEventType::PointerAbsolute;
    QPointF position;
    DevelopmentInputKey key = DevelopmentInputKey::LeftMeta;
    bool pressed = false;
    DevelopmentInputButton button = DevelopmentInputButton::Left;
    DevelopmentInputAxis axis = DevelopmentInputAxis::Vertical;
    qreal axisDelta = 0.0;
};

struct DevelopmentInputBatch final
{
    QVector<DevelopmentInputEvent> events;
};

struct DevelopmentInputFailure final
{
    QString code;
    QString message;
};

class DevelopmentInputCodec final
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr qsizetype MaxEvents = 64;
    // This accommodates negative-coordinate monitor layouts while rejecting
    // coordinates whose magnitude cannot represent a plausible desktop.
    static constexpr qreal MaxLogicalCoordinateMagnitude = 1'000'000.0;
    // Relative motion is a bounded qualification primitive. A generous
    // 10k-logical-pixel delta covers nested pointer-lock probes without
    // accepting unbounded values into KWin's input pipeline.
    static constexpr qreal MaxRelativeDeltaMagnitude = 10'000.0;
    // One wheel notch is 15 logical units; a bounded burst covers every
    // nested roll-up/unroll probe without accepting unbounded scroll energy.
    static constexpr qreal MaxAxisDeltaMagnitude = 1'000.0;

    [[nodiscard]] static std::optional<DevelopmentInputBatch>
    parse(const QByteArray &requestJson, DevelopmentInputFailure *failure = nullptr);
};

class DevelopmentInputSink
{
public:
    virtual ~DevelopmentInputSink() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;
    // Called synchronously on KWin's compositor thread. Returning false means
    // the input chain disappeared between capability inspection and dispatch.
    [[nodiscard]] virtual bool inject(const DevelopmentInputBatch &batch) = 0;
};

class DevelopmentInputController final
{
public:
    // AGENT-CONTRACT: The sink is optional and non-owning. When present it
    // must outlive this controller; KWinControlEndpoint and its plugin-owned
    // injector preserve that order.
    DevelopmentInputController(bool mutationsEnabled, DevelopmentInputSink *sink);

    [[nodiscard]] QByteArray injectTestInput(const QByteArray &requestJson) const;
    [[nodiscard]] QJsonObject capabilities() const;

private:
    bool m_mutationsEnabled = false;
    DevelopmentInputSink *m_sink = nullptr;
};

[[nodiscard]] QString developmentInputDeviceId();

} // namespace QindaQt::Compositor::KWinIntegration
