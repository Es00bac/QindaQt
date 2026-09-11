// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <cstdio>
#include <QTemporaryDir>
#include <QTest>

#include "support/fake_kglobalaccel.h"
#include "support/private_bus.h"

using QindaQt::Apps::SettingsInput::keySequenceDisplay;
using QindaQt::Apps::SettingsInput::QtShortcutPort;
using QindaQt::Apps::SettingsInput::shortcutKeysFromInts;
using QindaQt::Apps::SettingsInput::shortcutKeysToInts;
using QindaQt::Apps::SettingsInput::ShortcutAction;
using QindaQt::Tests::FakeKGlobalAccel;
using QindaQt::Tests::PrivateBus;

class ShortcutPortTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void failsClosedWithoutAuthority();
    void listsComponentsActionsAndKeys();
    void commandComponentsCarryTheirCommand();
    void setResetAndClearRoundTrip();
    void keyEncodingMatchesKGlobalAccel();
    void customCommandWritesDesktopComponent();
    void customCommandFailureCleansUp();
    void removesOnlyCommandComponents();
    void malformedComponentListFailsClosed();

private:
    static QKeySequence metaSpace() { return QKeySequence(Qt::META | Qt::Key_Space); }
    bool writeFile(const QString &relative, const QString &contents)
    {
        QFile file(m_dir.filePath(relative));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write(contents.toUtf8()) >= 0;
    }
    QTemporaryDir m_dir;
};

void ShortcutPortTest::failsClosedWithoutAuthority()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QtShortcutPort port(bus.connection, m_dir.path());
    QString error;
    QVERIFY(port.actions(&error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("not reachable")));
}

void ShortcutPortTest::listsComponentsActionsAndKeys()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("qindaqt-shell"),
                      QStringLiteral("QindaQt Shell"));
    fake.addAction(QStringLiteral("qindaqt-shell"),
                   QStringLiteral("qindaqt_reveal_panels"),
                   QStringLiteral("Reveal QindaQt panels"),
                   {(Qt::META | Qt::Key_Space).toCombined()}, {(Qt::META | Qt::Key_Space).toCombined()});
    fake.addAction(QStringLiteral("qindaqt-shell"),
                   QStringLiteral("qindaqt_lock_session"),
                   QStringLiteral("Lock QindaQt session"),
                   {}, {(Qt::META | Qt::Key_L).toCombined()});

    QtShortcutPort port(bus.connection, m_dir.path());
    QString error;
    const QList<ShortcutAction> actions = port.actions(&error);
    QCOMPARE(error, QString());
    QCOMPARE(actions.size(), 2);
    QCOMPARE(actions.at(0).componentUnique, QStringLiteral("qindaqt-shell"));
    QCOMPARE(actions.at(0).componentFriendly, QStringLiteral("QindaQt Shell"));
    QCOMPARE(actions.at(0).actionUnique, QStringLiteral("qindaqt_reveal_panels"));
    QCOMPARE(actions.at(0).active.size(), 1);
    QCOMPARE(actions.at(0).active.first(), metaSpace());
    // A disabled action keeps its recorded defaults so Reset stays possible.
    QCOMPARE(actions.at(1).active.size(), 0);
    QCOMPARE(actions.at(1).defaults.size(), 1);
}

void ShortcutPortTest::commandComponentsCarryTheirCommand()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("qindaqt-custom-logger.desktop"),
                      QStringLiteral("qindaqt-custom-logger.desktop"));
    fake.addAction(QStringLiteral("qindaqt-custom-logger.desktop"),
                   QStringLiteral("qindaqt-custom-logger.desktop"),
                   QStringLiteral("qindaqt-custom-logger.desktop"),
                   {(Qt::CTRL | Qt::ALT | Qt::Key_L).toCombined()}, {});
    QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("kglobalaccel")));
    QVERIFY(writeFile(QStringLiteral("kglobalaccel/qindaqt-custom-logger.desktop"),
                      "[Desktop Entry]\nType=Application\nName=logger\n"
                      "Exec=/usr/bin/logger hi\nNoDisplay=true\n"));

    QtShortcutPort port(bus.connection, m_dir.path());
    QString error;
    const QList<ShortcutAction> actions = port.actions(&error);
    QCOMPARE(actions.size(), 1);
    QVERIFY(actions.first().isCommandComponent());
    QCOMPARE(actions.first().command, QStringLiteral("/usr/bin/logger hi"));
}

void ShortcutPortTest::setResetAndClearRoundTrip()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("ksmserver"), QStringLiteral("Session Management"));
    fake.addAction(QStringLiteral("ksmserver"),
                   QStringLiteral("Lock Session"),
                   QStringLiteral("Lock Session"),
                   {(Qt::META | Qt::Key_L).toCombined()}, {(Qt::META | Qt::Key_L).toCombined()});

    QtShortcutPort port(bus.connection, m_dir.path());
    QString error;
    // Reassign.
    QVERIFY(port.setShortcuts(QStringLiteral("ksmserver"),
                              QStringLiteral("Lock Session"),
                              {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L)},
                              &error));
    QCOMPARE(fake.setForeignCalls, 1);
    QCOMPARE(fake.actionList().first().active.first(),
             (Qt::CTRL | Qt::ALT | Qt::Key_L).toCombined());
    // Clear disables; defaults survive on the fake.
    QVERIFY(port.setShortcuts(QStringLiteral("ksmserver"),
                              QStringLiteral("Lock Session"), {}, &error));
    QVERIFY(fake.actionList().first().active.isEmpty());
    QVERIFY(fake.actionList().first().defaults.size() == 1);
    // Reset restores the default encoding through the same call.
    QVERIFY(port.setShortcuts(QStringLiteral("ksmserver"),
                              QStringLiteral("Lock Session"),
                              {QKeySequence(Qt::META | Qt::Key_L)}, &error));
    QCOMPARE(fake.actionList().first().active.first(),
             (Qt::META | Qt::Key_L).toCombined());

    // Unknown actions fail closed on the port side.
    QVERIFY(!port.setShortcuts(QStringLiteral("ksmserver"), QString(),
                               {metaSpace()}, &error));
    QVERIFY(!error.isEmpty());
}

