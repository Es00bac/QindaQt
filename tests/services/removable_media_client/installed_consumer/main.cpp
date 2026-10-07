#include <qindaqt/services/removable_media_client/media_client.h>
#include <QCoreApplication>
#include <QTimer>
class Launcher final : public QindaQt::RemovableMedia::MediaOwnerLauncher {
public:
    int starts = 0;
    bool startOwner() override { ++starts; return false; }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Launcher launcher;
    QindaQt::RemovableMedia::MediaClient client(QDBusConnection::sessionBus(), launcher);
    QObject::connect(&client, &QindaQt::RemovableMedia::MediaSource::snapshotChanged, &app, [&] {
        if (client.snapshot().availability == QindaQt::RemovableMedia::Availability::Unavailable)
            app.exit(launcher.starts == 0 && client.snapshot().rows.isEmpty() ? 0 : 2);
    });
    QTimer::singleShot(0, &client, &QindaQt::RemovableMedia::MediaClient::start);
    QTimer::singleShot(7000, &app, [&] { app.exit(3); });
    return app.exec();
}
