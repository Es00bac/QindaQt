// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/window_management/command.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTest>
#include <limits>
using namespace QindaQt::WindowManagement;
namespace {
QByteArray request(QString op, QJsonObject arguments = {}, QJsonObject target = {{"kind", "current"}})
{
    return QJsonDocument(QJsonObject{{"version",1},{"operation",op},{"target",target},{"arguments",arguments}}).toJson(QJsonDocument::Compact);
}
}
class GeometryCodecTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void panelAndNegativeOrigin()
    {
        const QRect area(-1920, 48, 1920, 1032);
        const auto frame = regionalFrame(area, insetRegion());
        QVERIFY(frame);
        QCOMPARE(frame->x(), -1824);
        QCOMPARE(frame->y(), 100);
        QCOMPARE(frame->width(), 1728);
        QVERIFY(area.contains(*frame));
        QCOMPARE(regionalFrame(area, {0,0,1,1}), std::optional(area));
    }
    void adjacentThirdsAndQuadrants()
    {
        const QRect area(17, 55, 1919, 1023);
        const auto left=regionalFrame(area,{0,0,1.0/3,1});
        const auto middle=regionalFrame(area,{1.0/3,0,1.0/3,1});
        const auto right=regionalFrame(area,{2.0/3,0,1.0/3,1});
        QVERIFY(left && middle && right);
        QCOMPARE(left->right()+1,middle->left());
        QCOMPARE(middle->right()+1,right->left());
        QCOMPARE(right->right(),area.right());
        const auto upperRight=regionalFrame(area,{.5,0,.5,.5});
        QVERIFY(upperRight && area.contains(*upperRight));
    }
    void refusesInvalidGeometry()
    {
        const QRect area(0,40,640,440);
        QVERIFY(!regionalFrame(area,{-0.1,0,.5,.5}));
        QVERIFY(!regionalFrame(area,{.5,.5,.6,.5}));
        QVERIFY(!regionalFrame(area,{0,0,0,1}));
        QVERIFY(!regionalFrame(area,{0,0,std::numeric_limits<double>::infinity(),1}));
        QVERIFY(insetRegion(.09).isEmpty());
    }
    void parsesComposableRequests()
    {
        const auto maximize=decodeCommand(request("maximize"));
        QVERIFY(maximize);
        QCOMPARE(maximize->operation,Operation::Maximize);
        QVERIFY(decodeCommand(request("place",{{"region",QJsonArray{0,0,1.0/3,1}}})));
        QVERIFY(decodeCommand(request("group-tile",{{"destination",QJsonObject{{"kind","container"},{"name","Research"}}},{"direction","left"},{"ratio",.3}})));
        QVERIFY(decodeCommand(request("launch",{{"desktopEntryId","firefox.desktop"},{"placement","tab"},{"containerName","Research"}})));
        QVERIFY(decodeCommand(request("rename",{{"name",QString::fromUtf8("Research 🦊")}})));
    }
    void refusesMalformedAndUnsafeRequests()
    {
        QVERIFY(!decodeCommand(QByteArray(MaximumRequestBytes+1,'x')));
        QVERIFY(!decodeCommand(request("maximize",{{"fraction",90}})));
        QVERIFY(!decodeCommand(request("color",{{"value","red"}})));
        QVERIFY(!decodeCommand(request("place",{{"region",QJsonArray{0,0,2,1}}})));
        QVERIFY(!decodeCommand(request("activate-tab",{{"index",1.5}})));
        QVERIFY(!decodeCommand(request("launch",{{"desktopEntryId","/tmp/fake.desktop"},{"placement","tab"}})));
        QVERIFY(!decodeCommand(request("rename",{{"name","bad\nname"}})));
        QVERIFY(!decodeCommand(request("close",{{"force",true}})));
        QVERIFY(!decodeCommand(request("focus",{},{{"kind","window"},{"id","a"},{"name","b"}})));
        QVERIFY(!decodeCommand(request("focus",{},{{"kind","current"},{"id","a"}})));
    }
};
QTEST_GUILESS_MAIN(GeometryCodecTest)
#include "tst_geometry_codec.moc"
