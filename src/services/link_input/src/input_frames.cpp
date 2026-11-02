// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/input_frames.h>

#include <QtCore/QHash>

#include <cmath>

namespace QindaQt::Link::InputFrames {
namespace {

std::optional<double> number(const QJsonObject &b, const char *key, double limit)
{
    const QJsonValue v = b.value(QLatin1String(key));
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || std::abs(v.toDouble()) > limit) return std::nullopt;
    return v.toDouble();
}

}

std::optional<qint32> buttonCode(const QString &name)
{
    if (name == QLatin1String("left")) return 272;   // BTN_LEFT
    if (name == QLatin1String("right")) return 273;  // BTN_RIGHT
    if (name == QLatin1String("middle")) return 274; // BTN_MIDDLE
    return std::nullopt;
}

std::optional<qint32> namedKeysym(const QString &name)
{
    static const QHash<QString, qint32> table = [] {
        QHash<QString, qint32> t{
            {QStringLiteral("Escape"), 0xff1b}, {QStringLiteral("Tab"), 0xff09}, {QStringLiteral("Return"), 0xff0d},
            {QStringLiteral("BackSpace"), 0xff08}, {QStringLiteral("Delete"), 0xffff}, {QStringLiteral("Insert"), 0xff63},
            {QStringLiteral("Home"), 0xff50}, {QStringLiteral("End"), 0xff57}, {QStringLiteral("PageUp"), 0xff55},
            {QStringLiteral("PageDown"), 0xff56}, {QStringLiteral("Left"), 0xff51}, {QStringLiteral("Up"), 0xff52},
            {QStringLiteral("Right"), 0xff53}, {QStringLiteral("Down"), 0xff54}, {QStringLiteral("Space"), 0x20},
            {QStringLiteral("Menu"), 0xff67}, {QStringLiteral("Print"), 0xff61},
            {QStringLiteral("Shift"), 0xffe1}, {QStringLiteral("Ctrl"), 0xffe3}, {QStringLiteral("Alt"), 0xffe9},
            {QStringLiteral("Meta"), 0xffeb}, {QStringLiteral("CapsLock"), 0xffe5}};
        for (int i = 1; i <= 12; ++i) t.insert(QStringLiteral("F%1").arg(i), 0xffbe + i - 1);
        return t;
    }();
    const auto it = table.constFind(name);
    return it == table.constEnd() ? std::nullopt : std::optional<qint32>(it.value());
}

bool validKeysym(qint64 k)
{
    return (k >= 0x20 && k <= 0x7e) || (k >= 0xa0 && k <= 0xff) || (k >= 0xfe00 && k <= 0xffff)
        || (k >= 0x1008ff00 && k <= 0x1008ffff)
        || (k >= 0x01000100 && k <= 0x0110ffff && !(k >= 0x0100d800 && k <= 0x0100dfff));
}

qint32 keysymForCodepoint(char32_t cp)
{
    return cp <= 0xff ? qint32(cp) : qint32(0x01000000u | cp);
}

std::optional<Event> parse(const QString &type, const QJsonObject &b)
{
    if (type == QLatin1String("input.pointer")) {
        const auto dx = number(b, "dx", kMaxPointerDelta);
        const auto dy = number(b, "dy", kMaxPointerDelta);
        if (!dx || !dy) return std::nullopt;
        return Pointer{*dx, *dy};
    }
    if (type == QLatin1String("input.button")) {
        if (!b.value(QLatin1String("button")).isString() || !b.value(QLatin1String("state")).isString()) return std::nullopt;
        const auto code = buttonCode(b.value(QLatin1String("button")).toString());
        const QString s = b.value(QLatin1String("state")).toString();
        if (!code) return std::nullopt;
        if (s == QLatin1String("down")) return Button{*code, ButtonState::Down};
        if (s == QLatin1String("up")) return Button{*code, ButtonState::Up};
        if (s == QLatin1String("click")) return Button{*code, ButtonState::Click};
        return std::nullopt;
    }
    if (type == QLatin1String("input.scroll")) {
        const QJsonValue mode = b.value(QLatin1String("mode"));
        const bool discrete = mode.isString() && mode.toString() == QLatin1String("discrete");
        if (!mode.isString() || (!discrete && mode.toString() != QLatin1String("smooth"))) return std::nullopt;
        const auto dx = number(b, "dx", discrete ? kMaxDiscreteSteps : kMaxSmoothScroll);
        const auto dy = number(b, "dy", discrete ? kMaxDiscreteSteps : kMaxSmoothScroll);
        if (!dx || !dy) return std::nullopt;
        if (discrete && (*dx != std::floor(*dx) || *dy != std::floor(*dy))) return std::nullopt;
        const QJsonValue fin = b.value(QLatin1String("end"));
        if (!fin.isUndefined() && !fin.isBool()) return std::nullopt;
        return Scroll{*dx, *dy, discrete, fin.toBool(false)};
    }
    if (type == QLatin1String("input.key")) {
        const QJsonValue sym = b.value(QLatin1String("keysym"));
        const QJsonValue name = b.value(QLatin1String("name"));
        const QJsonValue st = b.value(QLatin1String("state"));
        if (sym.isUndefined() == name.isUndefined() || !st.isString()) return std::nullopt;
        qint32 keysym = 0;
        if (!sym.isUndefined()) {
            if (!sym.isDouble() || sym.toDouble() != std::floor(sym.toDouble()) || !validKeysym(qint64(sym.toDouble()))) return std::nullopt;
            keysym = qint32(sym.toDouble());
        } else {
            if (!name.isString()) return std::nullopt;
            const auto n = namedKeysym(name.toString());
            if (!n) return std::nullopt;
            keysym = *n;
        }
        const QString s = st.toString();
        if (s == QLatin1String("down")) return Key{keysym, KeyState::Down};
        if (s == QLatin1String("up")) return Key{keysym, KeyState::Up};
        if (s == QLatin1String("tap")) return Key{keysym, KeyState::Tap};
        return std::nullopt;
    }
    if (type == QLatin1String("input.text")) {
        const QJsonValue t = b.value(QLatin1String("text"));
        if (!t.isString() || t.toString().isEmpty() || t.toString().toUtf8().size() > kMaxTextBytes) return std::nullopt;
        return Text{t.toString()};
    }
    return std::nullopt;
}

} // namespace QindaQt::Link::InputFrames
