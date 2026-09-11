// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcut_port.h>

#include <QtDBus/QDBusConnection>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "support/fake_kglobalaccel.h"
#include "support/private_bus.h"

using QindaQt::Apps::SettingsInput::keySequenceDisplay;
using QindaQt::Apps::SettingsInput::QtShortcutPort;
using QindaQt::Apps::SettingsInput::shortcutKeysFromInts;
using QindaQt::Apps::SettingsInput::shortcutKeysToInts;
using QindaQt::Apps::SettingsInput::ShortcutAction;
using QindaQt::Tests::FakeKGlobalAccel;
using QindaQt::Tests::PrivateBus;

namespace
{
int metaSpace() { return (Qt::META | Qt::Key_Space).toCombined(); }
int metaJ() { return (Qt::META | Qt::Key_J).toCombined(); }
int metaL() { return (Qt::META | Qt::Key_L).toCombined(); }
int ctrlAltL() { return (Qt::CTRL | Qt::ALT | Qt::Key_L).toCombined(); }
QKeySequence sequence(int key) { return QKeySequence(QKeyCombination::fromCombined(key)); }
const ShortcutAction *find(const QList<ShortcutAction> &actions, const QString &component,
                           const QString &action)
{
    for (const ShortcutAction &candidate : actions) {
        if (candidate.componentUnique == component && candidate.actionUnique == action) {
            return &candidate;
        }
    }
    return nullptr;
}
} // namespace

class ShortcutPortTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void failsClosedWithoutAuthority();
    void listsComponentsActionsAndKeys();
    void onlyRouteCommandComponentsCarryACommand();
    void setResetAndClearUseTheDaemonWireFormat();
    void refusedAssignmentIsReportedTruthfully();
    void keyEncodingMatchesKGlobalAccel();
    void customCommandRegistersTheLaunchAction();
    void customCommandFailuresLeaveNothingBehind();
    void removesOnlyCommandComponents();
    void malformedRepliesFailClosed();

private:
    bool writeFile(const QString &relative, const QString &contents)
    {
        QFile file(m_dir->filePath(relative));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write(contents.toUtf8()) >= 0;
    }
    int commandFileCount() const
    {
        return int(QDir(m_dir->filePath(QStringLiteral("kglobalaccel"))).entryList(QDir::Files).size());
    }
    std::unique_ptr<QTemporaryDir> m_dir;
};

void ShortcutPortTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void ShortcutPortTest::failsClosedWithoutAuthority()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;
    QVERIFY(port.actions(&error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("not reachable")));
    QVERIFY(!port.setShortcuts(QStringLiteral("ksmserver"), QStringLiteral("Lock Session"),
                               {sequence(metaL())}, &error));
}

void ShortcutPortTest::listsComponentsActionsAndKeys()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("qindaqt-shell"), QStringLiteral("QindaQt Shell"));
    fake.addAction(QStringLiteral("qindaqt-shell"), QStringLiteral("qindaqt_reveal_panels"),
                   QStringLiteral("Reveal QindaQt panels"), {metaSpace()}, {metaSpace()});
    fake.addAction(QStringLiteral("qindaqt-shell"), QStringLiteral("qindaqt_toggle_notifications"),
                   QStringLiteral("Notification center"), {},
                   {(Qt::META | Qt::Key_N).toCombined()});

    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;
    const QList<ShortcutAction> actions = port.actions(&error);
    QCOMPARE(error, QString());
    QCOMPARE(actions.size(), 2);
    QCOMPARE(actions.at(0).componentUnique, QStringLiteral("qindaqt-shell"));
    QCOMPARE(actions.at(0).componentFriendly, QStringLiteral("QindaQt Shell"));
    QCOMPARE(actions.at(0).actionUnique, QStringLiteral("qindaqt_reveal_panels"));
    QCOMPARE(actions.at(0).actionFriendly, QStringLiteral("Reveal QindaQt panels"));
    QCOMPARE(actions.at(0).active, QList<QKeySequence>{sequence(metaSpace())});
    // A disabled action keeps its recorded defaults so Reset stays possible.
    QVERIFY(actions.at(1).active.isEmpty());
    QCOMPARE(actions.at(1).defaults.size(), 1);
    // Listing reads the component objects, the only reliable key source.
    QVERIFY(fake.callOrder.contains(QStringLiteral("allShortcutInfos")));
    QVERIFY(!fake.callOrder.contains(QStringLiteral("shortcutKeys")));
}

