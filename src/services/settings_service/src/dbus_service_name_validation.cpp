// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbus_service_name_validation_p.h"

namespace QindaQt::Services::SettingsService::Private {
namespace {

bool isAsciiLetter(ushort value) noexcept
{
    return (value >= ushort('A') && value <= ushort('Z')) || (value >= ushort('a') && value <= ushort('z'));
}

bool isElementCharacter(ushort value) noexcept
{
    return isAsciiLetter(value) || (value >= ushort('0') && value <= ushort('9')) || value == ushort('_')
        || value == ushort('-');
}

} // namespace

bool validateWellKnownServiceName(const QString &serviceName, QString *error)
{
    auto reject = [error](const QString &message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    // D-Bus names are ASCII, so QString length equals the encoded byte count
    // after the character-domain check below.
    if (serviceName.isEmpty() || serviceName.size() > 255) {
        return reject(QStringLiteral("settings service name must contain 1 through 255 ASCII bytes"));
    }
    if (serviceName.startsWith(QLatin1Char(':'))) {
        return reject(QStringLiteral("settings service name must be well-known, not a unique bus name"));
    }

    bool sawSeparator = false;
    bool atElementStart = true;
    for (const QChar character : serviceName) {
        const ushort value = character.unicode();
        if (value == ushort('.')) {
            if (atElementStart) {
                return reject(QStringLiteral("settings service name contains an empty element"));
            }
            sawSeparator = true;
            atElementStart = true;
            continue;
        }
        if (!isElementCharacter(value)) {
            return reject(QStringLiteral("settings service name contains an invalid character"));
        }
        if (atElementStart && value >= ushort('0') && value <= ushort('9')) {
            return reject(QStringLiteral("settings service name elements must not begin with a digit"));
        }
        atElementStart = false;
    }

    if (atElementStart) {
        return reject(QStringLiteral("settings service name contains an empty element"));
    }
    if (!sawSeparator) {
        return reject(QStringLiteral("settings service name must contain at least two elements"));
    }
    return true;
}

} // namespace QindaQt::Services::SettingsService::Private
