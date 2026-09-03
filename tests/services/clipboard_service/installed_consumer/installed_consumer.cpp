// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/clipboard_protocol/clipboard_dbus.h>
#include <qindaqt/services/clipboard_protocol/clipboard_validation.h>
#include <qindaqt/services/clipboard_service/resident_clipboard_service.h>
#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLatin1String>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    using namespace QindaQt::Services::Clipboard;
    registerDBusTypes();
    if (kSchemaVersion != 1
        || QLatin1String(kServiceName) != QLatin1String("org.qindaqt.Clipboard1")) {
        return 1;
    }
    OperationRequest request{.kind = OperationKind::Clear,
                             .requestId = 1,
                             .expectedEpoch = 2,
                             .expectedGeneration = 1,
                             .expectedRevision = 0,
                             .entry = {},
                             .clearAll = true};
    return validateOperationRequest(request).accepted ? 0 : 2;
}
