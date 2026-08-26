// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applets/api_version.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

namespace QindaQt::Applets {

ApiVersion ApiVersion::current()
{
    return {CurrentMajor, CurrentMinor};
}

std::optional<ApiVersion> ApiVersion::parse(QStringView value)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^([0-9]+)\.([0-9]+)$)"));
    const QRegularExpressionMatch match = pattern.matchView(value);
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    bool majorIsValid = false;
    bool minorIsValid = false;
    const int major = match.capturedView(1).toInt(&majorIsValid);
    const int minor = match.capturedView(2).toInt(&minorIsValid);
    const ApiVersion version{major, minor};
    if (!majorIsValid || !minorIsValid || !version.isValid()) {
        return std::nullopt;
    }
    return version;
}

QString ApiVersion::toString() const
{
    return QStringLiteral("%1.%2").arg(major).arg(minor);
}

bool ApiVersion::isValid() const
{
    return major > 0 && minor >= 0;
}

bool ApiVersion::isSupportedBy(const ApiVersion &hostVersion) const
{
    return isValid() && hostVersion.isValid() && major == hostVersion.major
        && minor <= hostVersion.minor;
}

} // namespace QindaQt::Applets
