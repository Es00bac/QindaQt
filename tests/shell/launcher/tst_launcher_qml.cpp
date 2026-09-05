// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_scanner.h"
#include "launch_executor.h"
#include "launcher_applet_controller.h"
#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"
#include "../icon_resolution_test_fixture.h"

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/design_tokens/token_deriver.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/themes/theme_loader.h>

#include <QAccessible>
#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;

namespace {

QQuickItem *popupContent(QQuickItem *root)
{
    auto *popup = root->findChild<QObject *>(QStringLiteral("launcherAppletPopup"));
    return popup ? popup->property("contentItem").value<QQuickItem *>() : nullptr;
}

// The installed-package gate overrides the import path to prove the staged
// module loads without the build tree.
QString importPath()
{
    const QByteArray overridePath =
        qgetenv("QINDAQT_LAUNCHER_QML_IMPORT_PATH_OVERRIDE");
    if (!overridePath.isEmpty())
        return QString::fromUtf8(overridePath);
    return QStringLiteral(QINDAQT_LAUNCHER_QML_IMPORT_PATH);
}

QQuickItem *activeResultRow(QQuickItem *root)
{
    if (root->objectName().startsWith(QStringLiteral("launcherResultRow-"))
        && root->hasActiveFocus())
        return root;
    for (QQuickItem *child : root->childItems())
        if (auto *match = activeResultRow(child))
            return match;
    return nullptr;
}

QQuickItem *visualItemNamed(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems())
        if (auto *match = visualItemNamed(child, name))
            return match;
    return nullptr;
}

QQuickItem *resultRowForEntry(QQuickItem *root, const QString &entryId)
{
    if (root->objectName().startsWith(
            QStringLiteral("launcherResultRow-") + entryId + QLatin1Char('-')))
        return root;
    for (QQuickItem *child : root->childItems())
        if (auto *match = resultRowForEntry(child, entryId))
            return match;
    return nullptr;
}

bool publishTokens(QQmlEngine &engine)
{
    engine.addImportPath(importPath());
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml", QUrl(QStringLiteral("inline:launcher-token-registration.qml")));
    if (registration.status() == QQmlComponent::Loading) {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&registration, &QQmlComponent::statusChanged, &loop,
                         [&loop](QQmlComponent::Status status) {
                             if (status != QQmlComponent::Loading)
                                 loop.quit();
                         });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(5000);
        loop.exec();
    }
    if (!registration.isReady()) {
        qInfo() << "token registration failed" << registration.errorString();
        return false;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    if (!registrationObject) {
        qInfo() << "token registration object failed" << registration.errorString();
        return false;
    }
    auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (facade == nullptr) {
        qInfo() << "token singleton lookup failed";
        return false;
    }
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dusk.json"));
    if (!loaded.ok) {
        qInfo() << "theme load failed" << loaded.error;
        return false;
    }
    QString error;
    const bool published = facade->publish(
        loaded.theme, QindaQt::DesignTokens::AccessibilityInputs {}, &error);
    if (!published)
        qInfo() << "token publication failed" << error;
    return published;
}

struct Stack {
    QTemporaryDir root;
    ApplicationScanner scanner;
    FakeSettingsTransport transport;
    SettingsClient client;
    LauncherPersistenceController persistence;
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor;
    LauncherAppletController controller;

    Stack()
        : scanner({ root.path() })
        , client(transport,
                 { LauncherPersistenceController::pinnedKey(),
                   LauncherPersistenceController::recentKey() })
        , persistence(client)
        , executor(scanner, spawner, activator)
        , controller(&scanner, &persistence, &executor, true)
    {
        writeDesktopFile(root.path(), QStringLiteral("editor.desktop"),
                         minimalEntry(QStringLiteral("Fixture Editor"),
                                      QStringLiteral("qindaqt-editor"),
                                      QStringLiteral("Categories=Development\n")));
        writeDesktopFile(root.path(), QStringLiteral("files.desktop"),
                         minimalEntry(QStringLiteral("Fixture Files"),
                                      QStringLiteral("qindaqt-files"),
                                      QStringLiteral("Categories=Utility\n")));
        if (!scanner.start())
            qFatal("fixture scanner failed to start");
    }
};

