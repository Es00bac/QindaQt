#include QINDAQT_STAGED_SESSION_HEADER
#include QINDAQT_STAGED_RADIO_HEADER
#include <QtCore/QCoreApplication>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QindaQt::BluetoothRadio::RadioServiceSession unavailable(QString{});
    if (unavailable.prepared() || !unavailable.legacyStartupAllowed()) return 1;
    QindaQt::BluetoothRadio::QtRadioPowerPort port(unavailable);
    port.cancel(1);
    return 0;
}
