// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <optional>
#include <variant>

namespace QindaQt::Link::InputFrames {

// Bounds from the protocol page, "Remote input (input.*)".
inline constexpr double kMaxPointerDelta = 1000.0;
inline constexpr double kMaxSmoothScroll = 500.0;
inline constexpr int kMaxDiscreteSteps = 20;
inline constexpr int kMaxTextBytes = 4096;

enum class KeyState { Down, Up, Tap };
enum class ButtonState { Down, Up, Click };

struct Pointer { double dx = 0, dy = 0; };
struct Button { qint32 evdev = 0; ButtonState state = ButtonState::Click; };
struct Scroll { double dx = 0, dy = 0; bool discrete = false; bool finish = false; };
struct Key { qint32 keysym = 0; KeyState state = KeyState::Tap; };
struct Text { QString text; };
using Event = std::variant<Pointer, Button, Scroll, Key, Text>;

// Pure parsing of an input.* body. Returns nullopt for any wrong type, missing
// field, non-finite number or out-of-range value (the caller answers
// bad-request or drops the frame). Never logs and never keeps the body.
// `type` is the full frame type, for example "input.pointer".
[[nodiscard]] std::optional<Event> parse(const QString &type, const QJsonObject &body);

// Linux evdev codes for the closed button set.
[[nodiscard]] std::optional<qint32> buttonCode(const QString &name);
// The closed key-name set; nullopt when the name is unknown.
[[nodiscard]] std::optional<qint32> namedKeysym(const QString &name);
// Allowed X11 keysym ranges (printable Latin-1, function/modifier, XF86, Unicode).
[[nodiscard]] bool validKeysym(qint64 keysym);
// Keysym for one Unicode scalar: itself up to U+00FF, else 0x01000000 | cp.
[[nodiscard]] qint32 keysymForCodepoint(char32_t cp);

} // namespace QindaQt::Link::InputFrames
