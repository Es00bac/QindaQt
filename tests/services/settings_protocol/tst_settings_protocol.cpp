// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_encode.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDateTime>
#include <QDBusSignature>
#include <QtTest>

#include <limits>

using namespace QindaQt::Services::SettingsProtocol;

class SettingsProtocolTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptsEveryNestedJsonNativeShape();
    void rejectsDepthNodeCountAndAggregateByteOverflow();
    void rejectsOversizedListsMapsStringsAndNonJsonTypes();
    void roundTripsCanonicalNullAndIntegerDomain();
    void wireStatusesAreExactAndStable();
};

void SettingsProtocolTests::acceptsEveryNestedJsonNativeShape()
{
    const QVariantMap metadata{{QStringLiteral("serial"), QVariant::fromValue(nullptr)},
                               {QStringLiteral("tags"),
                                QStringList{QStringLiteral("desk")}}};
    const QVariantMap output{{QStringLiteral("name"), QStringLiteral("HDMI-A-1")},
                             {QStringLiteral("enabled"), true},
                             {QStringLiteral("scale"), 1.25},
                             {QStringLiteral("position"), QVariantList{0, 0}},
                             {QStringLiteral("metadata"), metadata}};
    const QVariantMap display{{QStringLiteral("outputs"), QVariantList{output}},
                              {QStringLiteral("layout"), QStringLiteral("extended")}};
    BoundedSettingsValueCodec::Usage usage;
    QString error;
    QVERIFY2(BoundedSettingsValueCodec::validateValue(display, &error, &usage), qPrintable(error));
    QVERIFY(usage.nodes > 10);
    QVERIFY(usage.maximumDepth >= 5);
}

