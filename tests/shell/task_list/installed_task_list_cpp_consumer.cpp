// SPDX-License-Identifier: GPL-3.0-or-later

// Installed-boundary consumer probe for the task-list applet. Builds against
// only the staged public archives and headers of the TaskListAppletRuntime
// component — no build-tree applet paths — and exercises:
//   1. the real controller over a real TaskListSource reaching the ready
//      phase with two rows (one standalone window, one two-member container),
//   2. one fenced intent dispatch path: activateTask with the exact
//      generation revision records exactly one operation-port call, and
//   3. the packaged QML surface: the staged qml/TaskListApplet.qml
//      instantiates offscreen against the real controller, with the QST-1
//      theme published through the staged Tokens/Controls modules.
//
// Stage locations arrive as STAGE-RELATIVE compile definitions from the
// harness script; the probe resolves them against its own executable location
// (<stage>/consumer-build) at runtime, so the whole stage can be relocated
// and rerun without LD_LIBRARY_PATH. The probe itself contains no build-tree
// or source-tree path. The inline fakes deliberately carry no Q_OBJECT: they
// only override public virtuals and re-emit inherited boundary signals.

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtGlobal>

#include <memory>
#include <optional>

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/shell/task_list/applet/task_list_applet_controller.h>
#include <qindaqt/shell/task_list/applet/task_list_applet_operation_port.h>
#include <qindaqt/shell/task_list/producer/task_list_operation_authority.h>
#include <qindaqt/themes/theme_loader.h>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_TaskListPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::ShellTaskList;
using namespace QindaQt::ShellTaskList::Producer;
using namespace QindaQt::ShellTaskListApplet;

namespace {

class InstalledFakeAuthority final : public TaskListOperationAuthority {
public:
  using TaskListOperationAuthority::TaskListOperationAuthority;

  [[nodiscard]] QString uniqueOwner() const override { return owner; }
  [[nodiscard]] quint64 publishedRevision() const override { return revision; }
  [[nodiscard]] TaskListSourceStatus status() const override {
    return sourceStatus;
  }
  [[nodiscard]] std::optional<TaskListActionGeneration>
  actionGeneration() const override { return generation; }
  [[nodiscard]] std::optional<TaskListContainerLineage>
  containerLineage(const QString &containerId) const override {
    for (const TaskListContainerLineage &lineage : containers) {
      if (lineage.containerId == containerId) {
        return lineage;
      }
    }
    return std::nullopt;
  }

  void notifyStateChanged() { Q_EMIT stateChanged(); }

  QString owner = QStringLiteral(":1.1");
  quint64 revision = 0;
  std::optional<TaskListActionGeneration> generation{
      TaskListActionGeneration{QStringLiteral("test-epoch"), 1}};
  TaskListSourceStatus sourceStatus = TaskListSourceStatus::Ready;
  QVector<TaskListContainerLineage> containers{
      {QStringLiteral("c1"), 7, TaskListContainerAuthority::ControlBridge}};
};

struct RecordedCall {
  QString method;
  quint64 token = 0;
  TaskIntentRequest request;
  quint64 revision = 0;
};

class InstalledFakePort final : public TaskListAppletOperationPort {
public:
  using TaskListAppletOperationPort::TaskListAppletOperationPort;

  quint64 executeTaskIntent(const TaskIntentRequest &request,
                            const TaskIntentOutcome &) override {
    const quint64 token = nextToken++;
    calls.append({QStringLiteral("executeTaskIntent"), token, request,
                  request.expectedRevision});
    return token;
  }
  quint64 activateContainerPage(const QString &containerId, const QString &,
                                quint64 expectedRevision) override {
    return record(QStringLiteral("activateContainerPage"), containerId,
                  expectedRevision);
  }
  quint64 detachWindow(const QString &containerId, const QString &,
                       quint64 expectedRevision) override {
    return record(QStringLiteral("detachWindow"), containerId,
                  expectedRevision);
  }
  quint64 releaseContainer(const QString &containerId,
                           quint64 expectedRevision) override {
    return record(QStringLiteral("releaseContainer"), containerId,
                  expectedRevision);
  }
  quint64 dockWindows(const QString &targetWindowId, const QString &,
                      const QString &, const QString &, double,
                      quint64 expectedRevision) override {
    return record(QStringLiteral("dockWindows"), targetWindowId,
                  expectedRevision);
  }

  QVector<RecordedCall> calls;
  quint64 nextToken = 1;

private:
  quint64 record(const QString &method, const QString &, quint64 revision) {
    const quint64 token = nextToken++;
    calls.append({method, token, {}, revision});
    return token;
  }
};

QString stagedPath(const char *relative)
{
    static const QString stageRoot = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    return stageRoot + QLatin1Char('/') + QString::fromUtf8(relative);
}

TaskWindowFact makeFact(const QString &windowId, const QString &applicationId)
{
    TaskWindowFact fact;
    fact.windowId = windowId;
    fact.applicationId = applicationId;
    fact.applicationName = QStringLiteral("App %1").arg(applicationId);
    fact.title = QStringLiteral("Title %1").arg(windowId);
    fact.outputId = QStringLiteral("output-1");
    fact.workspaceIds = {QStringLiteral("ws-1")};
    return fact;
}

bool publishStagedTheme(QQmlEngine &engine)
{
    // Mirror of the Controls test-support publication path, kept local so the
    // consumer exercises only the staged public modules (clipboard precedent).
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    if (registration.isError()) {
        qCritical("installed consumer: QindaQt.Tokens did not resolve: %s",
                  qPrintable(registration.errorString()));
        return false;
    }

    auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (!facade) {
        qCritical("installed consumer: Tokens singleton not registered");
        return false;
    }

    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        stagedPath(QINDAQT_STAGED_THEME_RELATIVE));
    if (!loaded.ok) {
        qCritical("installed consumer: staged theme refused: %s", qPrintable(loaded.error));
        return false;
    }
    QString error;
    if (!facade->publish(loaded.theme, {}, &error)) {
        qCritical("installed consumer: theme publish failed: %s", qPrintable(error));
        return false;
    }
    return true;
}

