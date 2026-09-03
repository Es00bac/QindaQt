// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_protocol/clipboard_dbus.h>
#include <qindaqt/services/clipboard_protocol/clipboard_validation.h>

#include <QtDBus/QDBusMetaType>
#include <QtTest/QTest>

using namespace QindaQt::Services;

class ClipboardProtocolTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        Clipboard::registerDBusTypes();
    }

    void fixedSignatures()
    {
        QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(
                     QMetaType::fromType<ClipboardModel::EntryId>())),
                 QStringLiteral("(uu)"));
        QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(
                     QMetaType::fromType<Clipboard::Snapshot>())),
                 QStringLiteral("(ututbbay)"));
        QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(
                     QMetaType::fromType<Clipboard::OperationResult>())),
                 QStringLiteral("(uuttuttuts)"));
    }

    void canonicalDescriptorSnapshot()
    {
        ClipboardModel::ClipboardEntryDescriptor entry;
        entry.id = {7, 2};
        entry.formats = {{QStringLiteral("text/plain"), 4}};
        entry.fingerprint = QByteArray(32, 'x');
        const auto encoded = ClipboardModel::encodeDescriptorList({entry});
        QVERIFY(encoded.accepted());
        Clipboard::Snapshot snapshot{.epoch = 9, .generation = 7,
                                     .historyEnabled = true, .privacyAllowed = true,
                                     .descriptorList = encoded.bytes};
        QVERIFY(Clipboard::validateSnapshot(snapshot).accepted);
        snapshot.generation = 8;
        QCOMPARE(Clipboard::validateSnapshot(snapshot).reasonCode,
                 QStringLiteral("entry-generation-mismatch"));
    }

    void hostileValuesFailClosed()
    {
        Clipboard::Snapshot snapshot{.epoch = 1, .generation = 1,
                                     .descriptorList = QByteArrayLiteral("hostile")};
        QCOMPARE(Clipboard::validateSnapshot(snapshot).reasonCode,
                 QStringLiteral("malformed-descriptors"));
        Clipboard::OperationRequest request;
        QCOMPARE(Clipboard::validateOperationRequest(request).reasonCode,
                 QStringLiteral("malformed-request"));
        Clipboard::OperationResult result;
        QCOMPARE(Clipboard::validateOperationResult(result).reasonCode,
                 QStringLiteral("malformed-result"));
    }
};

QTEST_GUILESS_MAIN(ClipboardProtocolTest)
#include "tst_clipboard_protocol.moc"
