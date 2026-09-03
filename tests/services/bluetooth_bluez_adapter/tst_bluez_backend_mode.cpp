// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/bluetooth_bluez_adapter/bluez_backend_mode.h>

#include <QStringView>
#include <QtTest>

using namespace QindaQt::Bluetooth;

// Composition-root backend selection. The packaged service defaults to the
// production BlueZ adapter; only the exact "deterministic" token selects the
// B0 empty backend, and every other spelling fails closed to production.
class BluezBackendModeTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unsetSelectsProduction();
    void exactTokensSelectEachMode();
    void invalidValuesFailClosedToProduction();
};

void BluezBackendModeTests::unsetSelectsProduction()
{
    QCOMPARE(resolveBluetoothBackendMode(QStringView{}),
             BluetoothBackendMode::Production);
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u""}),
             BluetoothBackendMode::Production);
}

void BluezBackendModeTests::exactTokensSelectEachMode()
{
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u"production"}),
             BluetoothBackendMode::Production);
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u"deterministic"}),
             BluetoothBackendMode::Deterministic);
}

void BluezBackendModeTests::invalidValuesFailClosedToProduction()
{
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u"Deterministic"}),
             BluetoothBackendMode::Production);
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u"deterministic "}),
             BluetoothBackendMode::Production);
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u"bluez"}),
             BluetoothBackendMode::Production);
    QCOMPARE(resolveBluetoothBackendMode(QStringView{u"banana"}),
             BluetoothBackendMode::Production);
}

QTEST_GUILESS_MAIN(BluezBackendModeTests)
#include "tst_bluez_backend_mode.moc"
