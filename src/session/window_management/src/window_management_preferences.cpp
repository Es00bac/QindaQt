// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/window_management_preferences.h"

#include <QMetaType>

namespace QindaQt::Session::WindowManagement {

namespace {

constexpr auto FocusPolicyKey = "windowManagement.focusPolicy";
constexpr auto DockingModifierKey = "windowManagement.dockingModifier";
constexpr auto SnapDistanceKey = "windowManagement.snapDistance";
constexpr auto SessionRestoreKey = "windowManagement.sessionRestore";
constexpr auto CloseContainerPolicyKey = "windowManagement.closeContainerPolicy";

bool fail(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool readString(const QVariantMap &values, const char *key, QString *out, QString *error)
{
    const QVariant value = values.value(QLatin1String(key));
    if (value.metaType().id() != QMetaType::QString) {
        return fail(error, QStringLiteral("%1 is missing or not a string").arg(QLatin1String(key)));
    }
    *out = value.toString();
    return true;
}

} // namespace

QStringList WindowManagementPreferences::scopedKeys()
{
    return {QLatin1String(FocusPolicyKey), QLatin1String(DockingModifierKey),
            QLatin1String(SnapDistanceKey), QLatin1String(SessionRestoreKey),
            QLatin1String(CloseContainerPolicyKey)};
}

std::optional<WindowManagementPreferences> WindowManagementPreferences::fromVariantMap(
    const QVariantMap &values, QString *error)
{
    WindowManagementPreferences preferences;
    QString text;
    if (!readString(values, FocusPolicyKey, &text, error)) {
        return std::nullopt;
    }
    if (text == QLatin1String("click")) {
        preferences.focusPolicy = FocusPolicy::Click;
    } else if (text == QLatin1String("focus-follows-mouse")) {
        preferences.focusPolicy = FocusPolicy::FocusFollowsMouse;
    } else if (text == QLatin1String("focus-under-mouse")) {
        preferences.focusPolicy = FocusPolicy::FocusUnderMouse;
    } else {
        fail(error, QStringLiteral("%1 has no focus policy '%2'").arg(QLatin1String(FocusPolicyKey), text));
        return std::nullopt;
    }
    if (!readString(values, DockingModifierKey, &text, error)) {
        return std::nullopt;
    }
    if (text == QLatin1String("super")) {
        preferences.dockingModifier = DockingModifier::Super;
    } else if (text == QLatin1String("alt")) {
        preferences.dockingModifier = DockingModifier::Alt;
    } else if (text == QLatin1String("control")) {
        preferences.dockingModifier = DockingModifier::Control;
    } else if (text == QLatin1String("disabled")) {
        preferences.dockingModifier = DockingModifier::Disabled;
    } else {
        fail(error, QStringLiteral("%1 has no docking modifier '%2'").arg(QLatin1String(DockingModifierKey), text));
        return std::nullopt;
    }
    const QVariant snap = values.value(QLatin1String(SnapDistanceKey));
    const int snapType = snap.metaType().id();
    if (snapType != QMetaType::Int && snapType != QMetaType::LongLong && snapType != QMetaType::Double
        && snapType != QMetaType::UInt && snapType != QMetaType::ULongLong) {
        fail(error, QStringLiteral("%1 is missing or not a number").arg(QLatin1String(SnapDistanceKey)));
        return std::nullopt;
    }
    const double snapValue = snap.toDouble();
    if (snapValue < 0.0 || snapValue > 64.0 || snapValue != static_cast<double>(static_cast<int>(snapValue))) {
        fail(error, QStringLiteral("%1 is outside 0..64").arg(QLatin1String(SnapDistanceKey)));
        return std::nullopt;
    }
    preferences.snapDistance = static_cast<int>(snapValue);
    const QVariant restore = values.value(QLatin1String(SessionRestoreKey));
    if (restore.metaType().id() != QMetaType::Bool) {
        fail(error, QStringLiteral("%1 is missing or not a boolean").arg(QLatin1String(SessionRestoreKey)));
        return std::nullopt;
    }
    preferences.sessionRestore = restore.toBool();
    if (!readString(values, CloseContainerPolicyKey, &text, error)) {
        return std::nullopt;
    }
    if (text == QLatin1String("ask")) {
        preferences.closeContainerPolicy = CloseContainerPolicy::Ask;
    } else if (text == QLatin1String("close-all")) {
        preferences.closeContainerPolicy = CloseContainerPolicy::CloseAll;
    } else if (text == QLatin1String("ungroup")) {
        preferences.closeContainerPolicy = CloseContainerPolicy::Ungroup;
    } else {
        fail(error, QStringLiteral("%1 has no close policy '%2'").arg(QLatin1String(CloseContainerPolicyKey), text));
        return std::nullopt;
    }
    if (error != nullptr) {
        error->clear();
    }
    return preferences;
}

QString WindowManagementPreferences::kwinFocusPolicy(FocusPolicy policy)
{
    switch (policy) {
    case FocusPolicy::Click:
        return QStringLiteral("ClickToFocus");
    case FocusPolicy::FocusFollowsMouse:
        return QStringLiteral("FocusFollowsMouse");
    case FocusPolicy::FocusUnderMouse:
        return QStringLiteral("FocusUnderMouse");
    }
    return QStringLiteral("ClickToFocus");
}

QString WindowManagementPreferences::dockingModifierName(DockingModifier modifier)
{
    switch (modifier) {
    case DockingModifier::Super:
        return QStringLiteral("super");
    case DockingModifier::Alt:
        return QStringLiteral("alt");
    case DockingModifier::Control:
        return QStringLiteral("control");
    case DockingModifier::Disabled:
        return QStringLiteral("disabled");
    }
    return QStringLiteral("super");
}

QString WindowManagementPreferences::closeContainerPolicyName(CloseContainerPolicy policy)
{
    switch (policy) {
    case CloseContainerPolicy::Ask:
        return QStringLiteral("ask");
    case CloseContainerPolicy::CloseAll:
        return QStringLiteral("close-all");
    case CloseContainerPolicy::Ungroup:
        return QStringLiteral("ungroup");
    }
    return QStringLiteral("ask");
}

} // namespace QindaQt::Session::WindowManagement
