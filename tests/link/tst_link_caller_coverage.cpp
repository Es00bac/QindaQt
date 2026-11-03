// SPDX-License-Identifier: GPL-3.0-or-later
// Coverage by default (ADR-0438): introspect the live service and require
// Denied from an untrusted caller for EVERY method of every
// org.qindaqt.Link1* interface that is not on the explicit open allowlist.
// A new Link1.* member (for example Link1.Setup) is therefore covered the
// moment it exists; forgetting the CallerAuthority check fails this test.
#include "support/scripted_phone.h"

#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtCore/QXmlStreamReader>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtTest/QtTest>

using namespace QindaQt::Link::Test;

namespace {

// Public metadata, by decision. Adding a name here is a security review item.
const QSet<QString> kOpenMethods = {QStringLiteral("Devices"), QStringLiteral("PendingPairings"),
                                    QStringLiteral("ListenPort"), QStringLiteral("Revision")};

struct Method { QString path, iface, name; QStringList inTypes; };

void walk(QDBusConnection &bus, const QString &path, QList<Method> *out)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.qindaqt.Link1"), path,
                                                       QStringLiteral("org.freedesktop.DBus.Introspectable"),
                                                       QStringLiteral("Introspect"));
    const QDBusMessage reply = bus.call(call, QDBus::Block, 10000);
    if (reply.type() != QDBusMessage::ReplyMessage) return;
    QXmlStreamReader xml(reply.arguments().value(0).toString());
    QString iface;
    Method current;
    bool inMethod = false;
    QStringList children;
    int depth = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            ++depth;
            const QString tag = xml.name().toString();
            if (tag == QLatin1String("interface")) {
                iface = xml.attributes().value(QLatin1String("name")).toString();
            } else if (tag == QLatin1String("method")) {
                inMethod = true;
                current = {path, iface, xml.attributes().value(QLatin1String("name")).toString(), {}};
            } else if (tag == QLatin1String("arg") && inMethod
                       && xml.attributes().value(QLatin1String("direction")).toString() != QLatin1String("out")) {
                current.inTypes.append(xml.attributes().value(QLatin1String("type")).toString());
            } else if (tag == QLatin1String("node") && xml.attributes().hasAttribute(QLatin1String("name"))
                       && depth == 2) {
                children.append(xml.attributes().value(QLatin1String("name")).toString());
            }
        } else if (xml.isEndElement()) {
            --depth;
            if (xml.name() == QLatin1String("method") && inMethod) {
                inMethod = false;
                if (iface.startsWith(QLatin1String("org.qindaqt.Link1"))) out->append(current);
            }
        }
    }
    for (const QString &child : std::as_const(children)) {
        walk(bus, (path == QLatin1String("/") ? QString() : path) + QLatin1Char('/') + child, out);
    }
}

// A gdbus argument of the right type with an inert value.
QString dummyFor(const QString &type)
{
    if (type.startsWith(QLatin1Char('a')) || type.startsWith(QLatin1Char('('))) return QStringLiteral("@%1 %2").arg(type, type.startsWith(QLatin1Char('a')) ? QStringLiteral("[]") : QStringLiteral("()"));
    if (type == QLatin1String("s") || type == QLatin1String("o")) return QStringLiteral("''");
    if (type == QLatin1String("b")) return QStringLiteral("false");
    if (type == QLatin1String("d")) return QStringLiteral("0.0");
    return QStringLiteral("0");
}

} // namespace

class LinkCallerCoverageTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void everyNonOpenMethodDeniesAnUntrustedCaller()
    {
        QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
        QTemporaryDir data, cfg, cache;
        QProcess service;
        service.setProcessChannelMode(QProcess::ForwardedChannels);
        service.start(QStringLiteral(QINDAQT_LINK_SERVICE_BINARY),
                      {QStringLiteral("--data-dir"), data.path(), QStringLiteral("--config-dir"), cfg.path(),
                       QStringLiteral("--cache-dir"), cache.path(), QStringLiteral("--port"), QStringLiteral("0"),
                       QStringLiteral("--no-adb"), QStringLiteral("--no-notifications"),
                       QStringLiteral("--recording-notifier"), data.filePath(QStringLiteral("n.jsonl")),
                       QStringLiteral("--trusted-caller"), QCoreApplication::applicationFilePath()});
        QVERIFY(service.waitForStarted());
        QDBusConnection bus = QDBusConnection::sessionBus();
        QList<Method> methods;
        QVERIFY(waitUntil([&] {
            methods.clear();
            walk(bus, QStringLiteral("/org/qindaqt"), &methods);
            return !methods.isEmpty();
        }, 10000));

        // The sweep must actually see the known surface, or an empty walk would pass vacuously.
        QStringList names;
        for (const Method &m : methods) names << m.iface + QLatin1Char('.') + m.name;
        for (const char *expected : {"org.qindaqt.Link1.ConfirmPairing", "org.qindaqt.Link1.SetLanEnabled",
                                     "org.qindaqt.Link1.OpenPairingWindow", "org.qindaqt.Link1.SetLanInterfaces", "org.qindaqt.Link1.Messages.Send",
                                     "org.qindaqt.Link1.Notifications.Current"}) {
            QVERIFY2(names.contains(QLatin1String(expected)), expected);
        }

        int checked = 0;
        for (const Method &m : std::as_const(methods)) {
            if (kOpenMethods.contains(m.name)) continue;
            QStringList args{QStringLiteral("call"), QStringLiteral("--session"), QStringLiteral("--dest"),
                             QStringLiteral("org.qindaqt.Link1"), QStringLiteral("--object-path"), m.path,
                             QStringLiteral("--method"), m.iface + QLatin1Char('.') + m.name};
            for (const QString &t : m.inTypes) args << dummyFor(t);
            QProcess gdbus;  // a different executable than this test binary: untrusted
            gdbus.setProcessChannelMode(QProcess::MergedChannels);
            gdbus.start(QStringLiteral("/usr/bin/gdbus"), args);
            QVERIFY(gdbus.waitForFinished(15000));
            const QString out = QString::fromUtf8(gdbus.readAll());
            QVERIFY2(out.contains(QLatin1String("org.qindaqt.Link1.Error.Denied")),
                     qPrintable(m.iface + QLatin1Char('.') + m.name + QLatin1Char(' ') + out));
            ++checked;
        }
        qInfo("introspection sweep: %d methods total, %d protected methods denied", int(methods.size()), checked);
        QVERIFY(checked >= 14);  // 12 before LAN mode, plus SetLanEnabled and OpenPairingWindow
        service.terminate();  // our own child, by handle
        QVERIFY(service.waitForFinished(5000));
    }
};

QTEST_GUILESS_MAIN(LinkCallerCoverageTest)
#include "tst_link_caller_coverage.moc"
