// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/keyed_row_model.h"
#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

#include <qindaqt/shell/icons/desktop_entry_icon_resolver.h>
#include <qindaqt/shell/icons/icon_runtime.h>
#include <qindaqt/shell/icons/icon_availability_cache.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include <ctime>
#include <memory>

#include "task_list_applet_qml_theme_fixture.h"
#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_TaskListPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskListApplet;
using TaskListAppletTest::FakeTaskListOperationPort;
using TaskListOperationTest::FakeOperationAuthority;
namespace Icons = QindaQt::Shell::Icons;

namespace {

// AGENT-NOTE: This is the stability round's measurement harness for the task
// list (docs/wiki/shell/task-list.md, "Update cost"). It reproduces the
// shell's per-update work with the production resolver classes over the host's
// real desktop entries and icon themes: a terminal-style title spinner on one
// window of a busy strip, published through the real controller into the real
// compiled strip. It reports thread CPU time and row-model traffic; it never
// asserts a wall-clock bound, because that would be machine dependent. The
// row-model traffic assertions are deterministic.

double threadCpuMilliseconds()
{
    timespec now{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return double(now.tv_sec) * 1000.0 + double(now.tv_nsec) / 1.0e6;
}

// First `wanted` desktop-entry ids, in id order, that the resolver admits with
// an icon: the windows a real session would be showing.
QStringList iconedDesktopIds(const Icons::DesktopEntryIconResolver &resolver,
                             const QStringList &applicationRoots, int wanted)
{
    QStringList ids;
    for (const QString &root : applicationRoots) {
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files);
        while (iterator.hasNext()) {
            QString id = QFileInfo(iterator.next()).completeBaseName();
            if (!resolver.iconNameForDesktopId(id).isEmpty() && !ids.contains(id)) {
                ids.append(id);
            }
        }
    }
    ids.sort();
    return ids.mid(0, wanted);
}

} // namespace

class TaskListUpdateCostTests final : public QObject {
    Q_OBJECT

private slots:
    void titleSpinnerCostOnABusyStrip();
};

void TaskListUpdateCostTests::titleSpinnerCostOnABusyStrip()
{
    const QString dataHome =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList dataDirs =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    const QStringList iconRoots =
        Icons::IconRuntime::freedesktopIconRoots(dataHome, dataDirs);
    const QStringList themes{QStringLiteral("breeze-dark")};
    Icons::DesktopEntryIconResolver desktopEntries(
        Icons::IconRuntime::freedesktopApplicationRoots(dataHome, dataDirs));
    // The same class and call shapes as TaskListAppletComposition.
    Icons::IconAvailabilityCache availability(iconRoots, themes);

    // Twelve windows that all have a real desktop entry (the common case) plus
    // four with no entry at all, which is what a Wine program or an unusual
    // client looks like to the resolver.
    const QStringList applicationRoots =
        Icons::IconRuntime::freedesktopApplicationRoots(dataHome, dataDirs);
    QStringList applicationIds = iconedDesktopIds(desktopEntries, applicationRoots, 12);
    if (applicationIds.size() < 12) {
        QSKIP("the host publishes too few iconed desktop entries for a busy strip");
    }
    for (int index = 0; index < 4; ++index) {
        applicationIds.append(QStringLiteral("Unknown%1.exe").arg(index));
    }

    TaskListSource source;
    FakeOperationAuthority authority;
    FakeTaskListOperationPort port;
    TaskListAppletController controller(
        source, authority, port, {true, true, true},
        [&](const QString &applicationId) {
            const QString entryIcon = desktopEntries.iconNameForAppId(applicationId);
            if (!entryIcon.isEmpty()) {
                return entryIcon;
            }
            const QString cacheName =
                Icons::IconRuntime::wineCacheIconNameForAppId(applicationId);
            if (!cacheName.isEmpty() && availability.hasIcon(cacheName, 18, 1.0, false)) {
                return cacheName;
            }
            return QString{};
        },
        [&](const QString &iconName) { return availability.hasIcon(iconName, 18, 1.0, false); },
        [&](const QString &applicationId, const QString &reportedName) {
            return desktopEntries.applicationDisplayName(applicationId, reportedName);
        });

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_TASK_LIST_APPLET_QML_IMPORT_PATH));
    QVERIFY(Icons::IconRuntime::install(engine, iconRoots, themes));
    QString tokenError;
    QVERIFY2(TaskListAppletQmlTest::publishTokens(engine, &tokenError),
             qPrintable(tokenError));
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.TaskList"),
                             QStringLiteral("TaskListApplet"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> owned(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)}}));
    QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);
    QQuickWindow window;
    window.setGeometry(0, 0, 1600, 80);
    root->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY(window.isExposed());

    QVector<TaskWindowFact> facts;
    for (int index = 0; index < applicationIds.size(); ++index) {
        facts.append(TaskListTest::standalone(QStringLiteral("w%1").arg(index),
                                              applicationIds.at(index)));
    }
    int generation = 0;
    double publishMilliseconds = 0.0;
    double eventMilliseconds = 0.0;
    const auto publish = [&] {
        // The spinner window is the first one, as a terminal tab would be.
        facts[0].title = QStringLiteral("spinner %1").arg(++generation);
        const double begin = threadCpuMilliseconds();
        const auto evaluation = source.publishGeneration(facts);
        Q_ASSERT(evaluation.ok());
        authority.revision = source.revision();
        authority.sourceStatus = TaskListSourceStatus::Ready;
        // Everything up to the end of this emit is synchronous: the source,
        // the controller's projection with icon and name resolution, and every
        // QML binding that reads the controller. The event pass that follows
        // is polish, layout and the offscreen software render.
        Q_EMIT authority.stateChanged();
        const double published = threadCpuMilliseconds();
        QCoreApplication::processEvents();
        publishMilliseconds += published - begin;
        eventMilliseconds += threadCpuMilliseconds() - published;
    };

    publish();
    QCOMPARE(controller.entryCount(), 16);
    const auto models = owned->findChildren<KeyedRowModel *>();
    QVERIFY(!models.isEmpty());
    KeyedRowModel *model = models.first();
    QCOMPARE(model->rowCount(), 16);

    for (int warmup = 0; warmup < 20; ++warmup) {
        publish();
    }
    QSignalSpy dataChanged(model, &QAbstractItemModel::dataChanged);
    QSignalSpy structure(model, &QAbstractItemModel::rowsInserted);
    constexpr int iterations = 200;
    publishMilliseconds = 0.0;
    eventMilliseconds = 0.0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        publish();
    }
    const double rowsTouched = double(dataChanged.size()) / iterations;
    qInfo().noquote()
        << QStringLiteral("QINDAQT_BENCH task-list-title-spinner rows=16 "
                          "publish_cpu_ms=%1 event_cpu_ms=%2 total_cpu_ms=%3 "
                          "data_changed_per_update=%4")
               .arg(publishMilliseconds / iterations, 0, 'f', 3)
               .arg(eventMilliseconds / iterations, 0, 'f', 3)
               .arg((publishMilliseconds + eventMilliseconds) / iterations, 0, 'f', 3)
               .arg(rowsTouched, 0, 'f', 2);
    QCOMPARE(structure.size(), 0);
    // A title change is one row's data. Anything more means unchanged rows are
    // being republished (the pre-fix behavior was every row, every update).
    QCOMPARE(dataChanged.size(), iterations);
}

QTEST_MAIN(TaskListUpdateCostTests)
#include "tst_task_list_update_cost.moc"