std::unique_ptr<QObject> createApplet(QQmlEngine &engine, QObject *access)
{
    engine.addImportPath(importPath());
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.Launcher"),
                             QStringLiteral("LauncherApplet"));
    if (!component.isReady()) {
        qWarning() << component.errorString();
        return nullptr;
    }
    return std::unique_ptr<QObject>(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(access)}}));
}

} // namespace

class LauncherQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rendersSectionsPersistenceAndAccessibleStates();
    void directPinButtonMutatesWithoutLaunching();
    void deniedRowsExposeAccessibleDisabledState();
    void supportsCompleteKeyboardTraversalAndActivation();
    void nullAccessShowsDisabledFallback();
};

void LauncherQmlTests::rendersSectionsPersistenceAndAccessibleStates()
{
    Stack stack;
    QVERIFY(stack.client.start());
    stack.transport.announceOwner();
    QTRY_VERIFY(!stack.transport.snapshots.isEmpty());
    stack.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("qml-epoch"), 0,
        {{ LauncherPersistenceController::pinnedKey(),
           QVariantList { QStringLiteral("editor") } },
         { LauncherPersistenceController::recentKey(),
           QVariantList { QStringLiteral("files") } }}));
    QTRY_VERIFY(stack.persistence.persistenceReady());

    QQmlEngine engine;
    QString iconError;
    QVERIFY2(QindaQt::Tests::installResolvedIconFixture(
                 engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
                 {QStringLiteral("start-here-kde")}, &iconError),
             qPrintable(iconError));
    QVERIFY(publishTokens(engine));
    auto owned = createApplet(engine, &stack.controller);
    QVERIFY(owned != nullptr);
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 30);
    root->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY(window.isExposed());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("launcherAppletSummary"));
    QVERIFY(summary != nullptr);
    QAccessibleInterface *summaryInterface =
        QAccessible::queryAccessibleInterface(summary);
    QVERIFY(summaryInterface != nullptr);
    QCOMPARE(summaryInterface->role(), QAccessible::Button);
    QVERIFY(!summaryInterface->text(QAccessible::Name).isEmpty());
    QCOMPARE(summary->property("text").toString(), QString());
    QVERIFY(summary->width() <= root->height() + 4.0);
    auto *summaryIcon = summary->findChild<QQuickItem *>(
        QStringLiteral("launcherAppletIcon"));
    QVERIFY(summaryIcon != nullptr);
    QVERIFY(QindaQt::Tests::hasResolvedProviderSource(
        summaryIcon, QStringLiteral("start-here-kde")));

    summary->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    auto *popup = root->findChild<QObject *>(QStringLiteral("launcherAppletPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(popupContent(root) != nullptr);
    QVERIFY(popupContent(root)->window() != &window);
    QVERIFY(popup->property("height").toReal() > window.height());

    for (const QString &identity : { QStringLiteral("pinned"),
                                     QStringLiteral("recent"),
                                     QStringLiteral("development"),
                                     QStringLiteral("utilities") }) {
        auto *header = visualItemNamed(popupContent(root),
            QStringLiteral("launcherSectionHeader-") + identity);
        QVERIFY2(header != nullptr, qPrintable(identity));
        QAccessibleInterface *headerInterface =
            QAccessible::queryAccessibleInterface(header);
        QVERIFY(headerInterface != nullptr);
        QVERIFY(!headerInterface->text(QAccessible::Name).isEmpty());
    }

    auto *enabledRow = visualItemNamed(popupContent(root),
        QStringLiteral("launcherResultRow-editor-0"));
    QVERIFY(enabledRow != nullptr);
    QAccessibleInterface *enabledInterface =
        QAccessible::queryAccessibleInterface(enabledRow);
    QVERIFY(enabledInterface != nullptr);
    QCOMPARE(enabledInterface->role(), QAccessible::ListItem);
    QVERIFY(!enabledInterface->text(QAccessible::Name).isEmpty());
    QVERIFY(!enabledInterface->text(QAccessible::Description).isEmpty());
    QVERIFY(!enabledInterface->state().disabled);

    // Pinning remains reachable through an ordinary primary click even when
    // a compositor or input backend cannot deliver a secondary gesture.
    auto *visiblePin = enabledRow->findChild<QQuickItem *>(
        QStringLiteral("launcherTogglePinButton"));
    QVERIFY(visiblePin != nullptr);
    QVERIFY(visiblePin->property("visible").toBool());
    QCOMPARE(visiblePin->property("text").toString(), QStringLiteral("Unpin"));
    QCOMPARE(visiblePin->property("emphasized").toBool(), false);

    // Restore the theme-pixel fixture to its neutral state before checking
    // the token-owned background below.
    auto *field = root->findChild<QQuickItem *>(QStringLiteral("launcherSearchField"));
    QVERIFY(field != nullptr);
    field->forceActiveFocus();
    QTRY_VERIFY(field->hasActiveFocus());
    QTest::mouseMove(popupContent(root)->window(), QPoint(1, 1));

    // Native palettes must not override either side of the launch-row contrast
    // pair. Republish themes without recreating delegates to catch stale colors.
    auto *background = enabledRow->property("background").value<QQuickItem *>();
    auto *label = enabledRow->findChild<QQuickItem *>(QStringLiteral("launcherResultText"));
    QVERIFY(background != nullptr);
    QVERIFY(label != nullptr);
    auto *tokens = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    for (const QString &themeId : {QStringLiteral("qinda-dark"),
                                   QStringLiteral("qinda-light"),
                                   QStringLiteral("qinda-high-contrast")}) {
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + themeId + ".json");
        QVERIFY(loaded.ok);
        QVERIFY(tokens->publish(loaded.theme, {}));
        const QColor surface = tokens->bg().value("raised").value<QColor>();
        QTRY_COMPARE(background->property("color").value<QColor>(), surface);
        const QColor foreground = label->property("color").value<QColor>();
        QVERIFY(QindaQt::DesignTokens::DesignTokenDeriver::contrastRatio(
                    foreground, surface) >= 4.5);
        QQuickWindow *surfaceWindow = popupContent(root)->window();
        const QPointF sample = enabledRow->mapToScene(
            QPointF(enabledRow->width() / 2, enabledRow->height() - 2));
        const QImage frame = surfaceWindow->grabWindow();
        QVERIFY(!frame.isNull());
        QCOMPARE(frame.pixelColor((sample * frame.devicePixelRatio()).toPoint()), surface);
    }


    // A malformed authoritative value produces bounded, alert-role persistence
    // truth in the applet instead of disappearing inside the controller.
    const qsizetype snapshotsBefore = stack.transport.snapshots.size();
    Q_EMIT stack.transport.settingsChanged(
        QStringLiteral(":1.99"), QStringLiteral("qml-epoch"), 1,
        { LauncherPersistenceController::pinnedKey() });
    QTRY_COMPARE(stack.transport.snapshots.size(), snapshotsBefore + 1);
    stack.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("qml-epoch"), 1,
        {{ LauncherPersistenceController::pinnedKey(), QVariantList { 42 } }}));
    QTRY_VERIFY(!stack.controller.persistenceStatus().isEmpty());
    auto *status = visualItemNamed(popupContent(root),
        QStringLiteral("launcherAppletPersistenceStatus"));
    QVERIFY(status != nullptr);
    QTRY_VERIFY(status->isVisible());
    QCOMPARE(status->property("maximumLineCount").toInt(), 3);
    QAccessibleInterface *statusInterface =
        QAccessible::queryAccessibleInterface(status);
    QVERIFY(statusInterface != nullptr);
    QCOMPARE(statusInterface->role(), QAccessible::AlertMessage);
    QVERIFY(!statusInterface->text(QAccessible::Name).isEmpty());

}