void ShortcutPortTest::onlyRouteCommandComponentsCarryACommand()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    const QString logger = QStringLiteral("qindaqt-custom-logger.desktop");
    const QString konsole = QStringLiteral("org.kde.konsole.desktop");
    fake.addComponent(logger, QStringLiteral("logger"));
    fake.addAction(logger, QStringLiteral("_launch"), QStringLiteral("logger"), {ctrlAltL()}, {});
    fake.addComponent(konsole, QStringLiteral("Konsole"));
    fake.addAction(konsole, QStringLiteral("_launch"), QStringLiteral("Konsole"), {}, {});
    QVERIFY(QDir(m_dir->path()).mkpath(QStringLiteral("kglobalaccel")));
    QVERIFY(writeFile(QStringLiteral("kglobalaccel/qindaqt-custom-logger.desktop"),
                      QStringLiteral("[Desktop Entry]\nType=Application\nName=logger\n"
                                     "Exec=/usr/bin/logger hi\nNoDisplay=true\n")));
    QVERIFY(writeFile(QStringLiteral("kglobalaccel/org.kde.konsole.desktop"),
                      QStringLiteral("[Desktop Entry]\nExec=konsole\n")));

    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;
    const QList<ShortcutAction> actions = port.actions(&error);
    const ShortcutAction *command = find(actions, logger, QStringLiteral("_launch"));
    const ShortcutAction *application = find(actions, konsole, QStringLiteral("_launch"));
    QVERIFY(command != nullptr);
    QVERIFY(application != nullptr);
    QVERIFY(command->isCommandComponent());
    QCOMPARE(command->command, QStringLiteral("/usr/bin/logger hi"));
    // Negative control: an installed application's desktop component is not
    // a route command, so it carries no command and is never removable here.
    QVERIFY(!application->isCommandComponent());
    QVERIFY(application->command.isEmpty());
}

void ShortcutPortTest::setResetAndClearUseTheDaemonWireFormat()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    const QString component = QStringLiteral("ksmserver");
    const QString lock = QStringLiteral("Lock Session");
    fake.addComponent(component, QStringLiteral("Session Management"));
    fake.addAction(component, lock, lock, {metaL()}, {metaL()});

    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;
    QVERIFY2(port.setShortcuts(component, lock, {sequence(ctrlAltL())}, &error), qPrintable(error));
    // Identity order and four ints per sequence, exactly as KF6GlobalAccel reads them.
    QCOMPARE(fake.lastActionId, (QStringList{component, lock, QString(), QString()}));
    QCOMPARE(fake.lastSequences.size(), 1);
    QCOMPARE(fake.lastSequences.first(), (QList<int>{ctrlAltL(), 0, 0, 0}));
    QCOMPARE(fake.action(component, lock).active, QList<int>{ctrlAltL()});

    // Clear disables; the recorded defaults survive.
    QVERIFY2(port.setShortcuts(component, lock, {}, &error), qPrintable(error));
    QVERIFY(fake.lastSequences.isEmpty());
    QVERIFY(fake.action(component, lock).active.isEmpty());
    QCOMPARE(fake.action(component, lock).defaults.size(), 1);

    // Reset restores the default through the same call.
    QVERIFY2(port.setShortcuts(component, lock, {sequence(metaL())}, &error), qPrintable(error));
    QCOMPARE(fake.action(component, lock).active, QList<int>{metaL()});

    QCOMPARE(fake.malformedSequenceCalls, 0);
    QCOMPARE(fake.legacyIntegerCalls, 0);

    // Unknown and invalid actions fail closed.
    QVERIFY(!port.setShortcuts(component, QStringLiteral("No Such Action"), {sequence(metaJ())}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!port.setShortcuts(component, QString(), {sequence(metaJ())}, &error));
}

