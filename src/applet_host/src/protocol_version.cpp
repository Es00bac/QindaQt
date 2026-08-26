// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/protocol_version.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include <algorithm>

namespace QindaQt::AppletHost {

ProtocolVersion ProtocolVersion::current()
{
    return {CurrentMajor, CurrentMinor};
}

std::optional<ProtocolVersion> ProtocolVersion::parse(QStringView value)
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
    const ProtocolVersion version{major, minor};
    if (!majorIsValid || !minorIsValid || !version.isValid()) {
        return std::nullopt;
    }
    return version;
}

QString ProtocolVersion::toString() const
{
    return QStringLiteral("%1.%2").arg(major).arg(minor);
}

bool ProtocolVersion::isValid() const
{
    return major > 0 && minor >= 0;
}

std::optional<ProtocolVersion> ProtocolVersion::negotiate(const ProtocolVersion &peer) const
{
    if (!isValid() || !peer.isValid() || major != peer.major) {
        return std::nullopt;
    }
    return ProtocolVersion{major, std::min(minor, peer.minor)};
}

} // namespace QindaQt::AppletHost
