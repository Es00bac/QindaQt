// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <QDir>
#include <QDBusMetaType>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include "support/fake_native_shortcuts.h"
#include "support/private_bus.h"
using namespace QindaQt::Apps::SettingsInput;
using namespace QindaQt::Tests;
class ShortcutPortTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void absentAuthorityFailsClosed() {
        PrivateBus bus; QVERIFY(bus.start()); QTemporaryDir dir; QtShortcutPort port(bus.connection, dir.path()); QString error;
        QVERIFY(port.actions(&error).isEmpty()); QVERIFY(!error.isEmpty()); QVERIFY(!port.setShortcuts("shell", "launcher", {QKeySequence("Meta+Space")}, &error));
        QVERIFY(!port.addCommandShortcut("Example", "true", {}, nullptr, &error)); QVERIFY(!QDir(dir.filePath("kglobalaccel")).exists());
    }
    void nativeListPreservesDisabledDefaultsAndSequences() {
        PrivateBus bus; QVERIFY(bus.start()); FakeNativeShortcuts fake; QVERIFY(fake.publish(bus.connection));
        fake.add("shell", "launcher", {"Meta+Space"}, {"Alt+F1"}); fake.add("shell", "disabled", {}, {"Ctrl+K, Ctrl+C"});
        QTemporaryDir dir; QtShortcutPort port(bus.connection, dir.path()); QString error; const auto rows = port.actions(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(rows.size(), 2); QCOMPARE(rows[0].actionUnique, QStringLiteral("disabled")); QVERIFY(rows[0].active.isEmpty());
        QCOMPARE(rows[0].defaults, QList<QKeySequence>{QKeySequence("Ctrl+K, Ctrl+C")}); QCOMPARE(rows[1].componentFriendly, QStringLiteral("QindaQt Shell"));
        QCOMPARE(fake.calls, QStringList{"ListBindings"});
    }
    void setResetClearAndNamedConflict() {
        PrivateBus bus; QVERIFY(bus.start()); FakeNativeShortcuts fake; QVERIFY(fake.publish(bus.connection)); fake.add("shell", "launcher", {"Meta+Space"}, {"Alt+F1"});
        QTemporaryDir dir; QtShortcutPort port(bus.connection, dir.path()); QString error;
        QVERIFY(port.setShortcuts("shell", "launcher", {QKeySequence("Ctrl+K, Ctrl+C")}, &error));
        QCOMPARE(port.actions(&error).first().active, QList<QKeySequence>{QKeySequence("Ctrl+K, Ctrl+C")});
        QVERIFY(port.setShortcuts("shell", "launcher", {}, &error)); QVERIFY(port.actions(&error).first().active.isEmpty());
        QVERIFY(port.setShortcuts("shell", "launcher", {QKeySequence("Alt+F1")}, &error));
        fake.refuse = true; QVERIFY(!port.setShortcuts("shell", "launcher", {QKeySequence("Meta+L")}, &error)); QVERIFY(error.contains("Lock session"));
        QCOMPARE(port.actions(nullptr).first().active, QList<QKeySequence>{QKeySequence("Alt+F1")});
    }
    void commandLifecycleAndRollback() {
        PrivateBus bus; QVERIFY(bus.start()); FakeNativeShortcuts fake; QVERIFY(fake.publish(bus.connection));
        QTemporaryDir dir; QtShortcutPort port(bus.connection, dir.path()); QString component, error;
        QVERIFY(port.addCommandShortcut("My terminal", "qindaqt-terminal --new-window", {QKeySequence("Ctrl+Alt+T")}, &component, &error));
        QVERIFY(component.startsWith(CommandComponentPrefix)); QCOMPARE(port.actions(&error).first().command, QStringLiteral("qindaqt-terminal --new-window"));
        QVERIFY(fake.calls.contains("Register")); QVERIFY(port.removeCommandShortcut(component, &error)); QVERIFY(!QFile::exists(dir.filePath("kglobalaccel/" + component)));
        fake.refuse = true; QVERIFY(!port.addCommandShortcut("Refused", "true", {QKeySequence("Meta+L")}, nullptr, &error)); QVERIFY(QDir(dir.filePath("kglobalaccel")).entryList(QDir::Files).isEmpty());
        QVERIFY(!port.removeCommandShortcut("some-third-party.desktop", &error));
    }
    void invalidCommandAndMalformedReplies() {
        PrivateBus bus; QVERIFY(bus.start()); FakeNativeShortcuts fake; QVERIFY(fake.publish(bus.connection)); QTemporaryDir dir; QtShortcutPort port(bus.connection, dir.path()); QString error;
        QVERIFY(!port.addCommandShortcut("Bad\nname", "true", {}, nullptr, &error)); QVERIFY(!port.addCommandShortcut("Bad", "true\nfalse", {}, nullptr, &error));
        fake.malformed = true; QVERIFY(port.actions(&error).isEmpty()); QVERIFY(!error.isEmpty());
        fake.malformed = false; fake.rows.insert("bad-id", QVariantMap{{"component", "shell"}, {"action", "launcher"}, {"keys", QStringList{}}, {"defaults", QStringList{}}});
        error.clear(); QVERIFY(port.actions(&error).isEmpty()); QVERIFY(error.contains("Invalid"));
    }
    void compatibilityCodecRemainsFourChords() {
        const QList<QKeySequence> keys{QKeySequence("Meta+Space")}; QCOMPARE(shortcutKeysFromInts(shortcutKeysToInts(keys)), keys);
        QCOMPARE(keySequenceDisplay(keys.first()), keys.first().toString(QKeySequence::NativeText));
        registerShortcutDBusTypes(); QCOMPARE(QDBusMetaType::typeToSignature(QMetaType::fromType<ShortcutKeySequence>()), "(ai)");
    }
};
QTEST_GUILESS_MAIN(ShortcutPortTest)
#include "tst_shortcut_port.moc"
