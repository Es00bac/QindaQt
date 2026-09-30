// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/access_consent.h>
#include <QDBusMetaType>
#include <QSet>
#include <QRegularExpression>
namespace QindaQt::Services::Portal {
QDBusArgument &operator<<(QDBusArgument &a, const ChoiceOption &v) {
    a.beginStructure(); a << v.id << v.label; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, ChoiceOption &v) {
    a.beginStructure(); a >> v.id >> v.label; a.endStructure(); return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const AccessChoice &v) {
    a.beginStructure(); a << v.id << v.label << v.options << v.initial; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, AccessChoice &v) {
    a.beginStructure(); a >> v.id >> v.label >> v.options >> v.initial; a.endStructure(); return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const ChoiceValue &v) {
    a.beginStructure(); a << v.id << v.value; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, ChoiceValue &v) {
    a.beginStructure(); a >> v.id >> v.value; a.endStructure(); return a;
}
void registerAccessTypes() {
    qDBusRegisterMetaType<ChoiceOption>(); qDBusRegisterMetaType<ChoiceOptions>();
    qDBusRegisterMetaType<AccessChoice>(); qDBusRegisterMetaType<AccessChoices>();
    qDBusRegisterMetaType<ChoiceValue>(); qDBusRegisterMetaType<ChoiceValues>();
}
namespace {
bool text(const QString &s, qsizetype max = 4096) {
    if (s.size() > max) return false;
    for (const auto c : s) if (c.isNull() || (c.category() == QChar::Other_Control
        && c != QLatin1Char('\n') && c != QLatin1Char('\t'))) return false;
    return true;
}
bool id(const QString &s) { return !s.isEmpty() && text(s, 128); }
bool choicesValid(const AccessChoices &choices) {
    if (choices.size() > 16) return false;
    qsizetype totalText = 0;
    QSet<QString> ids;
    for (const auto &choice : choices) {
        if (!id(choice.id) || ids.contains(choice.id) || !text(choice.label, 512)
            || choice.options.size() > 32) return false;
        totalText += choice.id.size() + choice.label.size() + choice.initial.size();
        ids.insert(choice.id); QSet<QString> options;
        for (const auto &option : choice.options) {
            if (!id(option.id) || options.contains(option.id) || !text(option.label, 512)) return false;
            totalText += option.id.size() + option.label.size();
            if (totalText > 16000) return false;
            options.insert(option.id);
        }
        if (options.isEmpty()) {
            if (choice.initial != QStringLiteral("true") && choice.initial != QStringLiteral("false")) return false;
        } else if (!choice.initial.isEmpty() && !options.contains(choice.initial)) return false;
    }
    return true;
}
}
std::optional<AccessQuestion> accessQuestion(const QString &app, const QString &parent,
    const QString &title, const QString &subtitle, const QString &body, const QVariantMap &options) {
    if (!text(app, 255) || !text(title, 512) || !text(subtitle, 1024) || !text(body)
        || options.size() > 32) return std::nullopt;
    // xdg-foreign handles are opaque, not UUID/base64 identifiers. Bound and
    // reject controls without imposing a compositor-specific alphabet.
    if (!parent.isEmpty()) {
        if (!parent.startsWith(QStringLiteral("wayland:")) || parent.size() <= 8 || parent.size() > 512) return std::nullopt;
        for (const auto c : parent) if (c.isNull() || c.category() == QChar::Other_Control) return std::nullopt;
    }
    AccessQuestion q{app, parent, title, subtitle, body, QStringLiteral("Deny"), QStringLiteral("Allow"), {}, true, {}};
    for (const auto &key : {QStringLiteral("deny_label"), QStringLiteral("grant_label"), QStringLiteral("icon")}) {
        if (options.contains(key) && (options[key].metaType() != QMetaType::fromType<QString>()
            || !text(options[key].toString(), 512))) return std::nullopt;
    }
    if (options.contains(QStringLiteral("modal"))) {
        if (options[QStringLiteral("modal")].metaType() != QMetaType::fromType<bool>()) return std::nullopt;
        q.modal = options[QStringLiteral("modal")].toBool();
    }
    if (options.contains(QStringLiteral("deny_label"))) q.denyLabel = options[QStringLiteral("deny_label")].toString();
    if (options.contains(QStringLiteral("grant_label"))) q.grantLabel = options[QStringLiteral("grant_label")].toString();
    q.icon = options.value(QStringLiteral("icon")).toString();
    if (options.contains(QStringLiteral("choices"))) {
        const auto value = options[QStringLiteral("choices")];
        if (value.metaType() == QMetaType::fromType<AccessChoices>()) q.choices = value.value<AccessChoices>();
        else if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
            const auto argument = value.value<QDBusArgument>();
            if (argument.currentSignature() != QStringLiteral("a(ssa(ss)s)")) return std::nullopt;
            q.choices = qdbus_cast<AccessChoices>(argument);
        } else return std::nullopt;
    }
    return choicesValid(q.choices) ? std::optional(q) : std::nullopt;
}
bool validChoiceValues(const AccessQuestion &question, const ChoiceValues &values) {
    if (values.size() != question.choices.size()) return false;
    QSet<QString> seen;
    for (const auto &value : values) {
        if (seen.contains(value.id)) return false;
        seen.insert(value.id); bool found = false;
        for (const auto &choice : question.choices) {
            if (choice.id != value.id) continue;
            found = true;
            if (choice.options.isEmpty()) {
                if (value.value != QStringLiteral("true") && value.value != QStringLiteral("false")) return false;
            } else {
                bool optionFound = false;
                for (const auto &option : choice.options) optionFound |= option.id == value.value;
                if (!optionFound) return false;
            }
        }
        if (!found) return false;
    }
    return true;
}
} // namespace QindaQt::Services::Portal
