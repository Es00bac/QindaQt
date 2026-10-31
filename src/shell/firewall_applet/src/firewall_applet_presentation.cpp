// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/firewall_applet/firewall_applet_presentation.h>

namespace QindaQt::Shell::FirewallApplet {
namespace {

AlertPresentation presentAlert(const Firewall::Alert &alert)
{
    AlertPresentation out;
    out.id = alert.id;
    out.title = alert.title;
    out.summary = alert.summary;
    out.meaning = alert.meaning;
    out.advice = alert.advice;
    out.canConfirm = alert.canConfirm;
    return out;
}

} // namespace

QString toString(const AppletState state)
{
    switch (state) {
    case AppletState::Unavailable: return QStringLiteral("unavailable");
    case AppletState::Blocked: return QStringLiteral("blocked");
    case AppletState::Secure: return QStringLiteral("secure");
    case AppletState::Watching: return QStringLiteral("watching");
    case AppletState::Attention: return QStringLiteral("attention");
    case AppletState::Alarm: return QStringLiteral("alarm");
    }
    return QStringLiteral("unavailable");
}

FirewallAppletModel projectFirewallApplet(const ProjectionContext &context)
{
    FirewallAppletModel out;

    if (!context.serviceAvailable) {
        out.state = AppletState::Unavailable;
        out.stateText = QStringLiteral(
            "Background watching is off. Open QindaFirewall to turn it on.");
        out.iconColorRole = QStringLiteral("muted");
        return out;
    }

    // AGENT-GUARD: `denied` wins over every headline reading. A headline
    // computed before the log watcher lost read access is not current
    // truth about whether this computer is being attacked right now, so it
    // must never be shown as if it still were.
    if (Firewall::logWatcherStateFromString(context.state)
        == Firewall::LogWatcherState::Denied) {
        out.state = AppletState::Blocked;
        out.stateText = QStringLiteral("QindaFirewall can't read the logs.");
        out.iconColorRole = QStringLiteral("warning");
    } else if (context.headline.level == Firewall::HeadlineLevel::Danger) {
        out.state = AppletState::Alarm;
        out.stateText = context.headline.title.isEmpty()
            ? QStringLiteral("Possible break-in — act now")
            : context.headline.title;
        out.iconColorRole = QStringLiteral("danger");
        out.persistentAttention = true;
    } else if (context.headline.level == Firewall::HeadlineLevel::Notice
              || context.headline.level == Firewall::HeadlineLevel::Warning) {
        out.state = AppletState::Attention;
        out.stateText = context.headline.title.isEmpty()
            ? QStringLiteral("Someone is trying to get in")
            : context.headline.title;
        out.iconColorRole = QStringLiteral("warning");
    } else if (context.headline.level == Firewall::HeadlineLevel::Ok
              && context.alertCount == 0) {
        out.state = AppletState::Secure;
        out.stateText = QStringLiteral("All quiet. Nobody is trying to get in.");
        out.iconColorRole = QStringLiteral("success");
    } else {
        // AGENT-NOTE: catches an Unknown headline level (not yet decoded,
        // or the monitor just started) and the inconsistent case of an
        // "ok" headline with AlertCount still nonzero. Neither is grounds
        // to claim Secure or Attention, so this is the one deliberately
        // uncertain bucket.
        out.state = AppletState::Watching;
        out.stateText = QStringLiteral("Watching the logs…");
        out.iconColorRole = QStringLiteral("muted");
    }

    out.alertCount = context.alertCount;
    if (context.topAlert.has_value()) {
        out.hasAlert = true;
        out.alert = presentAlert(*context.topAlert);
    }
    return out;
}

} // namespace QindaQt::Shell::FirewallApplet
