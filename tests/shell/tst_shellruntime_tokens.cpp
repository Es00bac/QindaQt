// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelltokenpublisher.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QColor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_TaskListPlugin)

namespace {

constexpr auto HostedAppletSource = R"qml(
    import QtQuick
    import QindaQt.Shell.TaskList 1.0
    import QindaQt.Tokens 1.0

    Item {
        width: 480
        height: 64
        readonly property bool tokenReady: Tokens.ready
        readonly property color backgroundBase: Tokens.bg.base

        QtObject {
            id: taskAccess
            property string phaseText: "loading"
            property int entryCount: 0
            property int totalEntryCount: 0
            property var entryRows: []
            property int overflowCount: 0
            property string phaseReasonText: ""
            property bool feedbackPresent: false
            property string feedback: ""
        }

        TaskListApplet {
            id: hostedApplet
            anchors.fill: parent
            access: taskAccess
        }
    }
)qml";

QQuickItem *itemNamed(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = itemNamed(child, name)) {
            return match;
        }
    }
    return nullptr;
}

} // namespace

class ShellRuntimeTokenTests final : public QObject {
    Q_OBJECT

private slots:
    void productionPublisherPrecedesHostedApplet();
};

void ShellRuntimeTokenTests::productionPublisherPrecedesHostedApplet()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    QVERIFY2(themes.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"), &error),
             qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-dark")));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_BUILD_DIR));
    QindaQt::Shell::ShellTokenPublisher publisher(engine, themes);
    QVERIFY2(publisher.start(&error), qPrintable(error));
    QVERIFY(publisher.facade() != nullptr);
    QVERIFY(publisher.facade()->ready());
    QCOMPARE(publisher.facade()->sourceThemeId(), QStringLiteral("qinda-dark"));
    QCOMPARE(publisher.facade()->bg().value(QStringLiteral("base")).value<QColor>(),
             QColor(QStringLiteral("#171a18")));

    QQmlComponent component(&engine);
    component.setData(HostedAppletSource,
                      QUrl(QStringLiteral("inline:hosted-task-list-token-row.qml")));
    QTRY_VERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(root != nullptr);
    QCOMPARE(root->property("tokenReady").toBool(), true);
    QCOMPARE(root->property("backgroundBase").value<QColor>(),
             QColor(QStringLiteral("#171a18")));

    auto *hostedApplet = itemNamed(root, QStringLiteral("taskListApplet"));
    auto *loadingLabel = itemNamed(root, QStringLiteral("taskListLoadingLabel"));
    QVERIFY(hostedApplet != nullptr);
    QVERIFY(loadingLabel != nullptr);
    QCOMPARE(loadingLabel->property("color").value<QColor>(),
             publisher.facade()->fg()
                 .value(QStringLiteral("muted"))
                 .value<QColor>());

    const qulonglong priorGeneration = publisher.facade()->generation();
    QVERIFY(themes.selectById(QStringLiteral("qinda-light")));
    QCOMPARE(publisher.facade()->generation(), priorGeneration + 1);
    QCOMPARE(root->property("backgroundBase").value<QColor>(),
             QColor(QStringLiteral("#e9e8e4")));
}

QTEST_MAIN(ShellRuntimeTokenTests)
#include "tst_shellruntime_tokens.moc"
