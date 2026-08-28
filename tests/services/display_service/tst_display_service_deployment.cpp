// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_service/resident_display_service.h>

#include "support/display_service_test_support.h"

#include <QtCore/QFile>
#include <QtCore/QXmlStreamReader>
#include <QtTest/QTest>

using namespace QindaQt;
using namespace QindaQt::DisplayService;
using namespace QindaQt::DisplayService::TestSupport;

class DisplayServiceDeploymentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void descriptorsAndXmlStayAligned();
    void invalidConnectionFailsWithoutStartingPorts();
};

void DisplayServiceDeploymentTest::descriptorsAndXmlStayAligned()
{
    QFile activation(QStringLiteral(QINDAQT_DISPLAY_ACTIVATION_DESCRIPTOR));
    QVERIFY(activation.open(QIODevice::ReadOnly));
    const QByteArray activationBytes = activation.readAll();
    QVERIFY(activationBytes.contains("Name=org.qindaqt.Display1\n"));
    QVERIFY(activationBytes.contains("Exec=@KDE_INSTALL_FULL_BINDIR@/qindaqt-display-service\n"));
    QVERIFY(activationBytes.contains("SystemdService=qindaqt-display-service.service\n"));

    QFile unit(QStringLiteral(QINDAQT_DISPLAY_SYSTEMD_UNIT));
    QVERIFY(unit.open(QIODevice::ReadOnly));
    const QByteArray unitBytes = unit.readAll();
    QVERIFY(unitBytes.contains("Type=dbus\n"));
    QVERIFY(unitBytes.contains("BusName=org.qindaqt.Display1\n"));
    QVERIFY(unitBytes.contains("PrivateDevices=true\n"));
    QVERIFY(unitBytes.contains("ProtectSystem=strict\n"));

    QFile xml(QStringLiteral(QINDAQT_DISPLAY_DBUS_XML));
    QVERIFY(xml.open(QIODevice::ReadOnly));
    const QByteArray xmlBytes = xml.peek(xml.size());
    QVERIFY(xmlBytes.contains("type=\"(usta(sbbsiiduus))\" direction=\"in\""));
    QVERIFY(xmlBytes.contains("type=\"(uuusttss)\" direction=\"out\""));
    QVERIFY(xmlBytes.contains("type=\"(ustaya(ssssssiibbbbbsiiiiduusa(siiub))a(suustttu))\""));
    QXmlStreamReader reader(&xml);
    QStringList methods;
    QStringList signalNames;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        if (reader.name() == QStringLiteral("method")) {
            methods.push_back(reader.attributes().value(QStringLiteral("name")).toString());
        } else if (reader.name() == QStringLiteral("signal")) {
            signalNames.push_back(
                reader.attributes().value(QStringLiteral("name")).toString());
        }
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QCOMPARE(methods, QStringList({QStringLiteral("GetSnapshot"),
                                  QStringLiteral("Stage"), QStringLiteral("Preview"),
                                  QStringLiteral("Confirm"), QStringLiteral("Cancel")}));
    QCOMPARE(signalNames, QStringList({QStringLiteral("Changed")}));
}

void DisplayServiceDeploymentTest::invalidConnectionFailsWithoutStartingPorts()
{
    auto inventory = std::make_unique<FakeInventorySource>();
    FakeInventorySource *inventoryPointer = inventory.get();
    auto port = std::make_unique<FakeTransactionPort>();
    auto clock = std::make_unique<FakeClock>();
    ResidentDisplayService service(
        std::move(inventory), std::move(port), std::move(clock),
        [] { return QStringLiteral("epoch-a"); },
        QDBusConnection(QStringLiteral("qindaqt-display-invalid-test")));
    QCOMPARE(service.start(), ServiceStartStatus::InvalidConnection);
    QVERIFY(!service.isRunning());
    QVERIFY(!inventoryPointer->started);
}

QTEST_MAIN(DisplayServiceDeploymentTest)

#include "tst_display_service_deployment.moc"