void LauncherQmlTests::directPinButtonMutatesWithoutLaunching()
{
    Stack stack;
    QVERIFY(stack.client.start());
    stack.transport.announceOwner();
    QTRY_VERIFY(!stack.transport.snapshots.isEmpty());
    stack.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("pin-epoch"), 0,
        {{ LauncherPersistenceController::pinnedKey(),
           QVariantList { QStringLiteral("editor") } },
         { LauncherPersistenceController::recentKey(),
           QVariantList { QStringLiteral("files") } }}));
    QTRY_VERIFY(stack.persistence.persistenceReady());

    QQmlEngine engine;
    QVERIFY(publishTokens(engine));
    auto owned = createApplet(engine, &stack.controller);
    QVERIFY(owned != nullptr);
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);
    QQuickWindow window;
    window.setGeometry(0, 0, 420, 520);
    root->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY(window.isExposed());
    QVERIFY(QMetaObject::invokeMethod(root, "openBrowser"));
    QTRY_VERIFY(popupContent(root) != nullptr);

    auto clickPinButton = [root](const QString &expectedText) {
        auto *row = resultRowForEntry(popupContent(root), QStringLiteral("editor"));
        QVERIFY(row != nullptr);
        auto *button = row->findChild<QQuickItem *>(
            QStringLiteral("launcherTogglePinButton"));
        QVERIFY(button != nullptr);
        QTRY_COMPARE(button->property("text").toString(), expectedText);
        const QPoint point = button->mapToScene(
            QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(popupContent(root)->window(), Qt::LeftButton, {}, point);
    };

    clickPinButton(QStringLiteral("Unpin"));
    QTRY_COMPARE(stack.transport.commits.size(), 1);
    stack.transport.replyLastCommit(FakeSettingsTransport::commitWire(
        SettingsWireStatus::Applied, QStringLiteral("pin-epoch"), 0, 1,
        {{ LauncherPersistenceController::pinnedKey(), QVariantList {} },
         { LauncherPersistenceController::recentKey(),
           QVariantList { QStringLiteral("files") } }}));
    QTRY_VERIFY(stack.persistence.persistenceReady());

    clickPinButton(QStringLiteral("Pin"));
    QTRY_COMPARE(stack.transport.commits.size(), 2);
    stack.transport.replyLastCommit(FakeSettingsTransport::commitWire(
        SettingsWireStatus::Applied, QStringLiteral("pin-epoch"), 1, 2,
        {{ LauncherPersistenceController::pinnedKey(),
           QVariantList { QStringLiteral("editor") } },
         { LauncherPersistenceController::recentKey(),
           QVariantList { QStringLiteral("files") } }}));
    QTRY_VERIFY(stack.persistence.persistenceReady());
    QVERIFY(stack.spawner.requests.isEmpty());
    QVERIFY(stack.activator.activations.isEmpty());
}

