// SPDX-License-Identifier: GPL-3.0-or-later

// Installed-boundary consumer probe. Builds against only the staged public
// archives and headers of the StatusNotifierAppletRuntime component — no
// build-tree applet paths — and exercises two things at that boundary:
//   1. the generation-fenced intent contract of the composed controller over
//      an injected source seam (stale dispatch refused, admitted dispatch
//      recorded exactly once), and
//   2. the staged compiled QML module: the static plugin registers
//      QindaQt.Shell.StatusNotifier, the QST-1 theme publishes through the
//      staged Tokens/Controls modules, and the packaged StatusNotifierApplet
//      surface instantiates offscreen against the real controller.
//
// Stage locations arrive as STAGE-RELATIVE compile definitions from the
// harness script; the probe resolves them against its own executable location
// (<stage>/consumer-build) at runtime, so the whole stage can be relocated
// and rerun without LD_LIBRARY_PATH. The probe itself contains no build-tree
// or source-tree path.

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtGlobal>

#include <memory>

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h>
#include <qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h>
#include <qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h>
#include <qindaqt/themes/theme_loader.h>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_StatusNotifierPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifierApplet;

namespace {

QString stagedPath(const char *relative)
{
    static const QString stageRoot = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    return stageRoot + QLatin1Char('/') + QString::fromUtf8(relative);
}

// Minimal scripted seam: a two-item ready presentation, live generations, and
// a dispatch recorder. The generation fence is the contract under test.
class TwoItemFakeSource final : public StatusNotifierSourceInterface {
public:
    QList<OwnerKey> m_dispatched;

    TwoItemFakeSource()
    {
        TrayItemPresentation first;
        first.owner = OwnerKey { QStringLiteral(":1.42"),
                                 QStringLiteral("/StatusNotifierItem"), 3 };
        first.identity = QStringLiteral("org.qindaqt.one");
        first.accessibleName = QStringLiteral("One");
        first.accessibleStatusText = QStringLiteral("active");
        first.keyboardActions = {
            KeyboardAction { RequestKind::Activate, QStringLiteral("Enter or Space") },
            KeyboardAction { RequestKind::ContextMenu,
                             QStringLiteral("Shift+F10 or Menu key") },
        };
        TrayItemPresentation second;
        second.owner = OwnerKey { QStringLiteral(":1.43"), QStringLiteral("/"), 4 };
        second.identity = QStringLiteral("org.qindaqt.two");
        second.accessibleName = QStringLiteral("Two");
        second.accessibleStatusText = QStringLiteral("passive");
        second.keyboardActions = first.keyboardActions;

        m_presentation.state = PresentationState::Ready;
        m_presentation.items = { first, second };

        ItemDescriptor firstDescriptor;
        firstDescriptor.identity = first.identity;
        firstDescriptor.title = QStringLiteral("One");
        firstDescriptor.status = ItemStatus::Active;
        ItemDescriptor secondDescriptor;
        secondDescriptor.identity = second.identity;
        secondDescriptor.title = QStringLiteral("Two");
        m_descriptors = { firstDescriptor, secondDescriptor };
    }

    [[nodiscard]] TrayPresentation presentation() const override { return m_presentation; }
    [[nodiscard]] QList<ItemDescriptor> itemDescriptors() const override
    {
        return m_descriptors;
    }
    [[nodiscard]] QImage renderIcon(const OwnerKey &, int size) const override
    {
        return StatusNotifierIconRenderer::fallbackIcon(size);
    }
    [[nodiscard]] quint64 currentGeneration(const QString &uniqueName) const override
    {
        for (const auto &item : m_presentation.items) {
            if (item.owner.uniqueName == uniqueName) {
                return item.owner.generation;
            }
        }
        return 0;
    }

