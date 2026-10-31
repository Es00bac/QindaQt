// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/firewall_applet/firewall_applet_presentation.h>

#include <QTest>

using namespace QindaQt::Shell::FirewallApplet;
using namespace QindaQt::Firewall;

namespace {

Alert dangerAlert()
{
    Alert alert;
    alert.id = QStringLiteral("password-guessing:203.0.113.9:1761940000");
    alert.severity = AlertSeverity::Danger;
    alert.title = QStringLiteral("Possible break-in — act now");
    alert.summary = QStringLiteral("12 attempts from 203.0.113.9 in the last 5 minutes.");
    alert.meaning = QStringLiteral("Yes, this is dangerous.");
    alert.advice = QStringLiteral("Block this address.");
    alert.canConfirm = false;
    return alert;
}

} // namespace

class FirewallAppletPresentationTests final : public QObject {
    Q_OBJECT

private slots:
    void serviceAbsentIsUnavailable();
    void deniedStateIsBlockedRegardlessOfHeadline();
    void okHeadlineWithNoAlertsIsSecure();
    void unknownOrStartingHeadlineIsWatching();
    void noticeOrWarningHeadlineIsAttention();
    void dangerHeadlineIsAlarmWithPersistentAttention();
    void okHeadlineWithOpenAlertsFallsBackToWatching();
    void topAlertPresentationCopiesPlainEnglishFieldsVerbatim();
    void noTopAlertMeansNoAlertPresented();
    void sameInputsProjectEqualModels();
};

void FirewallAppletPresentationTests::serviceAbsentIsUnavailable()
{
    ProjectionContext context;
    context.serviceAvailable = false;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Danger;
    context.alertCount = 5;
    context.topAlert = dangerAlert();

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(model.state == AppletState::Unavailable);
    QCOMPARE(model.stateText,
             QStringLiteral("Background watching is off. Open QindaFirewall to turn it on."));
    QCOMPARE(model.iconColorRole, QStringLiteral("muted"));
    QVERIFY(!model.persistentAttention);
    // AGENT-GUARD: absence wins over everything else in the context, even a
    // topAlert/alertCount a stale caller still passed in.
    QVERIFY(!model.hasAlert);
    QCOMPARE(model.alertCount, quint32(0));
}

void FirewallAppletPresentationTests::deniedStateIsBlockedRegardlessOfHeadline()
{
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("denied");
    context.headline.level = HeadlineLevel::Danger;
    context.headline.title = QStringLiteral("Possible break-in — act now");
    context.alertCount = 3;

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(model.state == AppletState::Blocked);
    QCOMPARE(model.stateText, QStringLiteral("QindaFirewall can't read the logs."));
    QCOMPARE(model.iconColorRole, QStringLiteral("warning"));
    QVERIFY(!model.persistentAttention);
}

void FirewallAppletPresentationTests::okHeadlineWithNoAlertsIsSecure()
{
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Ok;
    context.alertCount = 0;

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(model.state == AppletState::Secure);
    QCOMPARE(model.stateText, QStringLiteral("All quiet. Nobody is trying to get in."));
    QCOMPARE(model.iconColorRole, QStringLiteral("success"));
}

void FirewallAppletPresentationTests::unknownOrStartingHeadlineIsWatching()
{
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("starting");
    context.headline.level = HeadlineLevel::Unknown;
    context.alertCount = 0;

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(model.state == AppletState::Watching);
    QCOMPARE(model.stateText, QStringLiteral("Watching the logs…"));
    QCOMPARE(model.iconColorRole, QStringLiteral("muted"));
}

void FirewallAppletPresentationTests::noticeOrWarningHeadlineIsAttention()
{
    ProjectionContext notice;
    notice.serviceAvailable = true;
    notice.state = QStringLiteral("live");
    notice.headline.level = HeadlineLevel::Notice;
    notice.headline.title = QStringLiteral("Someone is trying to get in");
    notice.alertCount = 1;
    QVERIFY(projectFirewallApplet(notice).state == AppletState::Attention);

    ProjectionContext warning = notice;
    warning.headline.level = HeadlineLevel::Warning;
    const FirewallAppletModel model = projectFirewallApplet(warning);
    QVERIFY(model.state == AppletState::Attention);
    QCOMPARE(model.stateText, QStringLiteral("Someone is trying to get in"));
    QCOMPARE(model.iconColorRole, QStringLiteral("warning"));
    QVERIFY(!model.persistentAttention);
}

void FirewallAppletPresentationTests::dangerHeadlineIsAlarmWithPersistentAttention()
{
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Danger;
    context.headline.title = QStringLiteral("Possible break-in — act now");
    context.alertCount = 1;
    context.topAlert = dangerAlert();

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(model.state == AppletState::Alarm);
    QCOMPARE(model.stateText, QStringLiteral("Possible break-in — act now"));
    QCOMPARE(model.iconColorRole, QStringLiteral("danger"));
    QVERIFY(model.persistentAttention);
}

void FirewallAppletPresentationTests::okHeadlineWithOpenAlertsFallsBackToWatching()
{
    // Inconsistent SecurityMonitor data (ok headline, nonzero AlertCount)
    // must not be read as either Secure or Attention.
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Ok;
    context.alertCount = 1;

    QVERIFY(projectFirewallApplet(context).state == AppletState::Watching);
}

void FirewallAppletPresentationTests::topAlertPresentationCopiesPlainEnglishFieldsVerbatim()
{
    Alert alert = dangerAlert();
    alert.canConfirm = true;
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Danger;
    context.headline.title = alert.title;
    context.alertCount = 1;
    context.topAlert = alert;

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(model.hasAlert);
    QCOMPARE(model.alert.id, alert.id);
    QCOMPARE(model.alert.title, alert.title);
    QCOMPARE(model.alert.summary, alert.summary);
    QCOMPARE(model.alert.meaning, alert.meaning);
    QCOMPARE(model.alert.advice, alert.advice);
    QCOMPARE(model.alert.canConfirm, true);
    QCOMPARE(model.alertCount, quint32(1));
}

void FirewallAppletPresentationTests::noTopAlertMeansNoAlertPresented()
{
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Ok;
    context.alertCount = 0;
    context.topAlert = std::nullopt;

    const FirewallAppletModel model = projectFirewallApplet(context);
    QVERIFY(!model.hasAlert);
    QCOMPARE(model.alert, AlertPresentation{});
}

void FirewallAppletPresentationTests::sameInputsProjectEqualModels()
{
    ProjectionContext context;
    context.serviceAvailable = true;
    context.state = QStringLiteral("live");
    context.headline.level = HeadlineLevel::Notice;
    context.headline.title = QStringLiteral("Someone is trying to get in");
    context.alertCount = 2;
    context.topAlert = dangerAlert();

    QCOMPARE(projectFirewallApplet(context), projectFirewallApplet(context));
}

QTEST_GUILESS_MAIN(FirewallAppletPresentationTests)
#include "tst_firewall_applet_presentation.moc"
