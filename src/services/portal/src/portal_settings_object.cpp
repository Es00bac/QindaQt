// SPDX-License-Identifier: LGPL-3.0-or-later
#include "portal_settings_object_p.h"

#include <QDBusError>
#include <QDBusMetaType>

#include <algorithm>
#include <cmath>

namespace QindaQt::Services::Portal {

QDBusArgument &operator<<(QDBusArgument &argument,
                          const PortalAccentColor &color)
{
    argument.beginStructure();
    argument << color.red << color.green << color.blue;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                PortalAccentColor &color)
{
    argument.beginStructure();
    argument >> color.red >> color.green >> color.blue;
    argument.endStructure();
    return argument;
}

namespace Private {
namespace {

constexpr qsizetype MaximumNamespaceFilters = 64;
constexpr qsizetype MaximumTextBytes = 256;

bool boundedText(const QString &text)
{
    if (text.contains(QChar::Null) || text.toUtf8().size() > MaximumTextBytes) {
        return false;
    }
    const auto utf16 = QStringView(text);
    for (qsizetype index = 0; index < utf16.size(); ++index) {
        const QChar unit = utf16.at(index);
        if (unit.isHighSurrogate()) {
            if (index + 1 >= utf16.size()
                || !utf16.at(index + 1).isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (unit.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

bool patternMatches(const QString &pattern, const QString &namespaceName)
{
    if (pattern.isEmpty() || pattern == QStringLiteral("*")) {
        return true;
    }
    if (!pattern.contains(QLatin1Char('*'))) {
        return pattern == namespaceName;
    }
    if (!pattern.endsWith(QStringLiteral(".*"))
        || pattern.count(QLatin1Char('*')) != 1) {
        return false;
    }
    return namespaceName.startsWith(pattern.first(pattern.size() - 1));
}

} // namespace

void registerPortalDBusTypes()
{
    qRegisterMetaType<PortalAccentColor>();
    qRegisterMetaType<PortalNamespaceMap>();
    qDBusRegisterMetaType<PortalAccentColor>();
    qDBusRegisterMetaType<PortalNamespaceMap>();
}

PortalSettingsObject::PortalSettingsObject(AppearanceSource &source,
                                           QObject *parent)
    : QObject(parent)
    , m_source(source)
{
    registerPortalDBusTypes();
    connect(&m_source, &AppearanceSource::currentChanged,
            this, &PortalSettingsObject::handleSourceChanged);
}

quint32 PortalSettingsObject::version() const noexcept
{
    return kPortalSettingsBackendVersion;
}

PortalNamespaceMap PortalSettingsObject::ReadAll(
    const QStringList &namespaces)
{
    QString error;
    if (!validateReadAllInput(namespaces, &error)) {
        sendErrorReply(QDBusError::InvalidArgs, error);
        return {};
    }
    if (!matchesAppearance(namespaces) || !m_source.current().has_value()) {
        return {};
    }
    return {{QString::fromLatin1(kAppearanceNamespace),
             valuesFor(m_source.current()->policy)}};
}

QDBusVariant PortalSettingsObject::Read(const QString &namespaceName,
                                        const QString &key)
{
    if (!boundedText(namespaceName) || !boundedText(key)) {
        sendErrorReply(QDBusError::InvalidArgs,
                       QStringLiteral("portal Settings name exceeds its bound"));
        return {};
    }
    if (!m_source.current().has_value()) {
        sendErrorReply(QDBusError::Failed,
                       QStringLiteral("QindaQt appearance truth is unavailable"));
        return {};
    }
    if (namespaceName != QString::fromLatin1(kAppearanceNamespace)) {
        sendErrorReply(QDBusError::UnknownProperty,
                       QStringLiteral("portal Settings namespace is not supported"));
        return {};
    }
    const QVariant value = valueFor(m_source.current()->policy, key);
    if (!value.isValid()) {
        sendErrorReply(QDBusError::UnknownProperty,
                       QStringLiteral("portal Settings key is not supported"));
        return {};
    }
    return QDBusVariant(value);
}

void PortalSettingsObject::handleSourceChanged()
{
    if (!m_source.current().has_value()) {
        // The standard signal has no removal form. Methods already withdraw
        // stale truth; retaining this comparison baseline avoids falsely
        // signalling unchanged values after a same-value owner replacement.
        return;
    }
    const AppearancePolicy &next = m_source.current()->policy;
    if (!m_lastSignalled.has_value()
        || m_lastSignalled->colorScheme != next.colorScheme) {
        emitChanged(QString::fromLatin1(kColorSchemeKey),
                    valueFor(next, QString::fromLatin1(kColorSchemeKey)));
    }
    if (!m_lastSignalled.has_value()
        || m_lastSignalled->accentColor != next.accentColor) {
        emitChanged(QString::fromLatin1(kAccentColorKey),
                    valueFor(next, QString::fromLatin1(kAccentColorKey)));
    }
    if (!m_lastSignalled.has_value()
        || m_lastSignalled->contrast != next.contrast) {
        emitChanged(QString::fromLatin1(kContrastKey),
                    valueFor(next, QString::fromLatin1(kContrastKey)));
    }
    m_lastSignalled = next;
}

bool PortalSettingsObject::validateReadAllInput(
    const QStringList &namespaces, QString *error) const
{
    if (namespaces.size() > MaximumNamespaceFilters) {
        *error = QStringLiteral("portal Settings namespace filter count exceeds 64");
        return false;
    }
    for (const QString &pattern : namespaces) {
        if (!boundedText(pattern)) {
            *error = QStringLiteral("portal Settings namespace filter is malformed");
            return false;
        }
    }
    return true;
}

bool PortalSettingsObject::matchesAppearance(
    const QStringList &namespaces) const
{
    if (namespaces.isEmpty()) {
        return true;
    }
    const QString name = QString::fromLatin1(kAppearanceNamespace);
    return std::any_of(namespaces.cbegin(), namespaces.cend(),
                       [&name](const QString &pattern) {
                           return patternMatches(pattern, name);
                       });
}

QVariant PortalSettingsObject::valueFor(const AppearancePolicy &policy,
                                        const QString &key) const
{
    if (key == QString::fromLatin1(kColorSchemeKey)) {
        return QVariant::fromValue(static_cast<quint32>(policy.colorScheme));
    }
    if (key == QString::fromLatin1(kAccentColorKey)) {
        return QVariant::fromValue(policy.accentColor);
    }
    if (key == QString::fromLatin1(kContrastKey)) {
        return QVariant::fromValue(static_cast<quint32>(policy.contrast));
    }
    return {};
}

QVariantMap PortalSettingsObject::valuesFor(
    const AppearancePolicy &policy) const
{
    return {{QString::fromLatin1(kColorSchemeKey),
             valueFor(policy, QString::fromLatin1(kColorSchemeKey))},
            {QString::fromLatin1(kAccentColorKey),
             valueFor(policy, QString::fromLatin1(kAccentColorKey))},
            {QString::fromLatin1(kContrastKey),
             valueFor(policy, QString::fromLatin1(kContrastKey))}};
}

void PortalSettingsObject::emitChanged(const QString &key,
                                       const QVariant &value)
{
    Q_EMIT SettingChanged(QString::fromLatin1(kAppearanceNamespace), key,
                          QDBusVariant(value));
}

} // namespace Private
} // namespace QindaQt::Services::Portal
