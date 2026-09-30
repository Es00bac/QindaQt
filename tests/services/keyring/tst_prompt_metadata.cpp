// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <QDBusMetaType>
#include <QtTest>
using namespace qindaqt::keyring::protocol;
class PromptMetadataTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void boundedUnicodeRoundtripAndCanonicalWireType() {
        registerWireTypes();
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<MetadataRows>())),"aa{sv}");
        const PromptMetadata value{QString::fromUtf8("Synthetic café"),"Synthetic application"};
        auto frame=encodePromptMetadata(value);const auto copy=decodePromptMetadata(frame);
        QCOMPARE(copy.label,value.label);QCOMPARE(copy.caller,value.caller);QVERIFY(frame.isEmpty());
    }
    void hostileFramesRejectBeforeUnboundedCopies() {
        auto valid=encodePromptMetadata({"label","caller"});
        QList<QByteArray> cases{QByteArray(),valid.left(7),valid+"x",QByteArray(1289,'x')};
        auto malformed=valid;malformed[0]='X';cases.append(malformed);
        malformed=valid;malformed[4]=char(0xff);malformed[5]=char(0xff);cases.append(malformed);
        malformed=valid;malformed[8]=char(0xff);cases.append(malformed);
        malformed=valid;malformed[8]=char(0);cases.append(malformed);
        for(auto frame:cases) {
            bool rejected=false;try { (void)decodePromptMetadata(frame); } catch(const std::exception &) { rejected=true; }
            QVERIFY(rejected);
        }
        bool rejected=false;
        try { (void)encodePromptMetadata({QString(1025,'x'),"caller"}); } catch(const std::exception &) { rejected=true; }
        QVERIFY(rejected);
    }
};
QTEST_GUILESS_MAIN(PromptMetadataTest)
#include "tst_prompt_metadata.moc"
