// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_keyring/sensitive_clipboard.h>
#include <QClipboard>
#include <QGuiApplication>
#include <QTest>
#include <cstring>
using QindaQt::Apps::SettingsKeyring::SensitiveClipboard;
std::shared_ptr<qindaqt::keyring::SecureBuffer> value(){
    auto bytes=std::make_shared<qindaqt::keyring::SecureBuffer>(9);std::memcpy(bytes->bytes().data(),"synthetic",9);return bytes;
}
class ClipboardTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void markedProviderExpiresAndWipes(){
        SensitiveClipboard owner(nullptr,20);auto bytes=value();QVERIFY(owner.copy(bytes));
        auto *clipboard=QGuiApplication::clipboard();
        QVERIFY(clipboard->mimeData()->hasFormat("application/x-qindaqt-secret"));
        QCOMPARE(clipboard->text(),"synthetic");
        QTRY_COMPARE_WITH_TIMEOUT(bytes->size(),0U,1000);QVERIFY(!clipboard->mimeData() || !clipboard->mimeData()->hasFormat("application/x-qindaqt-secret"));
    }
    void clearingNeverErasesReplacement(){
        SensitiveClipboard owner;auto bytes=value();QVERIFY(owner.copy(bytes));
        QGuiApplication::clipboard()->setText("replacement fixture");
        owner.clear();QCOMPARE(QGuiApplication::clipboard()->text(),"replacement fixture");QCOMPARE(bytes->size(),0U);
        QGuiApplication::clipboard()->clear();
    }
    void destructionWipesAndInvalidOwnershipRefuses(){
        auto bytes=value();{SensitiveClipboard owner;QVERIFY(owner.copy(bytes));}
        QCOMPARE(bytes->size(),0U);SensitiveClipboard owner;QVERIFY(!owner.copy({}));
    }
};
QTEST_MAIN(ClipboardTest)
#include "tst_sensitive_clipboard.moc"
