#include QINDAQT_STAGED_RADIO_HEADER
#include <QtCore/QCoreApplication>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QindaQt::BluetoothRadio::QtRadioPowerPort port(QDBusConnection(QStringLiteral("invalid-stage-only")));
    port.cancel(1);
    return 0;
}