void ShortcutPortTest::keyEncodingMatchesKGlobalAccel()
{
    // Live-verified encoding: Meta+Space = 0x10000020.
    QCOMPARE(shortcutKeysToInts({metaSpace()}).first(), 0x10000020);
    const QList<QKeySequence> decoded =
        shortcutKeysFromInts({0x10000020});
    QCOMPARE(decoded.size(), 1);
    QCOMPARE(decoded.first(), metaSpace());
    // Zero is not a key; it must not become a sequence.
    QVERIFY(shortcutKeysFromInts({0}).isEmpty());
    // Display text is delegated to Qt's native formatter.
    QCOMPARE(keySequenceDisplay(metaSpace()),
             QKeySequence(Qt::META | Qt::Key_Space)
                 .toString(QKeySequence::NativeText));
}

void ShortcutPortTest::customCommandWritesDesktopComponent()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));

    QtShortcutPort port(bus.connection, m_dir.path());
    QString componentUnique;
    QString error;
    QVERIFY(port.addCommandShortcut(QStringLiteral("Open Editor"),
                                    QStringLiteral("/usr/bin/editor %1"),
                                    {metaSpace()}, &componentUnique, &error));
    QCOMPARE(error, QString());
    QCOMPARE(componentUnique,
             QStringLiteral("qindaqt-custom-open-editor.desktop"));
    QFile file(m_dir.filePath(
        QStringLiteral("kglobalaccel/qindaqt-custom-open-editor.desktop")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("[Desktop Entry]")));
    QVERIFY(contents.contains(QStringLiteral("Type=Application")));
    QVERIFY(contents.contains(QStringLiteral("Name=Open Editor")));
    QVERIFY(contents.contains(QStringLiteral("Exec=/usr/bin/editor %1")));
    // The assignment went to the daemon under the component identity.
    QCOMPARE(fake.setForeignCalls, 1);
    QCOMPARE(fake.actionList().first().componentUnique, componentUnique);
}

void ShortcutPortTest::customCommandFailureCleansUp()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    // Earlier rows share the temp dir; start from a clean component dir.
    QDir(m_dir.filePath(QStringLiteral("kglobalaccel"))).removeRecursively();
    // No fake kglobalaccel: the assignment fails after the file is written.
    QtShortcutPort port(bus.connection, m_dir.path());
    QString componentUnique;
    QString error;
    QVERIFY(!port.addCommandShortcut(QStringLiteral("Open Editor"),
                                     QStringLiteral("/usr/bin/editor"),
                                     {metaSpace()}, &componentUnique,
                                     &error));
    QVERIFY(!error.isEmpty());
    // AGENT-GUARD: no registered-but-unassignable file may remain.
    QVERIFY(!QFile::exists(m_dir.filePath(
        QStringLiteral("kglobalaccel/qindaqt-custom-open-editor.desktop"))));

    // Hostile inputs fail before any write at all.
    QVERIFY(!port.addCommandShortcut(QString(), QStringLiteral("x"), {},
                                     &componentUnique, &error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("name"), QString(), {},
                                     &componentUnique, &error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("name"),
                                     QStringLiteral("two\nlines"), {},
                                     &componentUnique, &error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("!!!"),
                                     QStringLiteral("x"), {},
                                     &componentUnique, &error));
    QDir dir(m_dir.filePath(QStringLiteral("kglobalaccel")));
    if (dir.exists()) {
        QCOMPARE(dir.entryList(QDir::Files).size(), 0);
    }
}

void ShortcutPortTest::removesOnlyCommandComponents()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("ksmserver"), QStringLiteral("Session"));

    QtShortcutPort port(bus.connection, m_dir.path());
    QString error;
    // Program components are never removable here.
    QVERIFY(!port.removeCommandShortcut(QStringLiteral("ksmserver"), &error));
    QVERIFY(error.contains(QStringLiteral("Only command")));
    QVERIFY(!port.removeCommandShortcut(QStringLiteral("not-a-command"),
                                        &error));
    // A command component without a file fails closed.
    QVERIFY(!port.removeCommandShortcut(
        QStringLiteral("qindaqt-custom-gone.desktop"), &error));
    error.clear();

    // The real flow: create, then remove.
    QString componentUnique;
    QVERIFY(port.addCommandShortcut(QStringLiteral("Recorder"),
                                    QStringLiteral("/usr/bin/record"), {},
                                    &componentUnique, &error));
    QVERIFY(port.removeCommandShortcut(componentUnique, &error));
    QCOMPARE(error, QString());
    QVERIFY(!QFile::exists(m_dir.filePath(
        QStringLiteral("kglobalaccel/") + componentUnique)));
}

void ShortcutPortTest::malformedComponentListFailsClosed(){
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.malformedComponentList = true;

    QtShortcutPort port(bus.connection, m_dir.path());
    QString error;
    QVERIFY(port.actions(&error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(ShortcutPortTest)
#include "tst_shortcut_port.moc"
