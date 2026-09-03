#include <QtCore/QObject>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusVirtualObject>
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>

class FakeV : public QDBusVirtualObject {
public:
    using QDBusVirtualObject::QDBusVirtualObject;
    QString introspect(const QString &) const override { return "<node/>"; }
    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override {
        printf("HANDLE path=%s iface=%s member=%s args=%lld\n", qPrintable(message.path()),
               qPrintable(message.interface()), qPrintable(message.member()),
               (long long)message.arguments().size());
        QDBusMessage reply = message.createReply();
        if (message.member() == "EnumerateDevices") {
            QList<QDBusObjectPath> paths{QDBusObjectPath("/org/example/dev0")};
            reply.setArguments({QVariant::fromValue(paths)});
        } else {
            QVariantMap props;
            props.insert("OnBattery", QVariant(true));
            reply.setArguments({QVariant(props)});
        }
        connection.send(reply);
        return true;
    }
};

class Listener : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void onProps(const QDBusMessage &message) {
        printf("RECEIVED-PROPS path=%s firstArg=%s\n", qPrintable(message.path()),
               qPrintable(message.arguments().value(0).toString()));
    }
    void onAdded(const QDBusObjectPath &path) {
        printf("RECEIVED-ADDED %s\n", qPrintable(path.path()));
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QDBusConnection daemon = QDBusConnection::sessionBus();  // the "bus" side
    // fake side: second connection
    QDBusConnection fakeConn = QDBusConnection::connectToBus(
        daemon.name() == QString() ? QString() : qgetenv("DBUS_SESSION_BUS_ADDRESS"),
        "fake-side");
    printf("fake connected=%d\n", fakeConn.isConnected());
    FakeV fake;
    fakeConn.registerService("org.example.Probe7");
    fakeConn.registerVirtualObject("/org/example", &fake, QDBusConnection::SubPath);

    // adapter side: the original connection
    Listener listener;
    bool okEmpty = daemon.connect("org.example.Probe7", QString(),
                                  "org.freedesktop.DBus.Properties",
                                  "PropertiesChanged", &listener,
                                  SLOT(onProps(const QDBusMessage&)));
    bool okAdded = daemon.connect("org.example.Probe7", "/org/example",
                                  "org.example.Iface", "DeviceAdded", &listener,
                                  SLOT(onAdded(QDBusObjectPath)));
    printf("empty connect=%d added connect=%d\n", okEmpty, okAdded);

    // adapter-style async EnumerateDevices
    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.example.Probe7", "/org/example", "org.example.Iface", "EnumerateDevices");
    auto *watcher = new QDBusPendingCallWatcher(daemon.asyncCall(call));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &app,
                     [watcher]() {
                         watcher->deleteLater();
                         const QDBusMessage reply = watcher->reply();
                         printf("async reply type=%d err=%s\n", reply.type(),
                                qPrintable(reply.errorMessage()));
                         if (reply.arguments().size() == 1) {
                             const QVariant first = reply.arguments().constFirst();
                             printf("arg=%d convert=%d\n", first.userType(),
                                    first.canConvert<QDBusArgument>());
                         }
                     });

    // GetAll style
    QDBusMessage getAll = QDBusMessage::createMethodCall(
        "org.example.Probe7", "/org/example/dev0", "org.freedesktop.DBus.Properties",
        "GetAll");
    getAll.setArguments({"org.example.Device"});
    auto *watcher2 = new QDBusPendingCallWatcher(daemon.asyncCall(getAll));
    QObject::connect(watcher2, &QDBusPendingCallWatcher::finished, &app,
                     [watcher2]() {
                         watcher2->deleteLater();
                         const QDBusPendingReply<QVariantMap> reply = *watcher2;
                         printf("getAll err=%s\n", qPrintable(reply.error().message()));
                         if (!reply.isError()) {
                             printf("map size=%lld onBattery=%d\n",
                                    (long long)reply.value().size(),
                                    reply.value().value("OnBattery").toBool());
                         }
                     });

    // signals from the fake connection
    QDBusMessage signal = QDBusMessage::createSignal(
        "/org/example/dev0", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    signal.setArguments({"org.example.Device", QVariantMap{{"OnBattery", false}},
                         QStringList{}});
    fakeConn.send(signal);
    QDBusMessage added = QDBusMessage::createSignal(
        "/org/example", "org.example.Iface", "DeviceAdded");
    added.setArguments({QVariant::fromValue(QDBusObjectPath("/org/example/dev0"))});
    fakeConn.send(added);
    QCoreApplication::processEvents();
    QTimer::singleShot(200, &app, QCoreApplication::quit);
    return app.exec();
}
#include "probe7.moc"
