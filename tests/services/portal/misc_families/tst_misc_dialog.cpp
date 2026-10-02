// SPDX-License-Identifier: GPL-3.0-or-later
#include "misc_dialog.h"
#include <QCheckBox>
#include <QJsonArray>
#include <QDialogButtonBox>
#include <QImage>
#include <QBuffer>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>
class MiscDialogTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void accountCannotGrantBeforeParentAndReturnsSelections() {
        MiscDialog dialog(QJsonObject{{"type", "account"}, {"title", "Account"}, {"id", "fixture"}, {"name", "Fixture Person"}, {"image", "file://"}});
        QVERIFY(!dialog.isEnabled()); dialog.show(); dialog.markReady(); QVERIFY(dialog.isEnabled());
        auto *name = dialog.findChild<QCheckBox *>("name"); QVERIFY(name); name->setChecked(false);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); QCOMPARE(dialog.response(), 0); QCOMPARE(dialog.results().value("id").toBool(), true); QCOMPARE(dialog.results().value("name").toBool(), false);
    }
    void usbCheckboxSubsetAndCancel() {
        const QJsonObject frame{{"type", "usb"}, {"title", "USB"}, {"devices", QJsonArray{QJsonObject{{"id", "one"}, {"label", "Fixture one"}, {"writable", true}}, QJsonObject{{"id", "two"}, {"label", "Fixture two"}, {"writable", false}}}}};
        MiscDialog dialog(frame); dialog.markReady(); dialog.findChild<QCheckBox *>("two")->setChecked(false); dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); QCOMPARE(dialog.response(), 0); QCOMPARE(dialog.results().value("devices").toArray(), QJsonArray{"one"});
        MiscDialog cancelled(frame); cancelled.markReady(); cancelled.reject(); QCOMPARE(cancelled.response(), 1); QVERIFY(cancelled.results().isEmpty());
    }
    void launcherActualIconNameAndForeignLoss() {
        QImage image(16, 16, QImage::Format_ARGB32); image.fill(Qt::blue); QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(image.save(&buffer, "PNG"));
        const QJsonObject frame{{"type", "launcher"}, {"title", "Launcher"}, {"name", "Fixture"}, {"icon", QString::fromLatin1(bytes.toBase64())}, {"editable_name", true}};
        MiscDialog dialog(frame); dialog.markReady(); auto *name = dialog.findChild<QLineEdit *>("launcherName"); QVERIFY(name); name->setText("Chosen"); dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); QCOMPARE(dialog.response(), 0); QCOMPARE(dialog.results().value("name").toString(), QStringLiteral("Chosen"));
        MiscDialog lost(frame); lost.markReady(); lost.fail(); QCOMPARE(lost.response(), 2); QVERIFY(lost.results().isEmpty());
        MiscDialog invalid(QJsonObject{{"type", "launcher"}, {"name", "Bad"}, {"icon", "invalid"}}); invalid.markReady(); QCOMPARE(invalid.response(), 2);
    }
};
QTEST_MAIN(MiscDialogTest)
#include "tst_misc_dialog.moc"
