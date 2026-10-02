// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/idle_policy/shared_preferences.h>
#include <QMetaType>
namespace QindaQt::Session::IdlePolicy {
QStringList sharedIdleSettingsKeys() {
    auto keys = perSourceDisplayOffSettingsKeys();
    for (const auto &source : {QStringLiteral("ac"), QStringLiteral("battery"), QStringLiteral("lowBattery")})
        for (const auto &field : {QStringLiteral("dimEnabled"), QStringLiteral("dimSeconds"),
                QStringLiteral("lockBeforeDisplayOff"), QStringLiteral("suspendAction"), QStringLiteral("suspendSeconds")})
            keys << QStringLiteral("power.idle.%1.%2").arg(source, field);
    return keys;
}
std::optional<SharedIdlePreferences> sharedIdlePreferencesFor(
    const Services::SettingsClient::SettingsSnapshot &snapshot,
    const QString &owner, PowerSourceProfile source) {
    const auto display = displayOffPreferencesFor(snapshot, owner, source);
    if (!display) return std::nullopt;
    const auto prefix = QStringLiteral("power.idle.%1.").arg(powerSourceProfileKey(source));
    const auto value = [&](const char *field) { return snapshot.values.value(prefix + QString::fromLatin1(field)); };
    const auto seconds = [&](const char *field) -> std::optional<int> {
        const auto v = value(field); const auto type = v.metaType().id();
        if (type != QMetaType::Int && type != QMetaType::UInt && type != QMetaType::LongLong && type != QMetaType::ULongLong) return std::nullopt;
        bool ok = false; const auto n = v.toLongLong(&ok);
        if (!ok || n < 0 || n > 14400) return std::nullopt;
        return static_cast<int>(n);
    };
    const auto dim = value("dimEnabled"), lock = value("lockBeforeDisplayOff"), action = value("suspendAction");
    const auto dimSeconds = seconds("dimSeconds"), suspendSeconds = seconds("suspendSeconds");
    if (dim.metaType().id() != QMetaType::Bool || lock.metaType().id() != QMetaType::Bool ||
        action.metaType().id() != QMetaType::QString || !dimSeconds || !suspendSeconds ||
        !QStringList{QStringLiteral("none"), QStringLiteral("suspend"), QStringLiteral("hibernate")}.contains(action.toString())) return std::nullopt;
    return SharedIdlePreferences{owner + QLatin1Char('|') + snapshot.epoch + QLatin1Char('|') + powerSourceProfileKey(source),
        dim.toBool(), *dimSeconds, *display, lock.toBool(), action.toString(), *suspendSeconds};
}
}
