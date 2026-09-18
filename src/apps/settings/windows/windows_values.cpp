// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_windows/windows_values.h>

#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>

namespace QindaQt::Apps::SettingsWindows {
namespace {

[[nodiscard]] bool isNumericType(int metaTypeId) noexcept
{
    switch (metaTypeId) {
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::optional<QString> decodeToken(const QVariantMap &values, const char *key,
                                                 const QStringList &allowed, QString *error)
{
    const QVariant value = values.value(QLatin1String(key));
    if (value.metaType().id() != QMetaType::QString || !allowed.contains(value.toString())) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be one of %2")
                         .arg(QLatin1String(key), allowed.join(QStringLiteral(", ")));
        }
        return std::nullopt;
    }
    return value.toString();
}

[[nodiscard]] std::optional<int> decodeDistance(const QVariant &value, QString *error)
{
    if (!isNumericType(value.metaType().id())) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be a number")
                         .arg(QLatin1String(WindowsKeys::SnapDistance));
        }
        return std::nullopt;
    }
    const double distance = value.toDouble();
    if (!WindowsValues::isValidSnapDistance(distance)) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be a whole number between %2 and %3")
                         .arg(QLatin1String(WindowsKeys::SnapDistance))
                         .arg(WindowsValues::MinimumSnapDistance)
                         .arg(WindowsValues::MaximumSnapDistance);
        }
        return std::nullopt;
    }
    return static_cast<int>(distance);
}

} // namespace

QStringList WindowsKeys::scopedKeys()
{
    return {QLatin1String(FocusPolicy), QLatin1String(DockingModifier),
            QLatin1String(SnapDistance), QLatin1String(CloseContainerPolicy)};
}

QStringList WindowsValues::focusPolicyTokens()
{
    return {QStringLiteral("click"), QStringLiteral("focus-follows-mouse"),
            QStringLiteral("focus-under-mouse")};
}

QStringList WindowsValues::dockingModifierTokens()
{
    return {QStringLiteral("super"), QStringLiteral("alt"), QStringLiteral("control"),
            QStringLiteral("disabled")};
}

QStringList WindowsValues::closeContainerPolicyTokens()
{
    return {QStringLiteral("ask"), QStringLiteral("close-all"), QStringLiteral("ungroup")};
}

bool WindowsValues::isValidSnapDistance(double distance) noexcept
{
    return qIsFinite(distance) && distance >= MinimumSnapDistance
        && distance <= MaximumSnapDistance
        && distance == static_cast<double>(static_cast<int>(distance));
}

std::optional<WindowsValues> WindowsValues::fromVariantMap(const QVariantMap &values,
                                                           QString *error)
{
    const auto focusPolicy =
        decodeToken(values, WindowsKeys::FocusPolicy, focusPolicyTokens(), error);
    if (!focusPolicy) {
        return std::nullopt;
    }
    const auto dockingModifier =
        decodeToken(values, WindowsKeys::DockingModifier, dockingModifierTokens(), error);
    if (!dockingModifier) {
        return std::nullopt;
    }
    const auto snapDistance =
        decodeDistance(values.value(QLatin1String(WindowsKeys::SnapDistance)), error);
    if (!snapDistance) {
        return std::nullopt;
    }
    const auto closePolicy = decodeToken(values, WindowsKeys::CloseContainerPolicy,
                                         closeContainerPolicyTokens(), error);
    if (!closePolicy) {
        return std::nullopt;
    }
    return WindowsValues{.focusPolicy = *focusPolicy,
                         .dockingModifier = *dockingModifier,
                         .snapDistance = *snapDistance,
                         .closeContainerPolicy = *closePolicy};
}

QVariantMap WindowsValues::toVariantMap() const
{
    return {{QLatin1String(WindowsKeys::FocusPolicy), focusPolicy},
            {QLatin1String(WindowsKeys::DockingModifier), dockingModifier},
            {QLatin1String(WindowsKeys::SnapDistance), snapDistance},
            {QLatin1String(WindowsKeys::CloseContainerPolicy), closeContainerPolicy}};
}

QVariant WindowsValues::value(const QString &key) const
{
    return toVariantMap().value(key);
}

bool WindowsValues::sameValue(const QString &key, const QVariant &wire,
                              const QVariant &intended)
{
    if (key == QLatin1String(WindowsKeys::SnapDistance)) {
        QString ignored;
        const auto decoded = decodeDistance(wire, &ignored);
        return decoded.has_value() && *decoded == intended.toInt();
    }
    return wire.metaType().id() == QMetaType::QString
        && intended.metaType().id() == QMetaType::QString
        && wire.toString() == intended.toString();
}

bool WindowsValues::operator==(const WindowsValues &other) const noexcept
{
    return focusPolicy == other.focusPolicy && dockingModifier == other.dockingModifier
        && snapDistance == other.snapDistance
        && closeContainerPolicy == other.closeContainerPolicy;
}

} // namespace QindaQt::Apps::SettingsWindows
