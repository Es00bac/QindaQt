// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/bluetooth_protocol/bluetooth_limits.h>
#include <qindaqt/services/bluetooth_protocol/bluetooth_validation.h>

#include <QCoreApplication>
#include <QLatin1String>
#include <QString>

// Staged installed-consumer probe. It links only the staged Bluetooth1
// protocol archive and its public headers, so a broken install surface, an
// inaccurate compatibility set, or a missing static library fails here
// instead of at first real consumer integration.
int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    using namespace QindaQt::Bluetooth;

    if (kSchemaVersion != 1) {
        return 1;
    }
    if (QLatin1String(kServiceName) != QLatin1String("org.qindaqt.Bluetooth1")) {
        return 2;
    }
    if (QLatin1String(kObjectPath) != QLatin1String("/org/qindaqt/Bluetooth1")) {
        return 3;
    }
    if (QLatin1String(kInterfaceName) != QLatin1String("org.qindaqt.Bluetooth1")) {
        return 4;
    }
    if (!isCanonicalAddress(QStringLiteral("AA:BB:CC:00:11:22"))) {
        return 5;
    }
    if (isCanonicalAddress(QStringLiteral("aa:bb:cc:00:11:22"))) {
        return 6;
    }
    OperationRequest request{.kind = OperationKind::Connect,
                             .target = {.epoch = 7, .serial = 9},
                             .powered = false};
    if (!validateOperationRequest(request).accepted) {
        return 7;
    }
    return 0;
}
