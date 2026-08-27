// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

using namespace QindaQt::DesignTokens;
using namespace QindaQt::Themes;

namespace {

ThemeSpec builtIn(const QString &fileName)
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + fileName);
    if (!result.ok) {
        qFatal("built-in theme fixture failed: %s", qPrintable(result.error));
    }
    return result.theme;
}

} // namespace

class TokenFacadeTests final : public QObject {
    Q_OBJECT

private slots:
    void qmlConsumesOneReadOnlyGeneration();
    void refusesPublicationFromAnotherThread();
};

void TokenFacadeTests::qmlConsumesOneReadOnlyGeneration()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject {
            property bool tokensReady: Tokens.ready
            property color baseColor: Tokens.bg.base ?? "transparent"
            property real bodySize: Tokens.type.body ?? 0
            property int tokenGeneration: Tokens.generation
            property string themeId: Tokens.sourceThemeId
        }
    )qml",
                      QUrl(QStringLiteral("inline:qst-consumer.qml")));
    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 5000);
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> consumer(component.create());
    QVERIFY2(consumer != nullptr, qPrintable(component.errorString()));

    auto *facade = engine.singletonInstance<TokenFacade *>("QindaQt.Tokens", "Tokens");
    QVERIFY(facade != nullptr);
    QVERIFY(!facade->ready());
    QCOMPARE(consumer->property("tokensReady").toBool(), false);

    QSignalSpy changed(facade, &TokenFacade::tokensChanged);
    QString error;
    const ThemeSpec dark = builtIn(QStringLiteral("qinda-dark.json"));
    QVERIFY2(facade->publish(dark, {.basePointSize = 11.0, .textScale = 1.25}, &error),
             qPrintable(error));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(facade->generation(), 1ULL);
    QCOMPARE(consumer->property("tokensReady").toBool(), true);
    QCOMPARE(consumer->property("baseColor").value<QColor>(),
             dark.colors.value(QStringLiteral("canvas")));
    QCOMPARE(consumer->property("bodySize").toDouble(), 13.75);
    QCOMPARE(consumer->property("tokenGeneration").toULongLong(), 1ULL);
    QCOMPARE(consumer->property("themeId").toString(), dark.id);

    QVERIFY2(facade->publish(dark, {.basePointSize = 11.0, .textScale = 1.25}, &error),
             qPrintable(error));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(facade->generation(), 1ULL);

    const ThemeSpec light = builtIn(QStringLiteral("qinda-light.json"));
    QVERIFY2(facade->publish(light, {}, &error), qPrintable(error));
    QCOMPARE(changed.size(), 2);
    QCOMPARE(facade->generation(), 2ULL);
    QCOMPARE(consumer->property("baseColor").value<QColor>(),
             light.colors.value(QStringLiteral("canvas")));
    QCOMPARE(consumer->property("themeId").toString(), light.id);
}

void TokenFacadeTests::refusesPublicationFromAnotherThread()
{
    TokenFacade facade;
    const ThemeSpec theme = builtIn(QStringLiteral("qinda-dark.json"));
    bool published = true;
    QString error;
    std::unique_ptr<QThread> worker(QThread::create([&]() {
        published = facade.publish(theme, {}, &error);
    }));
    worker->start();
    QVERIFY(worker->wait());
    QVERIFY(!published);
    QVERIFY(error.contains(QStringLiteral("GUI thread")));
    QVERIFY(!facade.ready());
}

QTEST_MAIN(TokenFacadeTests)
#include "tst_token_facade.moc"
