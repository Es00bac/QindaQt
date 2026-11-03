// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/link_applet/link_applet_presentation.h>

namespace QindaQt::Shell::LinkApplet {
namespace {

constexpr qsizetype kMaxLabel = 64;

QString sanitize(const QString &raw)
{
    QString out;
    for (const QChar c : raw) {
        // Controls, bidi overrides and line breaks never reach the panel.
        if (c.isSpace()) {
            out.append(QLatin1Char(' '));
        } else if (c.isPrint() && c.category() != QChar::Other_Format) {
            out.append(c);
        }
        if (out.size() >= kMaxLabel) {
            break;
        }
    }
    return out.simplified();
}

QString lastSeenText(qint64 lastSeen, qint64 now)
{
    if (lastSeen <= 0) {
        return QStringLiteral("Not seen yet");
    }
    const qint64 minutes = qMax<qint64>(0, now - lastSeen) / 60000;
    if (minutes < 1) {
        return QStringLiteral("Last seen just now");
    }
    if (minutes < 60) {
        return QStringLiteral("Last seen %1 min ago").arg(minutes);
    }
    const qint64 hours = minutes / 60;
    if (hours < 48) {
        return QStringLiteral("Last seen %1 h ago").arg(hours);
    }
    return QStringLiteral("Last seen %1 days ago").arg(hours / 24);
}

QString networkText(const Link::LinkDevice &d)
{
    QString kind;
    if (d.networkType == QLatin1String("wifi")) {
        kind = QStringLiteral("Wi-Fi");
    } else if (d.networkType == QLatin1String("mobile")) {
        kind = QStringLiteral("Mobile data");
    } else if (d.networkType == QLatin1String("ethernet")) {
        kind = QStringLiteral("Ethernet");
    }
    if (kind.isEmpty()) {
        return {};
    }
    if (d.signal >= 0 && d.signal <= 4) {
        return QStringLiteral("%1, signal %2 of 4").arg(kind).arg(d.signal);
    }
    return kind;
}

const Link::LinkDevice *primary(const QList<Link::LinkDevice> &devices)
{
    const Link::LinkDevice *best = nullptr;
    int bestRank = -1;
    for (const auto &d : devices) {
        if (!d.paired) {
            continue;
        }
        const int rank = (d.connected && d.ready) ? 2 : d.connected ? 1 : 0;
        if (rank > bestRank) {
            best = &d;
            bestRank = rank;
        }
    }
    return best;
}

} // namespace

QString toString(const AppletState state)
{
    switch (state) {
    case AppletState::Unavailable: return QStringLiteral("unavailable");
    case AppletState::NoPhone: return QStringLiteral("no-phone");
    case AppletState::PairingRequest: return QStringLiteral("pairing-request");
    case AppletState::Connected: return QStringLiteral("connected");
    case AppletState::Offline: return QStringLiteral("offline");
    }
    return QStringLiteral("unavailable");
}

QString formatPairingCode(const QString &raw)
{
    if (raw.size() != 6) {
        return {};
    }
    for (const QChar c : raw) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
            return {};
        }
    }
    return raw.left(3) + QLatin1Char(' ') + raw.mid(3);
}

LinkAppletModel projectLinkApplet(const ProjectionContext &context)
{
    LinkAppletModel out;
    out.iconColorRole = QStringLiteral("muted");

    if (!context.serviceAvailable) {
        out.stateText = QStringLiteral("QindaLink is not running.");
        return out;
    }

    const Link::LinkDevice *device = primary(context.devices);
    if (device != nullptr) {
        out.deviceId = device->id;
        out.deviceName = sanitize(device->name);
    }

    // AGENT-GUARD: a waiting code outranks every other reading; a user who
    // does not see it cannot pair, and the code expires on the service side.
    if (context.pairing.has_value()) {
        const QString code = formatPairingCode(context.pairing->code);
        if (!code.isEmpty()) {
            out.state = AppletState::PairingRequest;
            out.hasPairing = true;
            out.pairing = {context.pairing->deviceId, sanitize(context.pairing->name),
                           sanitize(context.pairing->model), code};
            out.stateText = QStringLiteral("A phone wants to pair. Code %1.").arg(code);
            out.iconColorRole = QStringLiteral("warning");
            return out;
        }
    }

    if (device == nullptr) {
        out.state = AppletState::NoPhone;
        out.stateText = QStringLiteral("No phone paired. Open QindaLink on your phone to pair.");
        return out;
    }

    const QString who = out.deviceName.isEmpty() ? QStringLiteral("Phone") : out.deviceName;
    if (device->connected && device->ready) {
        out.state = AppletState::Connected;
        out.iconColorRole = QStringLiteral("success");
        out.connectionText = device->transport == QLatin1String("usb") ? QStringLiteral("USB")
            : device->transport == QLatin1String("lan")                ? QStringLiteral("Wi-Fi")
                                                                       : QString();
        if (device->batteryLevel >= 0 && device->batteryLevel <= 100) {
            out.batteryPercent = device->batteryLevel;
            out.batteryText = QStringLiteral("%1%").arg(device->batteryLevel);
        }
        out.charging = device->charging;
        out.networkText = networkText(*device);
        out.canRing = device->capabilities.contains(QStringLiteral("ring"));
        QString text = QStringLiteral("%1 connected").arg(who);
        if (!out.connectionText.isEmpty()) {
            text += QStringLiteral(" by %1").arg(out.connectionText);
        }
        if (!out.batteryText.isEmpty()) {
            text += QStringLiteral(", battery %1").arg(out.batteryText);
            if (out.charging) {
                text += QStringLiteral(" and charging");
            }
        }
        out.stateText = text + QLatin1Char('.');
        return out;
    }

    out.state = AppletState::Offline;
    out.lastSeenText = lastSeenText(device->lastSeen, context.nowMs);
    out.stateText = QStringLiteral("%1 is not connected. %2.").arg(who, out.lastSeenText);
    return out;
}

} // namespace QindaQt::Shell::LinkApplet
