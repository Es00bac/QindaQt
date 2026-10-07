// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixture_source.h"
#include "agent_usage_applet_controller.h"
#include "../icon_resolution_test_fixture.h"
#include "desktop_controls_qml_test_support.h"
#include <QAccessible>
#include <QDir>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_AgentUsageAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)
using namespace QindaQt;
using namespace QindaQt::Shell::AgentUsageApplet;
namespace {
QQuickItem *findVisual(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = findVisual(child, name)) return found;
    return nullptr;
}
struct Harness {
    FixtureSource source;
    AgentUsageAppletController controller;
    explicit Harness(bool granted=true) : controller(&source, granted) {}
    QQmlEngine engine;
    std::unique_ptr<QObject> owned;
    QQuickItem *root = nullptr;
    QQuickWindow window;
    bool load(const QSize panel, bool vertical, QString *error) {
        auto fresh = fixtureProvider(); fresh.state = UsageState::Ready;
        auto stale = fixtureProvider();
        stale.providerId=QStringLiteral("codex"); stale.displayName=QStringLiteral("Codex");
        stale.tokenScope=QStringLiteral("lifetime"); stale.costScope=QStringLiteral("unreported");
        stale.totalTokens=12345; stale.inputTokens=10000; stale.outputTokens=2345;
        stale.reportedCostUsd.reset();
        ProviderUsage absent; absent.providerId=QStringLiteral("kimi");
        absent.displayName=QStringLiteral("Kimi"); absent.detail=QStringLiteral("No local report configured");
        source.values = {fresh,stale,absent};
        source.refresh();
        engine.addImportPath(QStringLiteral(QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH));
        if (!Tests::DesktopControls::publishTokens(engine)) {
            *error = QStringLiteral("token publication failed"); return false;
        }
        if (!Tests::installResolvedIconFixture(engine,
                QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
                {QStringLiteral("utilities-system-monitor")}, error)) return false;
        QQmlComponent component(&engine);
        component.loadFromModule(QStringLiteral("QindaQt.Shell.AgentUsageApplet"),
                                 QStringLiteral("AgentUsageApplet"));
        if (!component.isReady()) { *error=component.errorString(); return false; }
        owned.reset(component.createWithInitialProperties({
            {QStringLiteral("access"), QVariant::fromValue(&controller)},
            {QStringLiteral("theme"), QVariantMap{{QStringLiteral("cornerRadius"), 8}}},
            {QStringLiteral("vertical"), vertical}}));
        root=qobject_cast<QQuickItem *>(owned.get());
        if (!root) { *error=component.errorString(); return false; }
        window.setGeometry(0,0,panel.width(),panel.height());
        root->setParentItem(window.contentItem());
        root->setSize(QSizeF(qMin(panel.width(),32),qMin(panel.height(),28)));
        window.show(); return true;
    }
    QObject *popup() { return root->findChild<QObject *>(QStringLiteral("agentUsageAppletPopup")); }
    QQuickItem *content() { return popup()->property("contentItem").value<QQuickItem *>(); }
};
}
class AgentUsageQmlTests final : public QObject {
    Q_OBJECT
private slots:
    void hostileMetadataUsesPlainText() {
        Harness h; QString error;
        QVERIFY2(h.load(QSize(480,30),false,&error),qPrintable(error));
        h.source.values = {fixtureProvider()};
        h.source.values.front().displayName=QStringLiteral("<b>Claude</b>");
        h.source.values.front().source=QStringLiteral("<img src='file:///private/report'/>");
        h.source.refresh();
        QTRY_VERIFY(h.window.isExposed());
        auto *summary=h.root->findChild<QQuickItem *>(QStringLiteral("agentUsageAppletSummary"));
        summary->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Space);
        QTRY_VERIFY(h.popup()->property("opened").toBool());
        auto *row=findVisual(h.content(),QStringLiteral("agentUsageProviderRow"));
        QVERIFY(row);
        bool literal=false;
        for (auto *item : row->childItems()) {
            if (item->metaObject()->indexOfProperty("textFormat")>=0) {
                QCOMPARE(item->property("textFormat").toInt(),0);
                literal |= item->property("text").toString().contains("<b>Claude</b>");
            }
        }
        QVERIFY(literal);
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Escape);
        QTRY_VERIFY(!h.popup()->property("opened").toBool());
    }
    void deniedPopupExplainsPolicyWithoutRefreshing() {
        Harness h(false); QString error;
        QVERIFY2(h.load(QSize(480,30),false,&error),qPrintable(error));
        QTRY_VERIFY(h.window.isExposed());
        const int before = h.source.refreshes;
        auto *summary = h.root->findChild<QQuickItem *>(QStringLiteral("agentUsageAppletSummary"));
        summary->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Space);
        QTRY_VERIFY(h.popup()->property("opened").toBool());
        QCOMPARE(h.source.refreshes,before);
        auto *refresh=findVisual(h.content(),QStringLiteral("agentUsageRefresh"));
        QVERIFY(refresh); QVERIFY(!refresh->isEnabled());
        auto *diagnostic=findVisual(h.content(),QStringLiteral("agentUsageDiagnostic"));
        QVERIFY(diagnostic); QVERIFY(diagnostic->property("text").toString().contains("denied"));
        QVERIFY(!findVisual(h.content(),QStringLiteral("agentUsageProviderRow")));
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Escape);
        QTRY_VERIFY(!h.popup()->property("opened").toBool());
    }
    void popupInteraction_data() {
        QTest::addColumn<QSize>("panel"); QTest::addColumn<bool>("vertical");
        QTest::newRow("horizontal") << QSize(480,30) << false;
        QTest::newRow("vertical") << QSize(24,480) << true;
    }
    void popupInteraction() {
        QFETCH(QSize,panel); QFETCH(bool,vertical);
        Harness h; QString error;
        QVERIFY2(h.load(panel,vertical,&error),qPrintable(error));
        QTRY_VERIFY(h.window.isExposed());
        auto *summary=h.root->findChild<QQuickItem *>(QStringLiteral("agentUsageAppletSummary"));
        QVERIFY(summary);
        auto *accessible=QAccessible::queryAccessibleInterface(summary);
        QVERIFY(accessible); QCOMPARE(accessible->role(),QAccessible::Button);
        QCOMPARE(accessible->text(QAccessible::Name),QStringLiteral("AI agent usage"));
        QVERIFY(accessible->text(QAccessible::Description).contains("reset"));
        const int before=h.source.refreshes;
        summary->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Space);
        QTRY_VERIFY(h.popup()->property("opened").toBool());
        QTRY_COMPARE(h.source.refreshes,before+1);
        QVERIFY(h.content()); QVERIFY(h.content()->window()!=&h.window);
        auto *refresh=findVisual(h.content(),QStringLiteral("agentUsageRefresh"));
        QVERIFY(refresh); refresh->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Space);
        QTRY_COMPARE(h.source.refreshes,before+2);
        auto *row=findVisual(h.content(),QStringLiteral("agentUsageProviderRow"));
        QVERIFY(row);
        for (auto *item : row->childItems()) {
            if (item->metaObject()->indexOfProperty("textFormat")>=0)
                QCOMPARE(item->property("textFormat").toInt(),0); // QQuickText::PlainText
        }
        QVERIFY(h.popup()->property("width").toReal()>0);
        const QString evidence=qEnvironmentVariable("QINDAQT_USAGE_EVIDENCE_DIR");
        if (!evidence.isEmpty()) {
            QVERIFY(QDir().mkpath(evidence));
            QTest::qWait(150); // settle software scene/font layout before actual window capture
            const QImage image=h.content()->window()->grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(evidence + (vertical ? QStringLiteral("/usage-vertical.png")
                                                   : QStringLiteral("/usage-horizontal.png"))));
        }
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Escape);
        QTRY_VERIFY(!h.popup()->property("opened").toBool());
        QTRY_VERIFY(summary->hasActiveFocus());
        QTest::mouseClick(&h.window,Qt::LeftButton,Qt::NoModifier,
                         summary->mapToScene(QPointF(summary->width()/2,summary->height()/2)).toPoint());
        QTRY_VERIFY(h.popup()->property("opened").toBool());
        QTRY_COMPARE(h.source.refreshes,before+3);
        QTest::keyClick(QGuiApplication::focusWindow(),Qt::Key_Escape);
        QTRY_VERIFY(!h.popup()->property("opened").toBool());
    }
};
QTEST_MAIN(AgentUsageQmlTests)
#include "tst_agent_usage_qml.moc"
