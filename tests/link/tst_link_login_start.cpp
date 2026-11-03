// SPDX-License-Identifier: GPL-3.0-or-later
// Pins the QindaLink login start: the shipped XDG autostart entry must be eligible in a QindaQt
// session, must start the service only through D-Bus activation, and the systemd unit must never
// hang off graphical-session.target (QindaQt never activates it).
#include <qindaqt/session_autostart/autostart_catalog.h>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class LinkLoginStartTest : public QObject {
    Q_OBJECT
private slots:
    void entryIsEligibleInQindaQtOnly()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("autostart")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("bin")));
        QVERIFY(QFile::copy(QStringLiteral(QINDAQT_LINK_AUTOSTART_ENTRY),
                            root.filePath(QStringLiteral("autostart/qindaqt-link-service.desktop"))));
        QFile fake(root.filePath(QStringLiteral("bin/dbus-send")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

        QindaQt::SessionAutostart::ScanOptions options;
        options.systemDirectories = {root.filePath(QStringLiteral("autostart"))};
        options.executableDirectories = {root.filePath(QStringLiteral("bin"))};
        options.desktops = {QStringLiteral("QindaQt")};
        const auto entries = QindaQt::SessionAutostart::scan(options);
        QCOMPARE(entries.size(), 1);
        const auto &entry = entries.first();
        QVERIFY2(entry.eligible, qPrintable(entry.ineligibilityReason));
        QCOMPARE(entry.program, root.filePath(QStringLiteral("bin/dbus-send")));
        // Activation, never a direct launch of the service binary (it would bypass the unit).
        QVERIFY(entry.arguments.contains(QStringLiteral("--dest=org.qindaqt.Link1")));
        QVERIFY(entry.arguments.contains(QStringLiteral("org.freedesktop.DBus.Peer.Ping")));
        QVERIFY(!entry.exec.contains(QStringLiteral("qindaqt-link-service")));

        options.desktops = {QStringLiteral("KDE")};
        const auto other = QindaQt::SessionAutostart::scan(options);
        QCOMPARE(other.size(), 1);
        QVERIFY(!other.first().eligible);
    }

    void unitIsNotWantedByGraphicalSession()
    {
        QFile unit(QStringLiteral(QINDAQT_LINK_UNIT_TEMPLATE));
        QVERIFY(unit.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(unit.readAll());
        QStringList lines;  // the AGENT-GUARD comment itself names the forbidden target
        for (const QString &line : text.mid(text.indexOf(QStringLiteral("[Install]"))).split(u'\n')) {
            if (!line.startsWith(u'#'))
                lines << line;
        }
        const QString install = lines.join(u'\n');
        QVERIFY(install.contains(QStringLiteral("WantedBy=default.target")));
        QVERIFY(!install.contains(QStringLiteral("WantedBy=graphical-session.target")));
        QVERIFY(text.contains(QStringLiteral("ExecStart=@KDE_INSTALL_FULL_BINDIR@/qindaqt-link-service")));

        QFile bus(QStringLiteral(QINDAQT_LINK_DBUS_TEMPLATE));
        QVERIFY(bus.open(QIODevice::ReadOnly));
        const QString busText = QString::fromUtf8(bus.readAll());
        QVERIFY(busText.contains(QStringLiteral("Exec=@KDE_INSTALL_FULL_BINDIR@/qindaqt-link-service")));
        QVERIFY(busText.contains(QStringLiteral("SystemdService=qindaqt-link-service.service")));
    }
};

QTEST_GUILESS_MAIN(LinkLoginStartTest)
#include "tst_link_login_start.moc"
