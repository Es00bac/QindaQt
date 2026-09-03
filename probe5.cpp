#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QCoreApplication>
#include <cstdio>

class Svc : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Svc")
    Q_PROPERTY(QString Active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QList<QVariantMap> Profiles READ profiles CONSTANT)
public:
    using QObject::QObject;
    QString active() const { return m_active; }
    void setActive(const QString &v) { m_active = v; Q_EMIT activeChanged(v); }
    QList<QVariantMap> profiles() const {
        QVariantMap entry;
        entry.insert("Profile", QVariant(QStringLiteral("power-saver")));
        return {entry};
    }
    QString m_active = QStringLiteral("balanced");
Q_SIGNALS:
    void activeChanged(const QString &value);
};

class Listener : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void propsChanged(const QDBusMessage &message) {
        printf("PROPERTIES-CHANGED path=%s iface=%s\n", qPrintable(message.path()),
               qPrintable(message.arguments().value(0).toString()));
        gotIt = true;
    }
public:
    bool gotIt = false;
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService("org.example.Probe5");
    Svc svc;
    bus.registerObject("/net/example/svc", &svc,
                       QDBusConnection::ExportScriptableProperties
                           | QDBusConnection::ExportScriptableSignals);
    Listener listener;
    bus.connect("org.example.Probe5", "/net/example/svc",
                "org.freedesktop.DBus.Properties", "PropertiesChanged", &listener,
                SLOT(propsChanged(QDBusMessage)));

    QDBusMessage getAll = QDBusMessage::createMethodCall(
        "org.example.Probe5", "/net/example/svc", "org.freedesktop.DBus.Properties", "GetAll");
    getAll.setArguments({"org.example.Svc"});
    QDBusMessage reply = bus.call(getAll);
    QVariantMap map = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    printf("getAll keys=%lld\n", (long long)map.size());
    const auto it = map.constFind("Profiles");
    if (it != map.constEnd()) {
        printf("Profiles userType=%d sig=%s\n", it.value().userType(),
               it.value().canConvert<QDBusArgument>()
                   ? qPrintable(it.value().value<QDBusArgument>().currentSignature())
                   : "no-arg");
        QList<QVariantMap> entries;
        const QDBusArgument arg = it.value().value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) { QVariantMap m; arg >> m; entries.push_back(m); }
        arg.endArray();
        printf("parsed entries=%lld firstProfile=%s\n", (long long)entries.size(),
               qPrintable(entries.value(0).value("Profile").toString()));
    }
    // Trigger a property write through Set to force the notify signal.
    QDBusMessage setMsg = QDBusMessage::createMethodCall(
        "org.example.Probe5", "/net/example/svc", "org.freedesktop.DBus.Properties", "Set");
    setMsg.setArguments({"org.example.Svc", "Active",
                         QVariant::fromValue(QDBusVariant(QStringLiteral("performance")))});
    bus.call(setMsg);
    QCoreApplication::processEvents();
    printf("gotIt=%d\n", listener.gotIt);
    return 0;
}
#include "probe5.moc"
