#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QCoreApplication>
#include <cstdio>

static QDBusArgument makeProfiles() {
    QDBusArgument arg;
    arg.beginArray(QMetaType::fromType<QVariantMap>());
    QVariantMap entry;
    entry.insert("Profile", QVariant(QStringLiteral("balanced")));
    entry.insert("Driver", QVariant(QStringLiteral("platform_profile")));
    arg << entry;
    arg.endArray();
    return arg;
}

class Ppd : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Ppd")
    Q_PROPERTY(QString ActiveProfile READ activeProfile WRITE setActiveProfile NOTIFY activeChanged)
    Q_PROPERTY(QDBusArgument Profiles READ profiles CONSTANT)
public:
    using QObject::QObject;
    QString activeProfile() const { return m_active; }
    void setActiveProfile(const QString &v) { m_active = v; Q_EMIT activeChanged(v); }
    QDBusArgument profiles() const { return makeProfiles(); }
    QString m_active = QStringLiteral("balanced");
Q_SIGNALS:
    void activeChanged(const QString &value);
public Q_SLOTS:
    Q_SCRIPTABLE QList<QDBusObjectPath> Enumerate() {
        return {QDBusObjectPath("/a"), QDBusObjectPath("/b")};
    }
    Q_SCRIPTABLE QDBusArgument Inhibitors() {
        QDBusArgument arg;
        arg.beginArray(QMetaType::fromType<QStringList>());
        arg.beginStructure();
        arg << QStringLiteral("sleep") << QStringLiteral("who") << QStringLiteral("why")
            << QStringLiteral("block") << quint32(1000) << quint32(42);
        arg.endStructure();
        arg.endArray();
        return arg;
    }
    Q_SCRIPTABLE QDBusObjectPath Hold(QString, QString, QString, QString) {
        return QDBusObjectPath("/net/example/hold0");
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService("org.example.Probe4");
    Ppd ppd;
    bus.registerObject("/net/example/ppd", &ppd,
                       QDBusConnection::ExportScriptableProperties
                           | QDBusConnection::ExportScriptableSlots
                           | QDBusConnection::ExportScriptableSignals);

    // 1. GetAll with QDBusArgument-typed property
    QDBusMessage getAll = QDBusMessage::createMethodCall(
        "org.example.Probe4", "/net/example/ppd", "org.freedesktop.DBus.Properties", "GetAll");
    getAll.setArguments({"org.example.Ppd"});
    QDBusMessage reply = bus.call(getAll);
    printf("getAll: type=%d err=%s\n", reply.type(), qPrintable(reply.errorMessage()));
    QVariantMap map = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    for (auto it = map.begin(); it != map.end(); ++it)
        printf("  key=%s userType=%d sig=%s\n", qPrintable(it.key()), it.value().userType(),
               it.value().canConvert<QDBusArgument>()
                   ? qPrintable(it.value().value<QDBusArgument>().currentSignature()) : "?");

    // 2. Enumerate returning QList<QDBusObjectPath>
    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.example.Probe4", "/net/example/ppd", "org.example.Ppd", "Enumerate");
    QDBusMessage enumReply = bus.call(call);
    printf("enumerate: type=%d err=%s args=%lld\n", enumReply.type(),
           qPrintable(enumReply.errorMessage()), (long long)enumReply.arguments().size());
    if (!enumReply.arguments().isEmpty())
        printf("  sig=%s\n", qPrintable(enumReply.arguments().constFirst()
                                            .value<QDBusArgument>().currentSignature()));

    // 3. Inhibitors returning QDBusArgument
    QDBusMessage inhib = QDBusMessage::createMethodCall(
        "org.example.Probe4", "/net/example/ppd", "org.example.Ppd", "Inhibitors");
    QDBusMessage inhibReply = bus.call(inhib);
    printf("inhibitors: type=%d err=%s sig=%s\n", inhibReply.type(),
           qPrintable(inhibReply.errorMessage()),
           inhibReply.arguments().isEmpty()
               ? "?" : qPrintable(inhibReply.arguments().constFirst()
                                      .value<QDBusArgument>().currentSignature()));

    // 4. Properties.Set write path
    QDBusMessage setMsg = QDBusMessage::createMethodCall(
        "org.example.Probe4", "/net/example/ppd", "org.freedesktop.DBus.Properties", "Set");
    setMsg.setArguments({"org.example.Ppd", "ActiveProfile",
                         QVariant::fromValue(QDBusVariant(QStringLiteral("performance")))});
    QDBusMessage setReply = bus.call(setMsg);
    printf("set: type=%d err=%s newActive=%s\n", setReply.type(),
           qPrintable(setReply.errorMessage()), qPrintable(ppd.m_active));
    return 0;
}
#include "probe4.moc"
