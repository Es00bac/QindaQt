#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingReply>
#include <QCoreApplication>
#include <cstdio>

class Device : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Device")
    Q_PROPERTY(QVariant Percentage READ percentage CONSTANT)
    Q_PROPERTY(QString Model READ model CONSTANT)
public:
    using QObject::QObject;
    QVariant percentage() const { return QVariant(55.5); }
    QString model() const { return QStringLiteral("cell"); }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService("org.example.Probe3");
    Device device;
    bool ok = bus.registerObject("/org/example/device", &device,
                       QDBusConnection::ExportScriptableProperties);
    printf("registerObject: %d\n", ok);
    QDBusMessage getAll = QDBusMessage::createMethodCall(
        "org.example.Probe3", "/org/example/device",
        "org.freedesktop.DBus.Properties", "GetAll");
    getAll.setArguments({"org.example.Device"});
    QDBusMessage reply = bus.call(getAll);
    printf("reply type: %d args=%lld\n", reply.type(), (long long)reply.arguments().size());
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        const QVariant first = reply.arguments().constFirst();
        printf("first userType=%d typeName=%s\n", first.userType(), first.typeName());
        if (first.canConvert<QDBusArgument>()) {
            const QDBusArgument arg = first.value<QDBusArgument>();
            printf("arg type=%d sig=%s\n", arg.currentType(), qPrintable(arg.currentSignature()));
            QVariantMap map = qdbus_cast<QVariantMap>(arg);
            printf("map size=%lld\n", (long long)map.size());
            for (auto it = map.begin(); it != map.end(); ++it) {
                printf("key=%s userType=%d typeName=%s\n", qPrintable(it.key()),
                       it.value().userType(), it.value().typeName());
            }
        }
    }
    return 0;
}
#include "probe3.moc"
