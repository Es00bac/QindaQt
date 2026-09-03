#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QCoreApplication>
#include <cstdio>

class Device : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Device")
    Q_PROPERTY(QVariant Percentage READ percentage)
    Q_PROPERTY(QVariant Missing READ missing)
    Q_PROPERTY(QVariant Texty READ texty)
public:
    using QObject::QObject;
    QVariant percentage() const { return QVariant(55.5); }
    QVariant missing() const { return QVariant(); }
    QVariant texty() const { return QVariant(QString("abc")); }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService("org.example.Probe2");
    Device device;
    bus.registerObject("/org/example/device", &device,
                       QDBusConnection::ExportScriptableProperties);
    QDBusMessage getAll = QDBusMessage::createMethodCall(
        "org.example.Probe2", "/org/example/device",
        "org.freedesktop.DBus.Properties", "GetAll");
    getAll.setArguments({"org.example.Device"});
    QDBusMessage reply = bus.call(getAll);
    printf("reply type: %d\n", reply.type());
    const QVariant first = reply.arguments().constFirst();
    QVariantMap map;
    const QDBusArgument arg = first.value<QDBusArgument>();
    arg >> map;
    for (auto it = map.begin(); it != map.end(); ++it) {
        printf("key=%s userType=%d typeName=%s\n", qPrintable(it.key()),
               it.value().userType(), it.value().typeName());
    }
    // Also try Properties.Set from the caller side for write path later.
    return 0;
}
#include "probe2.moc"
