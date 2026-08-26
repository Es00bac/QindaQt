// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridwindowadmission.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class HybridWindowAdmissionTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsOnlyIndependentNormalClients();
    void rejectsNormalTypeTransientsAndDialogs();
    void rejectsNonClientAndDeadWindows();
};

void HybridWindowAdmissionTest::acceptsOnlyIndependentNormalClients()
{
    HybridWindowAdmission window;
    window.exists = true;
    window.normal = true;
    QVERIFY(admitsHybridTopologyWindow(window));
}

void HybridWindowAdmissionTest::rejectsNormalTypeTransientsAndDialogs()
{
    HybridWindowAdmission window;
    window.exists = true;
    window.normal = true;

    window.transient = true;
    QVERIFY(!admitsHybridTopologyWindow(window));
    window.transient = false;
    window.dialog = true;
    QVERIFY(!admitsHybridTopologyWindow(window));
    window.transient = true;
    QVERIFY(!admitsHybridTopologyWindow(window));
}

void HybridWindowAdmissionTest::rejectsNonClientAndDeadWindows()
{
    HybridWindowAdmission window;
    window.exists = true;
    window.normal = true;
    for (auto member : {&HybridWindowAdmission::deleted,
                        &HybridWindowAdmission::internal,
                        &HybridWindowAdmission::popup}) {
        window.*member = true;
        QVERIFY(!admitsHybridTopologyWindow(window));
        window.*member = false;
    }
    window.normal = false;
    QVERIFY(!admitsHybridTopologyWindow(window));
    window.normal = true;
    window.exists = false;
    QVERIFY(!admitsHybridTopologyWindow(window));
}

QTEST_APPLESS_MAIN(HybridWindowAdmissionTest)

#include "tst_hybridwindowadmission.moc"
