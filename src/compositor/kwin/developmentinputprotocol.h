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
    Key,
    Button,
};

enum class DevelopmentInputKey {
    LeftMeta,
    LeftShift,
    Down,
    Enter,
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
