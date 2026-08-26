// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellvisibilitywindowadmission.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class ShellVisibilityWindowAdmissionTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void includesOrdinaryDialogsAndTransients();
    void excludesShellAndSpecialSurfaces();
    void excludesDeadInternalAndUnmanagedWindows();
};

void ShellVisibilityWindowAdmissionTest::includesOrdinaryDialogsAndTransients()
{
    ShellVisibilityWindowAdmission window{.exists = true,
                                           .managed = true,
                                           .normal = true};
    QVERIFY(admitsShellVisibilityWindow(window));

    window.normal = false;
    window.dialog = true;
    window.transient = true;
    QVERIFY(admitsShellVisibilityWindow(window));

    window.dialog = false;
    window.utility = true;
    QVERIFY(admitsShellVisibilityWindow(window));
}

void ShellVisibilityWindowAdmissionTest::excludesShellAndSpecialSurfaces()
{
    ShellVisibilityWindowAdmission window{.exists = true,
                                           .managed = true,
                                           .normal = true};
    for (auto excluded : {&ShellVisibilityWindowAdmission::desktop,
                          &ShellVisibilityWindowAdmission::dock,
                          &ShellVisibilityWindowAdmission::splash,
                          &ShellVisibilityWindowAdmission::tooltip,
                          &ShellVisibilityWindowAdmission::menu,
                          &ShellVisibilityWindowAdmission::popup}) {
        window.*excluded = true;
        QVERIFY(!admitsShellVisibilityWindow(window));
        window.*excluded = false;
    }
}

void ShellVisibilityWindowAdmissionTest::excludesDeadInternalAndUnmanagedWindows()
{
    ShellVisibilityWindowAdmission window{.exists = true,
                                           .managed = true,
                                           .normal = true};
    window.deleted = true;
    QVERIFY(!admitsShellVisibilityWindow(window));
    window.deleted = false;
    window.internal = true;
    QVERIFY(!admitsShellVisibilityWindow(window));
    window.internal = false;
    window.managed = false;
    QVERIFY(!admitsShellVisibilityWindow(window));
    window.managed = true;
    window.exists = false;
    QVERIFY(!admitsShellVisibilityWindow(window));
}

QTEST_APPLESS_MAIN(ShellVisibilityWindowAdmissionTest)

#include "tst_shellvisibilitywindowadmission.moc"
