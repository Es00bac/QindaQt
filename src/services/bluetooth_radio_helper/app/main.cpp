// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/native_radio_authority_p.h"
#include "../src/native_radio_service_p.h"
#include <QtCore/QCoreApplication>
using namespace QindaQt::BluetoothRadio;
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-bluetooth-radio-helper"));
    const QString sessionAddress = qEnvironmentVariable("DBUS_STARTER_BUS_TYPE") == QLatin1String("session")
        && !qEnvironmentVariableIsEmpty("DBUS_STARTER_ADDRESS")
        ? qEnvironmentVariable("DBUS_STARTER_ADDRESS") : qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
    const QString systemAddress = qEnvironmentVariable("DBUS_SYSTEM_BUS_ADDRESS",
        QStringLiteral("unix:path=/run/dbus/system_bus_socket"));
    NativeRadioWire session, system;
    // This executable alone selects ambient addresses. Opening a bus is not
    // radio authority: no sysfs/rfkill observation occurs before admitted RPC.
    if (!session.open(sessionAddress) || !system.open(systemAddress)) return 1;
    NativeRadioAuthority authority(session, system);
    auto platform = makeLinuxRadioPlatform();
    RadioOperation operation(authority, *platform, boottimeMilliseconds);
    NativeRadioService service(session, operation);
    if (!session.own(QString::fromLatin1(kService))) return 1;
    const auto stopOnLoss = [&] { if (!session.connected() || !system.connected()) application.quit(); };
    session.setProgress(stopOnLoss); system.setProgress(stopOnLoss);
    return QCoreApplication::exec();
}