void ShortcutPortTest::refusedAssignmentIsReportedTruthfully()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("kwin"), QStringLiteral("KWin"));
    fake.addAction(QStringLiteral("kwin"), QStringLiteral("Window Close"),
                   QStringLiteral("Close Window"), {metaJ()}, {});
    fake.addAction(QStringLiteral("kwin"), QStringLiteral("Walk Through Windows"),
                   QStringLiteral("Walk Through Windows"), {}, {});

    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;
    // The daemon answers success but keeps Meta+J with its holder.
    QVERIFY(!port.setShortcuts(QStringLiteral("kwin"), QStringLiteral("Walk Through Windows"),
                               {sequence(metaJ())}, &error));
    QVERIFY(error.contains(QStringLiteral("kept")));
    QVERIFY(fake.action(QStringLiteral("kwin"), QStringLiteral("Walk Through Windows")).active.isEmpty());
    QCOMPARE(fake.action(QStringLiteral("kwin"), QStringLiteral("Window Close")).active,
             QList<int>{metaJ()});
}

void ShortcutPortTest::keyEncodingMatchesKGlobalAccel()
{
    // Live-verified encoding: Meta+Space = 0x10000020.
    QCOMPARE(shortcutKeysToInts({sequence(metaSpace())}).first(), 0x10000020);
    const QList<QKeySequence> decoded = shortcutKeysFromInts({0x10000020});
    QCOMPARE(decoded.size(), 1);
    QCOMPARE(decoded.first(), sequence(metaSpace()));
    // Zero is not a key; it must not become a sequence.
    QVERIFY(shortcutKeysFromInts({0}).isEmpty());
    QCOMPARE(keySequenceDisplay(sequence(metaSpace())),
             sequence(metaSpace()).toString(QKeySequence::NativeText));
}

void ShortcutPortTest::customCommandRegistersTheLaunchAction()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));

    QtShortcutPort port(bus.connection, m_dir->path());
    QString componentUnique;
    QString error;
    QVERIFY2(port.addCommandShortcut(QStringLiteral("Open Editor"), QStringLiteral("/usr/bin/editor %1"),
                                     {sequence(metaSpace())}, &componentUnique, &error),
             qPrintable(error));
    QCOMPARE(componentUnique, QStringLiteral("qindaqt-custom-open-editor.desktop"));
    QFile file(m_dir->filePath(QStringLiteral("kglobalaccel/qindaqt-custom-open-editor.desktop")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("Type=Application")));
    QVERIFY(contents.contains(QStringLiteral("Name=Open Editor")));
    QVERIFY(contents.contains(QStringLiteral("Exec=/usr/bin/editor %1")));
    QVERIFY(contents.contains(QStringLiteral("X-KDE-GlobalAccel-CommandShortcut=true")));
    // The launch action is registered before its key is assigned.
    const qsizetype registered = fake.callOrder.indexOf(QStringLiteral("doRegister"));
    const qsizetype assigned = fake.callOrder.indexOf(QStringLiteral("setForeignShortcutKeys"));
    QVERIFY(registered >= 0);
    QVERIFY(assigned > registered);
    QCOMPARE(fake.action(componentUnique, QStringLiteral("_launch")).active, QList<int>{metaSpace()});
    // The listing shows it as a command with its command line.
    const QList<ShortcutAction> actions = port.actions(&error);
    const ShortcutAction *added = find(actions, componentUnique, QStringLiteral("_launch"));
    QVERIFY(added != nullptr);
    QCOMPARE(added->command, QStringLiteral("/usr/bin/editor %1"));
}