void LauncherQmlTests::deniedRowsExposeAccessibleDisabledState()
{
    Stack stack;
    QVERIFY(stack.client.start());
    stack.transport.announceOwner();
    QTRY_VERIFY(!stack.transport.snapshots.isEmpty());
    stack.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("denied-epoch"), 0,
        {{ LauncherPersistenceController::pinnedKey(),
           QVariantList { QStringLiteral("editor") } }}));
    QTRY_VERIFY(stack.persistence.persistenceReady());

    LauncherAppletController denied(&stack.scanner, &stack.persistence,
                                    &stack.executor, false);
    QQmlEngine engine;
    QVERIFY(publishTokens(engine));
    auto owned = createApplet(engine, &denied);
    QVERIFY(owned != nullptr);
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 520);
    root->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY(window.isExposed());
    QVERIFY(QMetaObject::invokeMethod(root, "openBrowser"));
    QTRY_VERIFY(visualItemNamed(popupContent(root),
        QStringLiteral("launcherResultRow-editor-0")) != nullptr);
    auto *deniedRow = visualItemNamed(popupContent(root),
        QStringLiteral("launcherResultRow-editor-0"));
    QVERIFY(!deniedRow->isEnabled());
    QAccessibleInterface *deniedInterface =
        QAccessible::queryAccessibleInterface(deniedRow);
    QVERIFY(deniedInterface != nullptr);
    QVERIFY(deniedInterface->state().disabled);
}

