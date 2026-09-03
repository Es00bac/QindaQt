// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_scanner.h"
#include "launch_executor.h"
#include "launcher_applet_controller.h"
#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"

#include <qindaqt/services/settings_client/settings_client.h>

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;

namespace {

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

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root->objectName() == name)
        matches.append(root);
    for (QQuickItem *child : root->childItems())
        matches.append(visualItemsNamed(child, name));
    return matches;
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
    void compiledAppletSupportsKeyboardAndAccessibility();
    void nullAccessShowsDisabledFallback();
};

void LauncherQmlTests::compiledAppletSupportsKeyboardAndAccessibility()
{
    Stack stack;

    QQmlEngine engine;
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
    QAccessibleInterface *summaryInterface =
        QAccessible::queryAccessibleInterface(summary);
    QVERIFY(summaryInterface != nullptr);
    QCOMPARE(summaryInterface->role(), QAccessible::Button);
    QVERIFY(!summaryInterface->text(QAccessible::Name).isEmpty());

    summary->forceActiveFocus();
    QVERIFY(summary->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Space);

    QObject *popup = root->findChild<QObject *>(
        QStringLiteral("launcherAppletPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());

    auto *field = root->findChild<QQuickItem *>(
        QStringLiteral("launcherSearchField"));
    QVERIFY(field != nullptr);
    QTRY_VERIFY(field->hasActiveFocus());

    // Typing filters through the controller; the surface collapses to one
    // search-results section.
    for (const QChar key : QStringLiteral("editor"))
        QTest::keyClick(&window, key.toLatin1());
    QTRY_COMPARE(stack.controller.query(), QStringLiteral("editor"));
    QTRY_COMPARE(stack.controller.sections().size(), 1);

    // Down moves into the results; Return activates through the controller
    // and the executor reaches the injected spawner.
    QTest::keyClick(&window, Qt::Key_Down);
    const auto rows = visualItemsNamed(window.contentItem(),
                                       QStringLiteral("launcherResultRow"));
    QVERIFY(!rows.isEmpty());
    QQuickItem *focusedRow = nullptr;
    for (QQuickItem *row : rows) {
        if (row->hasActiveFocus())
            focusedRow = row;
    }
    QVERIFY(focusedRow != nullptr);
    QAccessibleInterface *rowInterface =
        QAccessible::queryAccessibleInterface(focusedRow);
    QVERIFY(rowInterface != nullptr);
    QCOMPARE(rowInterface->role(), QAccessible::ListItem);
    QVERIFY(!rowInterface->text(QAccessible::Name).isEmpty());
    QVERIFY(!rowInterface->text(QAccessible::Description).isEmpty());

    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_COMPARE(stack.spawner.requests.size(), 1);
    QCOMPARE(stack.spawner.requests.constFirst().program,
             QStringLiteral("qindaqt-editor"));
}

void LauncherQmlTests::nullAccessShowsDisabledFallback()
{
    QQmlEngine engine;
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
    QVERIFY(interface->text(QAccessible::Name).contains(
        QStringLiteral("unavailable")));
}

QTEST_MAIN(LauncherQmlTests)
#include "tst_launcher_qml.moc"
