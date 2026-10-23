// SPDX-License-Identifier: GPL-3.0-or-later
#include "containeridentityprojection.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QtTest>
using namespace QindaQt::Compositor::KWinIntegration;
class ContainerIdentityTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void updatesAndWithdrawsWithoutTouchingApplicationIdentity() {
        ContainerIdentityProjection projection;
        QSignalSpy changed(&projection, &ContainerIdentityProjection::changed);
        const QVariantMap identity{{"name", "Research"}, {"color", "#30A46C"}};
        projection.publish({{"member-a", identity}, {"member-b", identity}});
        QCOMPARE(changed.size(), 1);
        projection.publish(projection.byWindow());
        QCOMPARE(changed.size(), 1);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(SWITCHER_ROW)));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> row(component.createWithInitialProperties({
            {"index", 0}, {"caption", "Document — Browser"}, {"icon", "web-browser"},
            {"windowId", "{member-a}"}, {"minimized", false}, {"selected", false},
            {"appearancePalette", QVariantMap{}}, {"containerIdentities", projection.byWindow()}}));
        QVERIFY2(row, qPrintable(component.errorString()));
        QCOMPARE(row->property("displayCaption").toString(), QString("Research"));
        QCOMPARE(row->property("caption").toString(), QString("Document — Browser"));
        auto *icon = row->findChild<QQuickItem *>(QStringLiteral("switcherIdentityIcon"));
        QVERIFY(icon);
        QCOMPARE(icon->property("color").value<QColor>(), QColor("#30A46C"));
        row->setProperty("selected", true);
        QCOMPARE(icon->property("color").value<QColor>(), QColor("#30A46C"));
        row->setProperty("windowId", "{member-b}");
        QCOMPARE(row->property("displayCaption").toString(), QString("Research"));
        projection.publish({{"member-b", QVariantMap{{"name", "Build"}, {"color", "#E5484D"}}}});
        row->setProperty("containerIdentities", projection.byWindow());
        QCOMPARE(row->property("displayCaption").toString(), QString("Build"));
        projection.publish({});
        row->setProperty("containerIdentities", projection.byWindow());
        QCOMPARE(row->property("displayCaption").toString(), QString("Document — Browser"));
        QVERIFY(row->property("container").isNull());
    }
};
QTEST_MAIN(ContainerIdentityTest)
#include "tst_containeridentity.moc"
