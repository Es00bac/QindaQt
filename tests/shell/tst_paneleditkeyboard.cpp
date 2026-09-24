// SPDX-License-Identifier: GPL-3.0-or-later
// Panel edit mode's keyboard half (ADR-0266): panel windows ask for the
// keyboard only while edit mode is on, a window shown during edit mode is
// asked again, and Escape on a panel cancels an open drag before it leaves
// edit mode. The layer-shell seam is a recorder; the controller is real.
#include "livecustomizationcontroller.h"
#include "paneleditkeyboard.h"

#include "qindaqt/applets/manifest_catalog.h"

#include <QDir>
#include <QKeyEvent>
#include <QShowEvent>
#include <QTemporaryDir>
#include <QWindow>
#include <QtTest>

using namespace QindaQt;
using QindaQt::Shell::LiveCustomizationController;
using QindaQt::Shell::PanelEditKeyboard;

namespace {

Profiles::LayoutProfile fixtureProfile()
{
    Profiles::LayoutProfile profile;
    profile.id = QStringLiteral("keyboard-fixture");
    profile.name = QStringLiteral("Keyboard fixture");
    Profiles::PanelSpec bar;
    bar.id = QStringLiteral("bar");
    bar.edge = Profiles::Edge::Top;
    bar.thickness = 30;
    bar.applets = {
        {.id = QStringLiteral("launcher-1"), .plugin = QStringLiteral("launcher"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("start")}}},
        {.id = QStringLiteral("clock-1"), .plugin = QStringLiteral("clock"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("end")}}},
    };
    profile.panels = {bar};
    return profile;
}

QVector<ShellLayout::LogicalOutput> fixtureOutputs()
{
    return {{QStringLiteral("OUT-1"), QRect(0, 0, 1920, 1080), 1.0}};
}

void pressEscape(QWindow &window)
{
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &escape);
}

} // namespace

class PanelEditKeyboardTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void keyboardFollowsEditMode();
    void escapeCancelsADragBeforeLeavingEditMode();

private:
    Applets::ManifestCatalog m_catalog;
};

void PanelEditKeyboardTest::initTestCase()
{
    QString error;
    QVERIFY2(m_catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error),
             qPrintable(error));
}

void PanelEditKeyboardTest::keyboardFollowsEditMode()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    QStringList requests;
    PanelEditKeyboard keyboard(controller, [&requests](QWindow *window, bool wantsKeyboard) {
        requests.append(window->objectName()
                        + (wantsKeyboard ? QStringLiteral(":on") : QStringLiteral(":off")));
    });
    QWindow top;
    top.setObjectName(QStringLiteral("top"));
    QWindow dock;
    dock.setObjectName(QStringLiteral("dock"));
    keyboard.attach(&top);
    keyboard.attach(&dock);
    keyboard.attach(&top);
    keyboard.attach(nullptr);

    // Outside edit mode nothing asks for the keyboard and Escape is ignored.
    QVERIFY(requests.isEmpty());
    pressEscape(top);
    QVERIFY(!controller.editMode());

    controller.enterEditMode();
    QCOMPARE(requests, (QStringList{QStringLiteral("top:on"), QStringLiteral("dock:on")}));

    // The layer-shell backend configures a newly shown surface with no
    // keyboard, so a panel shown during edit mode is asked again.
    requests.clear();
    QShowEvent show;
    QCoreApplication::sendEvent(&dock, &show);
    QCOMPARE(requests, QStringList{QStringLiteral("dock:on")});

    // Escape on any panel leaves edit mode and hands every keyboard back.
    requests.clear();
    pressEscape(dock);
    QVERIFY(!controller.editMode());
    QCOMPARE(requests, (QStringList{QStringLiteral("top:off"), QStringLiteral("dock:off")}));

    // A destroyed window is forgotten, never called.
    {
        QWindow gone;
        gone.setObjectName(QStringLiteral("gone"));
        keyboard.attach(&gone);
    }
    requests.clear();
    controller.toggleEditMode();
    QCOMPARE(requests, (QStringList{QStringLiteral("top:on"), QStringLiteral("dock:on")}));
}

void PanelEditKeyboardTest::escapeCancelsADragBeforeLeavingEditMode()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    controller.adoptProfile(fixtureProfile());
    PanelEditKeyboard keyboard(controller, {});
    QWindow panel;
    keyboard.attach(&panel);
    controller.enterEditMode();

    QVERIFY(controller.beginAppletDrag(QStringLiteral("bar"), QStringLiteral("clock-1")));
    QVERIFY(controller.hoverDropTarget(QStringLiteral("bar"), QStringLiteral("start"), QString()));
    QVERIFY(controller.dragActive());
    pressEscape(panel);
    QVERIFY(!controller.dragActive());
    QVERIFY(!controller.dropAccepted());
    QVERIFY(controller.editMode());
    pressEscape(panel);
    QVERIFY(!controller.editMode());
    // The cancelled drag wrote nothing.
    QVERIFY(QDir(directory.path()).entryList(QDir::Files).isEmpty());
}

QTEST_MAIN(PanelEditKeyboardTest)
#include "tst_paneleditkeyboard.moc"
