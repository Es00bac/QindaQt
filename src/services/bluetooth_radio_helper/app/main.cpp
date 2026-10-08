// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/qt_radio_authority_p.h"
#include "../src/radio_service_object_p.h"
#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>

using namespace QindaQt::BluetoothRadio;
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-bluetooth-radio-helper"));
    registerDBusTypes();
    // Only the executable composition root selects the ambient buses. No
    // observation or write happens until an admitted explicit power request.
    auto session = QDBusConnection::sessionBus();
    auto system = QDBusConnection::systemBus();
    if (!session.isConnected() || !system.isConnected()
        || !session.connect({}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"), QStringLiteral("Disconnected"),
            &application, SLOT(quit()))) return 1;
    QtRadioAuthority authority(session, system);
    auto platform = makeLinuxRadioPlatform();
    RadioOperation operation(authority, *platform, boottimeMilliseconds);
    RadioServiceObject object(operation);
    if (!session.registerVirtualObject(QString::fromLatin1(kPath), &object)
        || !session.registerService(QString::fromLatin1(kService))) return 1;
    const int result = QCoreApplication::exec();
    session.unregisterService(QString::fromLatin1(kService));
    session.unregisterObject(QString::fromLatin1(kPath));
    return result;
}
