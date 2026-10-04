// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <unistd.h>
namespace {
constexpr auto Service = "org.qindaqt.Keyring1";
constexpr auto Path = "/org/freedesktop/secrets";
void publishEvent(const QJsonObject &value) {
    QTextStream(stdout) << QJsonDocument(value).toJson(QJsonDocument::Compact) << '\n' << Qt::flush;
}
class Endpoint final : public QDBusVirtualObject {
public:
    bool reject = false;
    int delay = 0;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &call, const QDBusConnection &bus) override {
        if (call.interface() != QLatin1String(Service)) return false;
        if ((call.member() == QLatin1String("AttachSession") && call.signature().isEmpty())
            || (call.member() == QLatin1String("AttachSessionWithDisplay") && call.signature() == QLatin1String("s"))) {
            publishEvent({{"event", "attach"}, {"caller", call.service()}, {"member", call.member()},
                {"display", call.arguments().value(0).toString()}});
            QTimer::singleShot(delay, this, [this, call, bus] {
                bus.send(call.createReply(QVariant{!reject}));
                publishEvent({{"event", "reply"}, {"accepted", !reject}});
            });
            return true;
        }
        if (call.member() == QLatin1String("Shutdown") && call.signature().isEmpty()) {
            publishEvent({{"event", "shutdown"}, {"caller", call.service()}});
            bus.send(call.createReply());
            QCoreApplication::quit();
            return true;
        }
        return false;
    }
};
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto bus = QDBusConnection::sessionBus();
    Endpoint endpoint;
    for (const auto &argument : app.arguments().mid(1)) {
        if (argument == QLatin1String("reject")) endpoint.reject = true;
        else if (argument.startsWith(QLatin1String("delay="))) endpoint.delay = argument.mid(6).toInt();
        else return 2;
    }
    if (!bus.registerVirtualObject(Path, &endpoint) || !bus.registerService(Service)) return 3;
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&] {
        char buffer[64];
        const auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count <= 0) { app.quit(); return; }
        const auto command = QByteArray(buffer, static_cast<qsizetype>(count)).trimmed();
        if (command == "release") {
            bus.unregisterService(Service);
            publishEvent({{"event", "released"}});
        } else if (command == "accept") {
            endpoint.reject = false;
            publishEvent({{"event", "accepting"}});
        } else if (command == "quit") app.quit();
        else app.exit(4);
    });
    publishEvent({{"event", "ready"}, {"owner", bus.baseService()}});
    return app.exec();
}
