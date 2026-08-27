// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_value_codec.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDateTime>
#include <QtTest>

using namespace QindaQt::Services::SettingsProtocol;

class SettingsProtocolTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptsEveryNestedJsonNativeShape();
    void rejectsDepthNodeCountAndAggregateByteOverflow();
    void rejectsOversizedListsMapsStringsAndNonJsonTypes();
    void wireStatusesAreExactAndStable();
};

void SettingsProtocolTests::acceptsEveryNestedJsonNativeShape()
{
    const QVariantMap metadata{{QStringLiteral("serial"), QVariant{}},
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
        nodes.append(QVariantList(8, true));
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
