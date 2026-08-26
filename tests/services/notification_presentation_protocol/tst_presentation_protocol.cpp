// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_access_token.h"
#include "qindaqt/services/notification_presentation/presentation_snapshot.h"
#include "qindaqt/services/notification_presentation/wire_contract.h"

#include <QtTest>

using namespace QindaQt::Services::NotificationPresentation;

namespace {

PresentationSnapshot snapshot()
{
    PresentationNotification notification;
    notification.id = 7;
    notification.applicationName = QStringLiteral("QindaQt Test");
    notification.applicationIcon = QStringLiteral("qindaqt");
    notification.summary = QStringLiteral("A bounded notification");
    notification.body = QStringLiteral("Body");
    notification.urgency = 2;
    notification.desktopEntry = QStringLiteral("org.qindaqt.Test");
    notification.createdAtMs = 100;
    notification.updatedAtMs = 120;
    notification.actions = {{QStringLiteral("default"), QStringLiteral("Open")}};
    return {QStringLiteral("d9428888-122b-11e1-b85c-61cd3cbb3210"),
            9, {notification}};
}

} // namespace

class PresentationProtocolTests final : public QObject {
    Q_OBJECT

private slots:
    void tokenValidationAndComparison();
    void snapshotRoundTrips();
    void rejectsMalformedAndOversizedSnapshots();
};

void PresentationProtocolTests::tokenValidationAndComparison()
{
    QString error;
    const QString tokenText(64, QLatin1Char('a'));
    const auto token = PresentationAccessToken::fromHex(tokenText, &error);
    QVERIFY2(token.has_value(), qPrintable(error));
    QVERIFY(token->matches(tokenText));
    QVERIFY(!token->matches(QString(64, QLatin1Char('b'))));
    QVERIFY(!token->matches(tokenText.chopped(1)));
    QVERIFY(!PresentationAccessToken::fromHex(tokenText.toUpper(), &error));

    const auto generated = PresentationAccessToken::generate();
    QCOMPARE(generated.toHex().size(), 64);
    QVERIFY(generated.matches(generated.toHex()));
}

void PresentationProtocolTests::snapshotRoundTrips()
{
    const auto expected = snapshot();
    const auto decoded = PresentationSnapshotCodec::decode(
        PresentationSnapshotCodec::encode(expected));
    QVERIFY2(decoded.ok(), qPrintable(decoded.error));
    QCOMPARE(*decoded.snapshot, expected);
}

void PresentationProtocolTests::rejectsMalformedAndOversizedSnapshots()
{
    QVariantMap wire = PresentationSnapshotCodec::encode(snapshot());
    wire.insert(QStringLiteral("unknown"), true);
    QVERIFY(!PresentationSnapshotCodec::decode(wire).ok());

    wire = PresentationSnapshotCodec::encode(snapshot());
    wire[QStringLiteral("epoch")] = QStringLiteral("not-an-epoch");
    QVERIFY(!PresentationSnapshotCodec::decode(wire).ok());

    wire = PresentationSnapshotCodec::encode(snapshot());
    QVariantList items = wire.value(QStringLiteral("notifications")).toList();
    QVariantMap duplicate = items.constFirst().toMap();
    duplicate[QStringLiteral("body")] = QString(
        WireContract::MaximumBodyBytes + 1, QLatin1Char('x'));
    items.append(duplicate);
    wire[QStringLiteral("notifications")] = items;
    QVERIFY(!PresentationSnapshotCodec::decode(wire).ok());

    wire = PresentationSnapshotCodec::encode(snapshot());
    items = wire.value(QStringLiteral("notifications")).toList();
    duplicate = items.constFirst().toMap();
    duplicate[QStringLiteral("id")] = quint32(7);
    items.append(duplicate);
    wire[QStringLiteral("notifications")] = items;
    QVERIFY(!PresentationSnapshotCodec::decode(wire).ok());

    wire = PresentationSnapshotCodec::encode(snapshot());
    items = wire.value(QStringLiteral("notifications")).toList();
    QVariantMap invalidTime = items.constFirst().toMap();
    invalidTime[QStringLiteral("expiresAtMs")] = qint64(99);
    items[0] = invalidTime;
    wire[QStringLiteral("notifications")] = items;
    QVERIFY(!PresentationSnapshotCodec::decode(wire).ok());

    PresentationSnapshot oversized = snapshot();
    oversized.notifications.clear();
    for (quint32 id = 1;
         id <= quint32(WireContract::MaximumNotifications + 1); ++id) {
        auto item = snapshot().notifications.constFirst();
        item.id = id;
        oversized.notifications.append(std::move(item));
    }
    QVERIFY(!PresentationSnapshotCodec::decode(
                 PresentationSnapshotCodec::encode(oversized)).ok());
}

QTEST_GUILESS_MAIN(PresentationProtocolTests)
#include "tst_presentation_protocol.moc"