void SettingsProtocolTests::roundTripsCanonicalNullAndIntegerDomain()
{
    const QVariantMap value{
        {QStringLiteral("null"), QVariant::fromValue(nullptr)},
        {QStringLiteral("minimum"), QVariant::fromValue(std::numeric_limits<qint64>::min())},
        {QStringLiteral("unsignedInRange"),
         QVariant::fromValue(quint64(std::numeric_limits<qint64>::max()))},
        {QStringLiteral("array"),
         QVariantList{QVariant::fromValue(nullptr), quint32(4'000'000'000U), 3.5}}};
    QString error;
    auto encoded = encodeBoundedJsonValueForWire(value, &error);
    QVERIFY2(encoded.has_value(), qPrintable(error));
    const QVariantMap wire = encoded->toMap();
    QCOMPARE(wire.value(QStringLiteral("null")).metaType(),
             QMetaType::fromType<QDBusSignature>());
    QCOMPARE(qvariant_cast<QDBusSignature>(wire.value(QStringLiteral("null"))).signature(),
             QString::fromLatin1(WireContract::JsonNullWireSignature));

    const auto decoded = decodeBoundedJsonValue(*encoded, &error);
    QVERIFY2(decoded.has_value(), qPrintable(error));
    const QVariantMap result = decoded->toMap();
    QCOMPARE(result.value(QStringLiteral("null")).metaType().id(), QMetaType::Nullptr);
    QCOMPARE(result.value(QStringLiteral("minimum")).metaType().id(), QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("minimum")).toLongLong(),
             std::numeric_limits<qint64>::min());
    QCOMPARE(result.value(QStringLiteral("unsignedInRange")).metaType().id(),
             QMetaType::LongLong);
    const QVariantList array = result.value(QStringLiteral("array")).toList();
    QCOMPARE(array.at(0).metaType().id(), QMetaType::Nullptr);
    QCOMPARE(array.at(1).metaType().id(), QMetaType::LongLong);
    QCOMPARE(array.at(1).toLongLong(), qint64(4'000'000'000ULL));
    QCOMPARE(array.at(2).metaType().id(), QMetaType::Double);

    QVERIFY(!BoundedSettingsValueCodec::validateValue(QVariant{}, &error));
    QVERIFY(!encodeBoundedJsonValueForWire(
        QVariant::fromValue(QDBusSignature(
            QString::fromLatin1(WireContract::JsonNullWireSignature))), &error).has_value());
    QVERIFY(!encodeBoundedJsonValueForWire(
        QVariant::fromValue(std::numeric_limits<quint64>::max()), &error).has_value());
}

void SettingsProtocolTests::rejectsDepthNodeCountAndAggregateByteOverflow()
{
    QVariant depth = true;
    for (qsizetype i = 0; i <= WireContract::MaximumValueDepth; ++i) {
        depth = QVariantList{depth};
    }
    QString error;
    QVERIFY(!BoundedSettingsValueCodec::validateValue(depth, &error));
    QVERIFY(error.contains(QStringLiteral("depth")));

    QVariantList nodes;
    nodes.reserve(WireContract::MaximumListEntries);
    for (qsizetype i = 0; i < WireContract::MaximumListEntries; ++i) {
        nodes.append(QVariant::fromValue(QVariantList(8, true)));
    }
    error.clear();
    QVERIFY(!BoundedSettingsValueCodec::validateValue(nodes, &error));
    QVERIFY2(!error.isEmpty(), "node overflow must return a bounded diagnostic");

    QVariantList bytes;
    const QString chunk(16'000, QLatin1Char('x'));
    for (int i = 0; i < 20; ++i) {
        bytes.append(chunk);
    }
    error.clear();
    QVERIFY(!BoundedSettingsValueCodec::validateValue(bytes, &error));
    QVERIFY(error.contains(QStringLiteral("aggregate bytes")));
}

void SettingsProtocolTests::rejectsOversizedListsMapsStringsAndNonJsonTypes()
{
    QString error;
    QVERIFY(!BoundedSettingsValueCodec::validateValue(
        QString(WireContract::MaximumStringValueBytes + 1, QLatin1Char('x')), &error));
    QVERIFY(error.contains(QStringLiteral("UTF-8 bytes")));

    QVariantList list;
    list.resize(WireContract::MaximumListEntries + 1);
    QVERIFY(!BoundedSettingsValueCodec::validateValue(list, &error));
    QVERIFY(error.contains(QStringLiteral("list exceeds")));

    QVariantMap map;
    for (qsizetype i = 0; i <= WireContract::MaximumMapEntries; ++i) {
        map.insert(QString::number(i), true);
    }
    QVERIFY(!BoundedSettingsValueCodec::validateValue(map, &error));
    QVERIFY(error.contains(QStringLiteral("object exceeds")));
    QVERIFY(!BoundedSettingsValueCodec::validateValue(QVariant::fromValue(QDateTime::currentDateTime()), &error));

    const QString loneHigh(1, QChar(0xd800));
    const QString loneLow(1, QChar(0xdc00));
    QString embeddedNul = QStringLiteral("ab");
    embeddedNul.insert(1, QChar::Null);
    QVERIFY(!BoundedSettingsValueCodec::validateValue(loneHigh, &error));
    QVERIFY(!BoundedSettingsValueCodec::validateValue(loneLow, &error));
    QVERIFY(!BoundedSettingsValueCodec::validateValue(embeddedNul, &error));
    QVariantMap collision{{loneHigh, true},
                          {QString(QChar::ReplacementCharacter), false}};
    QVERIFY(!BoundedSettingsValueCodec::validateValue(collision, &error));
}

void SettingsProtocolTests::wireStatusesAreExactAndStable()
{
    for (quint32 ordinal = 0; ordinal <= 8; ++ordinal) {
        QVERIFY(fromWireStatus(ordinal).has_value());
    }
    QVERIFY(!fromWireStatus(9).has_value());
    QCOMPARE(settingsWireStatusName(SettingsWireStatus::RevisionExhausted),
             QStringLiteral("revision-exhausted"));
}

QTEST_GUILESS_MAIN(SettingsProtocolTests)
#include "tst_settings_protocol.moc"
