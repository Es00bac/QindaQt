// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_identity/identity_resolver.h>
#include <qindaqt/services/display_identity/identity_limits.h>

#include "support/identity_test_data.h"

#include <QtTest>

using namespace QindaQt::DisplayIdentity;

class IdentityResolverTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void precedenceAndPrivacy();
    void uniqueAndDuplicateRawEdid();
    void mstAndConnectorFallbacks();
    void malformedAndEmptyEdid();
    void connectorRenameAndRuntimeUuidChange();
    void collisionSuffixingIsDeterministic();
    void rejectsHostileInputs();
};

void IdentityResolverTests::precedenceAndPrivacy()
{
    const ObservedOutput input = Test::observed();
    const ResolutionResult result = resolve({input});
    QVERIFY2(result.succeeded(), qPrintable(result.reasonCode));
    QCOMPARE(result.outputs.size(), 1);
    QCOMPARE(result.outputs.first().source, IdentitySource::EdidIdentifier);
    QVERIFY(result.outputs.first().stableId.startsWith(QStringLiteral("edid:")));
    QVERIFY(!result.outputs.first().stableId.contains(QStringLiteral("SERIAL-SECRET")));
    QVERIFY(!result.outputs.first().stableId.contains(QString::fromLatin1(input.rawEdid.toHex())));
    QCOMPARE(result.outputs.first().hasSerial, true);
}

void IdentityResolverTests::uniqueAndDuplicateRawEdid()
{
    ObservedOutput first = Test::observed(QStringLiteral("DP-1"), {}, QByteArray("raw-a"));
    ObservedOutput second = Test::observed(QStringLiteral("DP-2"), {}, QByteArray("raw-b"));
    first.hasSerial = false;
    second.hasSerial = false;
    ResolutionResult result = resolve({first, second});
    QVERIFY(result.succeeded());
    QCOMPARE(result.outputs[0].source, IdentitySource::RawEdid);
    QCOMPARE(result.outputs[1].source, IdentitySource::RawEdid);
    QVERIFY(!result.outputs[0].ambiguous);

    second.rawEdid = first.rawEdid;
    result = resolve({first, second});
    QVERIFY(result.succeeded());
    QCOMPARE(result.outputs[0].source, IdentitySource::Connector);
    QCOMPARE(result.outputs[1].source, IdentitySource::Connector);
    QVERIFY(result.outputs[0].ambiguous);
    QVERIFY(result.outputs[1].ambiguous);
    QVERIFY(result.outputs[0].stableId != result.outputs[1].stableId);
}

void IdentityResolverTests::mstAndConnectorFallbacks()
{
    ObservedOutput first = Test::observed(QStringLiteral("DP-1"), QByteArray("same"),
                                          QByteArray("same-raw"));
    ObservedOutput second = Test::observed(QStringLiteral("DP-2"), QByteArray("same"),
                                           QByteArray("same-raw"));
    first.hasSerial = false;
    second.hasSerial = false;
    first.mstPath = QStringLiteral("mst/1");
    second.mstPath = QStringLiteral("mst/2");
    ResolutionResult result = resolve({first, second});
    QVERIFY(result.succeeded());
    QCOMPARE(result.outputs[0].source, IdentitySource::MstPath);
    QCOMPARE(result.outputs[1].source, IdentitySource::MstPath);
    QVERIFY(result.outputs[0].ambiguous);

    ObservedOutput unicode;
    unicode.connectorName = QStringLiteral("显示器端口");
    unicode.edidState = EdidState::Absent;
    result = resolve({unicode});
    QVERIFY(result.succeeded());
    QCOMPARE(result.outputs.first().source, IdentitySource::ConnectorHash);
    QVERIFY(result.outputs.first().stableId.startsWith(QStringLiteral("connhash:")));
}

void IdentityResolverTests::malformedAndEmptyEdid()
{
    ObservedOutput malformed = Test::observed();
    malformed.edidState = EdidState::Malformed;
    malformed.edidIdentifier = QByteArray("untrusted-secret");
    malformed.rawEdid = QByteArray("malformed-private-bytes");
    ResolutionResult result = resolve({malformed});
    QVERIFY(result.succeeded());
    QCOMPARE(result.outputs.first().source, IdentitySource::Connector);
    QVERIFY(!result.outputs.first().stableId.contains(QStringLiteral("untrusted")));
    QVERIFY(!result.outputs.first().hasSerial);

    ObservedOutput absent;
    absent.connectorName = QStringLiteral("HDMI-A-1");
    absent.edidState = EdidState::Absent;
    result = resolve({absent});
    QVERIFY(result.succeeded());
    QCOMPARE(result.outputs.first().stableId, QStringLiteral("conn:HDMI-A-1"));
}

void IdentityResolverTests::connectorRenameAndRuntimeUuidChange()
{
    ObservedOutput first = Test::observed();
    ObservedOutput renamed = first;
    renamed.connectorName = QStringLiteral("DP-7");
    renamed.runtimeCompositorUuid = QStringLiteral("completely-different-runtime-uuid");
    const auto before = resolve({first});
    const auto after = resolve({renamed});
    QVERIFY(before.succeeded());
    QVERIFY(after.succeeded());
    QCOMPARE(after.outputs.first().stableId, before.outputs.first().stableId);
}

void IdentityResolverTests::collisionSuffixingIsDeterministic()
{
    ObservedOutput first = Test::observed(QStringLiteral("DP-1"), QByteArray("unique-a"),
                                          QByteArray("raw-a"));
    ObservedOutput second = Test::observed(QStringLiteral("DP-2"), QByteArray("unique-b"),
                                           QByteArray("raw-b"));
    const DigestFunction collision = [](QByteArrayView) {
        return QByteArray(kDigestBytes, '\x55');
    };
    const ResolutionResult result = resolve({first, second}, collision);
    QVERIFY(result.succeeded());
    QVERIFY(result.outputs[0].stableId.endsWith(QStringLiteral("#1")));
    QVERIFY(result.outputs[1].stableId.endsWith(QStringLiteral("#2")));
    QVERIFY(result.outputs[0].ambiguous);
    QVERIFY(result.outputs[1].ambiguous);
}

void IdentityResolverTests::rejectsHostileInputs()
{
    ObservedOutput invalid = Test::observed();
    invalid.connectorName = QString(129, QLatin1Char('x'));
    QCOMPARE(resolve({invalid}).error, IdentityError::InvalidConnector);
    invalid = Test::observed();
    invalid.rawEdid = QByteArray(4'097, '\0');
    QCOMPARE(resolve({invalid}).error, IdentityError::InvalidEdidMaterial);
    invalid = Test::observed();
    invalid.edidState = EdidState::Absent;
    QCOMPARE(resolve({invalid}).error, IdentityError::InvalidEdidMaterial);

    QList<ObservedOutput> tooMany;
    for (qsizetype index = 0; index <= kMaxConnectedOutputs; ++index) {
        tooMany.push_back(Test::observed(QStringLiteral("DP-%1").arg(index),
                                         QByteArray("id-%1").replace("%1", QByteArray::number(index)),
                                         QByteArray("raw-%1").replace("%1", QByteArray::number(index))));
    }
    QCOMPARE(resolve(tooMany).error, IdentityError::TooManyOutputs);
}

QTEST_GUILESS_MAIN(IdentityResolverTests)
#include "tst_identity_resolver.moc"
