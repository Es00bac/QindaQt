// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/firewall_protocol/firewall_codec.h>

#include <QTest>

using namespace QindaQt::Firewall;

class FirewallCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void decodesAWellFormedHeadline();
    void headlineDecodeFailsOnMalformedOrEmptyPayload();
    void decodesAWellFormedAlert();
    void emptyTopAlertPayloadIsNotAnAlert();
    void alertDecodeFailsWhenIdIsMissing();
    void unrecognizedAlertFieldsAreIgnored();
    void decodesAnAlertListMostSeriousFirst();
    void oneMalformedListElementIsSkippedNotFatal();
    void stateAndSeverityStringRoundTrip();
};

void FirewallCodecTests::decodesAWellFormedHeadline()
{
    const QByteArray json =
        R"({"level":"danger","title":"Possible break-in","text":"Someone is trying to get in."})";
    Headline headline;
    QVERIFY(decodeHeadline(json, &headline));
    QVERIFY(headline.level == HeadlineLevel::Danger);
    QCOMPARE(headline.title, QStringLiteral("Possible break-in"));
    QCOMPARE(headline.text, QStringLiteral("Someone is trying to get in."));
}

void FirewallCodecTests::headlineDecodeFailsOnMalformedOrEmptyPayload()
{
    Headline headline;
    QVERIFY(!decodeHeadline(QByteArray(), &headline));
    QCOMPARE(headline, Headline{});

    QVERIFY(!decodeHeadline(QByteArrayLiteral("not json"), &headline));
    QCOMPARE(headline, Headline{});

    QVERIFY(!decodeHeadline(QByteArrayLiteral("[1,2,3]"), &headline));
    QCOMPARE(headline, Headline{});

    // An unrecognized level string decodes to Unknown rather than failing
    // the whole headline; presentation treats Unknown as "watching".
    QVERIFY(decodeHeadline(R"({"level":"catastrophic","title":"t","text":"x"})",
                           &headline));
    QVERIFY(headline.level == HeadlineLevel::Unknown);
}

void FirewallCodecTests::decodesAWellFormedAlert()
{
    const QByteArray json = R"({
        "id":"password-guessing:203.0.113.9:1761940000",
        "kind":"password-guessing",
        "severity":"danger",
        "severityLabel":"Danger",
        "title":"Someone is trying to guess a password",
        "summary":"12 attempts from 203.0.113.9 in the last 5 minutes.",
        "meaning":"Yes, this is dangerous.",
        "advice":"Block this address.",
        "canBlock":true,
        "canConfirm":false,
        "address":"203.0.113.9",
        "place":"on the internet",
        "user":"root",
        "count":12,
        "firstText":"5 minutes ago",
        "lastText":"just now",
        "ongoing":true,
        "remote":true,
        "evidence":["Oct 31 14:02:11 sshd: Failed password for root"],
        "mac":""
    })";
    Alert alert;
    QVERIFY(decodeAlert(json, &alert));
    QCOMPARE(alert.id, QStringLiteral("password-guessing:203.0.113.9:1761940000"));
    QCOMPARE(alert.kind, QStringLiteral("password-guessing"));
    QVERIFY(alert.severity == AlertSeverity::Danger);
    QCOMPARE(alert.severityLabel, QStringLiteral("Danger"));
    QCOMPARE(alert.canBlock, true);
    QCOMPARE(alert.canConfirm, false);
    QCOMPARE(alert.address, QStringLiteral("203.0.113.9"));
    QCOMPARE(alert.count, quint64(12));
    QCOMPARE(alert.ongoing, true);
    QCOMPARE(alert.remote, true);
    QCOMPARE(alert.evidence.size(), 1);
}

void FirewallCodecTests::emptyTopAlertPayloadIsNotAnAlert()
{
    // QindaFirewall docs/monitor.md: TopAlert/Alert(id) return "" for "no
    // alert". That is not malformed JSON, but it still decodes to false.
    Alert alert;
    QVERIFY(!decodeAlert(QByteArray(), &alert));
    QCOMPARE(alert, Alert{});
}

void FirewallCodecTests::alertDecodeFailsWhenIdIsMissing()
{
    Alert alert;
    QVERIFY(!decodeAlert(R"({"title":"No id here"})", &alert));
    QCOMPARE(alert, Alert{});
}

void FirewallCodecTests::unrecognizedAlertFieldsAreIgnored()
{
    Alert alert;
    QVERIFY(decodeAlert(R"({"id":"a","x-future-field":{"nested":true}})", &alert));
    QCOMPARE(alert.id, QStringLiteral("a"));
}

void FirewallCodecTests::decodesAnAlertListMostSeriousFirst()
{
    const QByteArray json = R"([
        {"id":"a","severity":"danger"},
        {"id":"b","severity":"notice"}
    ])";
    const QList<Alert> alerts = decodeAlertList(json);
    QCOMPARE(alerts.size(), 2);
    QCOMPARE(alerts.at(0).id, QStringLiteral("a"));
    QVERIFY(alerts.at(0).severity == AlertSeverity::Danger);
    QCOMPARE(alerts.at(1).id, QStringLiteral("b"));
    QVERIFY(alerts.at(1).severity == AlertSeverity::Notice);
}

void FirewallCodecTests::oneMalformedListElementIsSkippedNotFatal()
{
    const QByteArray json = R"([
        {"id":"a"},
        {"title":"missing id, skipped"},
        "not even an object",
        {"id":"c"}
    ])";
    const QList<Alert> alerts = decodeAlertList(json);
    QCOMPARE(alerts.size(), 2);
    QCOMPARE(alerts.at(0).id, QStringLiteral("a"));
    QCOMPARE(alerts.at(1).id, QStringLiteral("c"));
}

void FirewallCodecTests::stateAndSeverityStringRoundTrip()
{
    QVERIFY(logWatcherStateFromString(QStringLiteral("denied")) == LogWatcherState::Denied);
    QVERIFY(logWatcherStateFromString(QStringLiteral("bogus")) == LogWatcherState::Unknown);
    QCOMPARE(toString(LogWatcherState::Live), QStringLiteral("live"));

    QVERIFY(alertSeverityFromString(QStringLiteral("warning")) == AlertSeverity::Warning);
    QVERIFY(alertSeverityFromString(QStringLiteral("bogus")) == AlertSeverity::Info);
    QCOMPARE(toString(AlertSeverity::Notice), QStringLiteral("notice"));
}

QTEST_GUILESS_MAIN(FirewallCodecTests)
#include "tst_firewall_codec.moc"