    RegistryOutcome activate(const OwnerKey &target, int, int) override
    {
        m_dispatched.append(target);
        return {};
    }
    RegistryOutcome secondaryActivate(const OwnerKey &target, int, int) override
    {
        m_dispatched.append(target);
        return {};
    }
    RegistryOutcome contextMenu(const OwnerKey &target, int, int) override
    {
        m_dispatched.append(target);
        return {};
    }
    // Never degraded in this fixture; the acknowledgement contract is covered
    // by the controller and adapter rows. Records the call for completeness.
    void acknowledgeDegraded() override { ++m_acknowledgeCalls; }

private:
    TrayPresentation m_presentation;
    QList<ItemDescriptor> m_descriptors;
    int m_acknowledgeCalls = 0;
};

bool publishStagedTheme(QQmlEngine &engine)
{
    // Mirror of the Controls test-support publication path, kept local so the
    // consumer exercises only the staged public modules.
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

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    TwoItemFakeSource source;
    StatusNotifierAppletController controller(&source, true, true);

    if (controller.phaseText() != QStringLiteral("ready")
        || controller.presentedCount() != 2 || controller.itemRows().size() != 2) {
        qCritical("installed consumer: expected ready phase with two rows");
        return 2;
    }
    const QVariant firstRow = controller.itemRows().constFirst();
    if (!firstRow.value<StatusNotifierItemRow>().iconDataUrl.startsWith(
            QLatin1String("data:image/png;base64,"))
        || !firstRow.value<StatusNotifierItemRow>().iconIsPlaceholder) {
        qCritical("installed consumer: icon did not cross as a placeholder data URL");
        return 3;
    }

    // AGENT-GUARD: the generation fence is the intent contract. A stale
    // generation must be refused without any dispatch; an admitted gesture
    // must dispatch exactly once.
    const OwnerKey liveKey { QStringLiteral(":1.42"),
                             QStringLiteral("/StatusNotifierItem"), 3 };
    if (controller.activateItem(liveKey.uniqueName, liveKey.objectPath,
                                liveKey.generation + 1)) {
        qCritical("installed consumer: stale generation dispatched");
        return 4;
    }
    if (!source.m_dispatched.isEmpty()) {
        qCritical("installed consumer: refused intent reached the seam");
        return 5;
    }
    if (!controller.feedbackPresent()) {
        qCritical("installed consumer: stale refusal produced no feedback");
        return 6;
    }
    if (!controller.activateItem(liveKey.uniqueName, liveKey.objectPath,
                                 liveKey.generation)) {
        qCritical("installed consumer: admitted intent refused");
        return 7;
    }
    if (source.m_dispatched.size() != 1 || source.m_dispatched.constFirst() != liveKey) {
        qCritical("installed consumer: admitted intent did not dispatch exactly once");
        return 8;
    }

    // Staged compiled QML module proof: publish the staged theme through the
    // staged Tokens/Controls modules, then instantiate the packaged
    // StatusNotifierApplet surface from the staged QML file against the real
    // controller.
    QQmlEngine engine;
    engine.addImportPath(stagedPath(QINDAQT_STAGED_QML_RELATIVE));
    if (!publishStagedTheme(engine)) {
        return 9;
    }

    QQmlComponent surface(
        &engine,
        QUrl::fromLocalFile(stagedPath(QINDAQT_STAGED_SURFACE_RELATIVE)));
    if (surface.isError()) {
        qCritical("installed consumer: staged StatusNotifierApplet.qml did not load: %s",
                  qPrintable(surface.errorString()));
        return 10;
    }
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("access"),
                             QVariant::fromValue<QObject *>(&controller));
    initialProperties.insert(QStringLiteral("theme"), QVariant());
    std::unique_ptr<QObject> surfaceObject(surface.createWithInitialProperties(initialProperties));
    if (!surfaceObject) {
        qCritical("installed consumer: staged surface did not instantiate: %s",
                  qPrintable(surface.errorString()));
        return 11;
    }
    if (surfaceObject->property("objectName").toString()
        != QStringLiteral("statusNotifierApplet")) {
        qCritical("installed consumer: staged surface is not the status notifier applet");
        return 12;
    }
    const QVariant phase = surfaceObject->property("access")
        .value<QObject *>()->property("phaseText");
    if (phase.toString() != QStringLiteral("ready")) {
        qCritical("installed consumer: staged surface lost the controller boundary");
        return 13;
    }

    qInfo("installed consumer: StatusNotifierAppletRuntime component boundary and staged module verified");
    return 0;
}
