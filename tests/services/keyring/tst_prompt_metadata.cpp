// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <QtTest>
#include <QDBusMetaType>
using namespace qindaqt::keyring::protocol;
class PromptMetadataTest final : public QObject {
    Q_OBJECT
    static PromptMetadata fixture() {
        PromptMetadata value;
        value.label = QString::fromUtf8("Synthetic café"); value.caller = ":1.42";
        value.executable = "/usr/bin/python3"; value.operation = "unlock";
        value.processId = 12345; value.userId = 1000;
        value.parents = {"/usr/bin/agent (PID 123)", "/usr/bin/shell (PID 12)"};
        return value;
    }
private Q_SLOTS:
    void requesterAndOperationRoundtrip() {
        registerWireTypes();
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<MetadataRows>())),"aa{sv}");
        const auto value = fixture();
        auto frame = encodePromptMetadata(value);
        const auto copy = decodePromptMetadata(frame);
        QCOMPARE(copy.label,value.label); QCOMPARE(copy.caller,value.caller);
        QCOMPARE(copy.executable,value.executable); QCOMPARE(copy.operation,value.operation);
        QCOMPARE(copy.processId,value.processId); QCOMPARE(copy.userId,value.userId);
        QCOMPARE(copy.parents,value.parents); QVERIFY(frame.isEmpty());
    }
    void hostileFramesAreRejectedAndWiped() {
        const auto valid = encodePromptMetadata(fixture());
        QList<QByteArray> cases{QByteArray(),valid.left(22),valid+"x",QByteArray(MaxPromptMetadataBytes+1,'x')};
        for (qsizetype size = 0; size < valid.size(); ++size) cases.append(valid.first(size));
        auto malformed = valid; malformed[3] = '1'; cases.append(malformed);
        malformed = valid; malformed[12] = char(0xff); malformed[13] = char(0xff); cases.append(malformed);
        malformed = valid; malformed[14] = char(0xff); cases.append(malformed);
        malformed = valid; malformed[14] = char(0); cases.append(malformed);
        for (auto frame : cases) {
            QVERIFY_EXCEPTION_THROWN(decodePromptMetadata(frame), std::runtime_error);
            QVERIFY(frame.isEmpty());
        }
        auto value = fixture(); value.executable = QString(4097,'x');
        QVERIFY_EXCEPTION_THROWN(encodePromptMetadata(value), std::runtime_error);
        value = fixture(); value.parents = QStringList(7, "parent");
        QVERIFY_EXCEPTION_THROWN(encodePromptMetadata(value), std::runtime_error);
    }
    void controlsAndBidiCannotRearrangeRequesterText() {
        QCOMPARE(promptDisplayText(QStringLiteral("bad\nExecutable:\t\u202eexe\u2066\u2028")),
                 QStringLiteral("bad\\u000aExecutable:\\u0009\\u202eexe\\u2066\\u2028"));
        QCOMPARE(promptDisplayText(QString::fromUtf8("café <b>program</b>")), QString::fromUtf8("café <b>program</b>"));
    }
};
QTEST_GUILESS_MAIN(PromptMetadataTest)
#include "tst_prompt_metadata.moc"