int visualItemsNamed(QQuickItem *root, const QString &name)
{
    int matches = root->objectName() == name ? 1 : 0;
    for (QQuickItem *child : root->childItems()) {
        matches += visualItemsNamed(child, name);
    }
    return matches;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    TaskListSource source;
    TaskWindowFact standaloneWindow =
        makeFact(QStringLiteral("w1"), QStringLiteral("app.one"));
    TaskWindowFact primaryWindow =
        makeFact(QStringLiteral("w2"), QStringLiteral("app.two"));
    primaryWindow.role = TaskWindowRole::ContainerPrimary;
    primaryWindow.containerId = QStringLiteral("c1");
    TaskWindowFact memberWindow =
        makeFact(QStringLiteral("w3"), QStringLiteral("app.member"));
    memberWindow.role = TaskWindowRole::ContainerMember;
    memberWindow.containerId = QStringLiteral("c1");
    const auto evaluation = source.publishGeneration(
        {standaloneWindow, primaryWindow, memberWindow});
    if (!evaluation.ok()) {
        qCritical("installed consumer: staged source refused the facts");
        return 1;
    }

    InstalledFakeAuthority authority;
    authority.revision = source.revision();
    InstalledFakePort port;
    TaskListAppletController controller(source, authority, port,
                                        {true, true, true});
    if (controller.phaseText() != QStringLiteral("ready")
        || controller.entryCount() != 2) {
        qCritical("installed consumer: expected ready phase with two rows, got %s/%d",
                  qPrintable(controller.phaseText()), controller.entryCount());
        return 2;
    }

    // Fenced intent dispatch: the exact displayed revision must dispatch once.
    const quint64 revision = source.revision();
    if (!controller.activateTask(QStringLiteral("w1"), revision)) {
        qCritical("installed consumer: activateTask was refused");
        return 3;
    }
    if (port.calls.size() != 1
        || port.calls.constFirst().method != QStringLiteral("executeTaskIntent")
        || port.calls.constFirst().request.taskId != QStringLiteral("w1")
        || port.calls.constFirst().request.expectedRevision != revision) {
        qCritical("installed consumer: fenced dispatch did not reach the port exactly once");
        return 4;
    }

    // Staged QML module proof: instantiate the packaged TaskListApplet
    // surface offscreen against the real controller. The surface consumes
    // QST-1 roles, so the staged theme publishes first.
    QQmlEngine engine;
    engine.addImportPath(stagedPath(QINDAQT_STAGED_QML_RELATIVE));
    if (!publishStagedTheme(engine)) {
        return 5;
    }
    QQmlComponent surface(
        &engine,
        QUrl::fromLocalFile(stagedPath(QINDAQT_STAGED_SURFACE_RELATIVE)));
    if (surface.isError()) {
        qCritical("installed consumer: staged TaskListApplet.qml did not load: %s",
                  qPrintable(surface.errorString()));
        return 5;
    }
    std::unique_ptr<QObject> surfaceObject(surface.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)},
         {QStringLiteral("vertical"), false}}));
    if (!surfaceObject) {
        qCritical("installed consumer: staged surface did not instantiate: %s",
                  qPrintable(surface.errorString()));
        return 6;
    }
    if (surfaceObject->objectName() != QStringLiteral("taskListApplet")) {
        qCritical("installed consumer: staged surface is not the task-list applet");
        return 7;
    }
    if (surfaceObject->property("phase").toString() != QStringLiteral("ready")) {
        qCritical("installed consumer: staged surface lost the controller boundary");
        return 8;
    }
    const auto *rootItem = qobject_cast<QQuickItem *>(surfaceObject.get());
    if (rootItem == nullptr) {
        qCritical("installed consumer: staged surface is not a visual item");
        return 9;
    }
    // Row delegates materialize once the surface is window-attached and the
    // scene graph polishes the layout.
    QQuickWindow window;
    window.setGeometry(0, 0, 640, 120);
    const_cast<QQuickItem *>(rootItem)->setParentItem(window.contentItem());
    window.show();
    for (int spin = 0; spin < 100
         && visualItemsNamed(const_cast<QQuickItem *>(rootItem),
                             QStringLiteral("taskListEntryButton"))
                != 2;
         ++spin) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    if (visualItemsNamed(const_cast<QQuickItem *>(rootItem),
                         QStringLiteral("taskListEntryButton"))
        != 2) {
        qCritical("installed consumer: staged surface did not render the two rows");
        return 10;
    }

    qInfo("installed consumer: TaskListAppletRuntime component boundary and staged module verified");
    return 0;
}