void LauncherQmlTests::supportsCompleteKeyboardTraversalAndActivation()
{
    Stack stack;
    QVERIFY(stack.client.start());
    stack.transport.announceOwner();
    QTRY_VERIFY(!stack.transport.snapshots.isEmpty());
    stack.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("keyboard-epoch"), 0,
        {{ LauncherPersistenceController::pinnedKey(),
           QVariantList { QStringLiteral("editor") } },
         { LauncherPersistenceController::recentKey(),
           QVariantList { QStringLiteral("files") } }}));
    QTRY_VERIFY(stack.persistence.persistenceReady());

    QQmlEngine engine;
    QVERIFY(publishTokens(engine));
    auto owned = createApplet(engine, &stack.controller);
    QVERIFY(owned != nullptr);
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 520);
    root->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY(window.isExposed());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("launcherAppletSummary"));
    QVERIFY(summary != nullptr);

    summary->forceActiveFocus();
    QVERIFY(summary->hasActiveFocus());
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);

    QObject *popup = root->findChild<QObject *>(QStringLiteral("launcherAppletPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(popupContent(root) != nullptr);
    QVERIFY(popupContent(root)->window() != &window);

    auto *field = root->findChild<QQuickItem *>(
        QStringLiteral("launcherSearchField"));
    QVERIFY(field != nullptr);
    QTRY_VERIFY(field->hasActiveFocus());

    // Tab reaches the first row. Up/Down then traverse the flat model across
    // the Pinned and Recent section boundary and back to the search field.
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Tab);
    QTRY_VERIFY(activeResultRow(popupContent(root)) != nullptr);
    QCOMPARE(QAccessible::queryAccessibleInterface(
                 activeResultRow(popupContent(root)))->text(QAccessible::Name),
             QStringLiteral("Fixture Editor"));
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Down);
    QTRY_VERIFY(activeResultRow(popupContent(root)) != nullptr);
    QCOMPARE(QAccessible::queryAccessibleInterface(
                 activeResultRow(popupContent(root)))->text(QAccessible::Name),
             QStringLiteral("Fixture Files"));
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Up);
    QCOMPARE(QAccessible::queryAccessibleInterface(
                 activeResultRow(popupContent(root)))->text(QAccessible::Name),
             QStringLiteral("Fixture Editor"));
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Up);
    QTRY_VERIFY(field->hasActiveFocus());

    // Space and Return are pointer-equivalent activation paths.
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Down);
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QTRY_COMPARE(stack.spawner.requests.size(), 1);
    QCOMPARE(stack.spawner.requests.constFirst().program,
             QStringLiteral("qindaqt-editor"));
    field->forceActiveFocus();
    QTRY_VERIFY(field->hasActiveFocus());
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Down);
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Down);
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Return);
    QTRY_COMPARE(stack.spawner.requests.size(), 2);
    QCOMPARE(stack.spawner.requests.constLast().program,
             QStringLiteral("qindaqt-editor"));

    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());

    // The offscreen platform does not reactivate the transient parent.
    window.requestActivate();
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    // Reopen and type to prove the search section replaces browse sections.
    summary->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Return);
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(popupContent(root) != nullptr);
    QVERIFY(popupContent(root)->window() != &window);
    QTRY_VERIFY(field->hasActiveFocus());
    for (const QChar key : QStringLiteral("editor"))
        QTest::keyClick(QGuiApplication::focusWindow(), key.toLatin1());
    QTRY_COMPARE(stack.controller.query(), QStringLiteral("editor"));
    QTRY_COMPARE(stack.controller.sections().size(), 1);
    QVERIFY(visualItemNamed(popupContent(root),
                            QStringLiteral("launcherSectionHeader-searchResults"))
            != nullptr);
}

void LauncherQmlTests::nullAccessShowsDisabledFallback()
{
    QQmlEngine engine;
    QVERIFY(publishTokens(engine));
    auto owned = createApplet(engine, nullptr);
    QVERIFY(owned != nullptr);
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("launcherAppletSummary"));
    QVERIFY(summary != nullptr);
    QVERIFY(!summary->isEnabled());
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(summary);
    QVERIFY(interface != nullptr);
    QVERIFY(interface->state().disabled);
    QVERIFY(interface->text(QAccessible::Name).contains(
        QStringLiteral("unavailable")));
}

QTEST_MAIN(LauncherQmlTests)
#include "tst_launcher_qml.moc"