void ShortcutPortTest::customCommandFailuresLeaveNothingBehind()
{
    QString componentUnique;
    QString error;
    {
        // No authority: nothing is written at all.
        PrivateBus bus;
        QVERIFY(bus.start());
        QtShortcutPort port(bus.connection, m_dir->path());
        QVERIFY(!port.addCommandShortcut(QStringLiteral("Open Editor"), QStringLiteral("/usr/bin/editor"),
                                         {sequence(metaSpace())}, &componentUnique, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(commandFileCount(), 0);
    }
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("kwin"), QStringLiteral("KWin"));
    fake.addAction(QStringLiteral("kwin"), QStringLiteral("Window Close"), QStringLiteral("Close Window"),
                   {metaJ()}, {});
    QtShortcutPort port(bus.connection, m_dir->path());

    // The daemon keeps the key with its holder: file and registration go away again.
    QVERIFY(!port.addCommandShortcut(QStringLiteral("Recorder"), QStringLiteral("/usr/bin/record"),
                                     {sequence(metaJ())}, &componentUnique, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(commandFileCount(), 0);
    QVERIFY(fake.unregistered.contains(
        QStringList{QStringLiteral("qindaqt-custom-recorder.desktop"), QStringLiteral("_launch")}));
    QVERIFY(!fake.hasAction(QStringLiteral("qindaqt-custom-recorder.desktop"), QStringLiteral("_launch")));

    // Hostile inputs fail before any write.
    QVERIFY(!port.addCommandShortcut(QString(), QStringLiteral("x"), {}, &componentUnique, &error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("name"), QString(), {}, &componentUnique, &error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("name"), QStringLiteral("two\nlines"), {},
                                     &componentUnique, &error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("!!!"), QStringLiteral("x"), {}, &componentUnique, &error));
    QCOMPARE(commandFileCount(), 0);

    // A second command with the same name never overwrites the first.
    QVERIFY2(port.addCommandShortcut(QStringLiteral("Once"), QStringLiteral("/usr/bin/once"), {},
                                     &componentUnique, &error),
             qPrintable(error));
    QVERIFY(!port.addCommandShortcut(QStringLiteral("Once"), QStringLiteral("/usr/bin/other"), {},
                                     &componentUnique, &error));
    QVERIFY(error.contains(QStringLiteral("already exists")));
    QCOMPARE(commandFileCount(), 1);
}

void ShortcutPortTest::removesOnlyCommandComponents()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("ksmserver"), QStringLiteral("Session"));
    fake.addComponent(QStringLiteral("org.kde.konsole.desktop"), QStringLiteral("Konsole"));

    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;
    QVERIFY(!port.removeCommandShortcut(QStringLiteral("ksmserver"), &error));
    QVERIFY(error.contains(QStringLiteral("Only command")));
    QVERIFY(!port.removeCommandShortcut(QStringLiteral("org.kde.konsole.desktop"), &error));
    QVERIFY(error.contains(QStringLiteral("Only command")));
    QVERIFY(!port.removeCommandShortcut(QStringLiteral("qindaqt-custom-gone.desktop"), &error));

    QString componentUnique;
    QVERIFY2(port.addCommandShortcut(QStringLiteral("Recorder"), QStringLiteral("/usr/bin/record"), {},
                                     &componentUnique, &error),
             qPrintable(error));
    QVERIFY2(port.removeCommandShortcut(componentUnique, &error), qPrintable(error));
    QVERIFY(!QFile::exists(m_dir->filePath(QStringLiteral("kglobalaccel/") + componentUnique)));
    QCOMPARE(fake.unregistered.last(), (QStringList{componentUnique, QStringLiteral("_launch")}));
    QVERIFY(!fake.hasAction(componentUnique, QStringLiteral("_launch")));
}

void ShortcutPortTest::malformedRepliesFailClosed()
{
    PrivateBus bus;
    QVERIFY(bus.start());
    FakeKGlobalAccel fake;
    QVERIFY(fake.publish(bus.connection));
    fake.addComponent(QStringLiteral("qindaqt-shell"), QStringLiteral("QindaQt Shell"));
    fake.addAction(QStringLiteral("qindaqt-shell"), QStringLiteral("qindaqt_reveal_panels"),
                   QStringLiteral("Reveal QindaQt panels"), {metaSpace()}, {metaSpace()});
    QtShortcutPort port(bus.connection, m_dir->path());
    QString error;

    // A component list with the wrong signature is refused before decoding;
    // reaching the next line at all proves the process did not abort.
    fake.malformedComponentList = true;
    QVERIFY(port.actions(&error).isEmpty());
    QVERIFY(!error.isEmpty());

    // A component answering with the wrong row shape is skipped the same way.
    fake.malformedComponentList = false;
    fake.malformedShortcutInfos = true;
    error.clear();
    QVERIFY(port.actions(&error).isEmpty());
    QVERIFY(!port.setShortcuts(QStringLiteral("qindaqt-shell"), QStringLiteral("qindaqt_reveal_panels"),
                               {sequence(metaJ())}, &error));
}

QTEST_MAIN(ShortcutPortTest)
#include "tst_shortcut_port.moc"
