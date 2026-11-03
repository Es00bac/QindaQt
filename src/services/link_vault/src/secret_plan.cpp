// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/input_frames.h>
#include <qindaqt/services/link_input/text_typer.h>
#include <qindaqt/services/link_vault/secret_plan.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include <cmath>

namespace QindaQt::Link {
namespace {

// The closed named-key set of `vault.type` and the keysym name each maps to.
std::optional<qint32> keyFor(const QString &name)
{
    static const struct { const char *wire; const char *keysymName; } table[] = {
        {"Tab", "Tab"}, {"Enter", "Return"}, {"Escape", "Escape"},
        {"Left", "Left"}, {"Right", "Right"}, {"Up", "Up"}, {"Down", "Down"}};
    for (const auto &row : table) {
        if (name == QLatin1String(row.wire)) return InputFrames::namedKeysym(QLatin1String(row.keysymName));
    }
    return std::nullopt;
}

bool onlyKeys(const QJsonObject &o, const QString &allowed)
{
    return o.size() == 1 && o.contains(allowed);
}

} // namespace

std::optional<SecretPlan> SecretPlan::parse(const QJsonObject &body, QString *token)
{
    const QJsonValue t = body.value(QStringLiteral("token"));
    const QJsonValue steps = body.value(QStringLiteral("steps"));
    if (body.size() != 2 || !t.isString() || !steps.isArray()) return std::nullopt;
    const QJsonArray arr = steps.toArray();
    if (arr.isEmpty() || arr.size() > kMaxSteps) return std::nullopt;
    SecretPlan plan;
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) return std::nullopt;
        const QJsonObject o = v.toObject();
        Step step;
        if (onlyKeys(o, QStringLiteral("text"))) {
            const QJsonValue text = o.value(QStringLiteral("text"));
            if (!text.isString()) return std::nullopt;
            const QString s = text.toString();
            if (s.isEmpty() || s.toUtf8().size() > kMaxTextBytes) return std::nullopt;
            QList<uint> cps = s.toUcs4();
            for (uint cp : std::as_const(cps)) {
                const bool skip = cp < 0x20 || cp == 0x7f || (cp >= 0x80 && cp <= 0x9f) || cp > 0x10ffff
                    || (cp >= 0xd800 && cp <= 0xdfff);
                if (!skip) step.symbols.append(InputFrames::keysymForCodepoint(cp));
            }
            std::fill(cps.begin(), cps.end(), 0u);
            if (step.symbols.isEmpty()) continue;
            step.kind = Kind::Text;
        } else if (onlyKeys(o, QStringLiteral("key"))) {
            const QJsonValue key = o.value(QStringLiteral("key"));
            const auto sym = key.isString() ? keyFor(key.toString()) : std::nullopt;
            if (!sym) return std::nullopt;
            step.kind = Kind::Key;
            step.keysym = *sym;
        } else if (onlyKeys(o, QStringLiteral("delayMs"))) {
            const QJsonValue d = o.value(QStringLiteral("delayMs"));
            if (!d.isDouble() || d.toDouble() != std::floor(d.toDouble()) || d.toDouble() < 0
                || d.toDouble() > kMaxDelayMs) {
                return std::nullopt;
            }
            step.kind = Kind::Delay;
            step.delayMs = d.toInt();
        } else {
            return std::nullopt;
        }
        plan.m_steps.append(std::move(step));
    }
    // A plan of only delays types nothing and is refused.
    bool types = false;
    for (const Step &s : std::as_const(plan.m_steps)) types = types || s.kind != Kind::Delay;
    if (!types) return std::nullopt;
    *token = t.toString();
    return plan;
}

void SecretPlan::wipe()
{
    for (Step &s : m_steps) TextTyper::wipe(s.symbols);
    m_steps.clear();
}

} // namespace QindaQt::Link
