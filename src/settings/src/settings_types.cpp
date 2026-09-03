// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/settings/settings_types.h"

#include <array>

namespace QindaQt::Settings {
namespace {

template<typename Enum>
struct EnumName final {
    Enum value;
    const char *name;
};

constexpr std::array layerNames{
    EnumName{SettingLayer::SystemDefaults, "system-defaults"},
    EnumName{SettingLayer::ProfileDefaults, "profile-defaults"},
    EnumName{SettingLayer::UserOverrides, "user-overrides"},
    EnumName{SettingLayer::SessionOverrides, "session-overrides"},
};

constexpr std::array domainNames{
    EnumName{SettingDomain::Appearance, "appearance"},
    EnumName{SettingDomain::Fonts, "fonts"},
    EnumName{SettingDomain::Displays, "displays"},
    EnumName{SettingDomain::Input, "input"},
    EnumName{SettingDomain::Panels, "panels"},
    EnumName{SettingDomain::WindowManagement, "window-management"},
    EnumName{SettingDomain::Accessibility, "accessibility"},
    EnumName{SettingDomain::Services, "services"},
};

constexpr std::array valueTypeNames{
    EnumName{SettingValueType::Boolean, "boolean"},
    EnumName{SettingValueType::Integer, "integer"},
    EnumName{SettingValueType::Number, "number"},
    EnumName{SettingValueType::String, "string"},
    EnumName{SettingValueType::StringList, "string-list"},
    EnumName{SettingValueType::Object, "object"},
};

template<typename Enum, std::size_t Size>
QString enumToString(Enum value, const std::array<EnumName<Enum>, Size> &values)
{
    for (const auto &candidate : values) {
        if (candidate.value == value) {
            return QString::fromLatin1(candidate.name);
        }
    }
    return {};
}

template<typename Enum, std::size_t Size>
bool parseEnum(const QString &text, Enum *value, const std::array<EnumName<Enum>, Size> &values)
{
    if (value == nullptr) {
        return false;
    }
    for (const auto &candidate : values) {
        if (text == QLatin1String(candidate.name)) {
            *value = candidate.value;
            return true;
        }
    }
    return false;
}

} // namespace

QString toString(SettingLayer layer) { return enumToString(layer, layerNames); }
QString toString(SettingDomain domain) { return enumToString(domain, domainNames); }
QString toString(SettingValueType type) { return enumToString(type, valueTypeNames); }

QString domainKeyPrefix(SettingDomain domain)
{
    if (domain == SettingDomain::WindowManagement) {
        return QStringLiteral("windowManagement");
    }
    return toString(domain);
}

bool parseSettingLayer(const QString &text, SettingLayer *layer)
{
    return parseEnum(text, layer, layerNames);
}

bool parseSettingDomain(const QString &text, SettingDomain *domain)
{
    return parseEnum(text, domain, domainNames);
}

bool parseSettingValueType(const QString &text, SettingValueType *type)
{
    return parseEnum(text, type, valueTypeNames);
}

bool isMutableLayer(SettingLayer layer) noexcept
{
    return layer != SettingLayer::SystemDefaults;
}

bool isPersistentLayer(SettingLayer layer) noexcept
{
    return layer == SettingLayer::ProfileDefaults || layer == SettingLayer::UserOverrides;
}

QString ValidationResult::summary() const
{
    QStringList messages;
    messages.reserve(m_issues.size());
    for (const auto &issue : m_issues) {
        const auto location = issue.key.isEmpty() ? QString() : issue.key + QStringLiteral(": ");
        messages.append(location + issue.message);
    }
    return messages.join(QStringLiteral("; "));
}

void ValidationResult::add(QString key, QString code, QString message)
{
    m_issues.append({std::move(key), std::move(code), std::move(message)});
}

void ValidationResult::append(const ValidationResult &other)
{
    m_issues.append(other.issues());
}

} // namespace QindaQt::Settings
