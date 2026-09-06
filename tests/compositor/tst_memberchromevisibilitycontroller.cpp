// SPDX-License-Identifier: GPL-3.0-or-later
#include "memberchromevisibilitycontroller.h"

#include <QHash>
#include <QTest>

using QindaQt::Compositor::KWinIntegration::MemberChromeInspection;
using QindaQt::Compositor::KWinIntegration::MemberChromeVisibilityController;
using QindaQt::Compositor::KWinIntegration::MemberChromeVisibilitySummary;
using QindaQt::Core::LayoutNode;
using QindaQt::Core::WindowContainer;
using QindaQt::Hybrid::WindowTopology;

namespace {

struct FakeMember final
{
    MemberChromeInspection inspection;
    int writes = 0;
    bool rejectWrite = false;
};

WindowContainer container(QString id, QString firstWindow, QString secondWindow)
{
    WindowContainer result(std::move(id));
    QString error;
    if (!result.addPage(QStringLiteral("page"), QStringLiteral("first-leaf"),
                        std::move(firstWindow), &error)) {
        qFatal("test container page failed: %s", qPrintable(error));
    }
    if (!result.splitWindow(
            {.targetWindowId = result.pages().first().root().windowId(),
             .newWindowId = std::move(secondWindow),
             .newLeafNodeId = QStringLiteral("second-leaf"),
             .splitNodeId = QStringLiteral("split"),
             .orientation = QindaQt::Core::SplitOrientation::Horizontal,
             .ratio = 0.5,
             .position = QindaQt::Core::InsertPosition::Second},
            &error)) {
        qFatal("test container split failed: %s", qPrintable(error));
    }
    return result;
}

WindowTopology topology(QVector<WindowContainer> containers,
                        QStringList independent = {})
{
    QString error;
    auto result = WindowTopology::create(std::move(independent),
                                         std::move(containers), 1, &error);
    if (!result) {
        qFatal("test topology failed: %s", qPrintable(error));
    }
    return *result;
}

class Fixture final
{
public:
    Fixture()
        : controller(
              [this](const QString &id) -> std::optional<MemberChromeInspection> {
                  const auto it = members.constFind(id);
                  return it == members.cend()
                      ? std::nullopt
                      : std::optional<MemberChromeInspection>{it->inspection};
              },
              [this](const QString &id, bool noBorder, QString *error) {
                  auto it = members.find(id);
                  if (it == members.end()) {
                      return false;
                  }
                  if (it->rejectWrite) {
                      if (error) {
                          *error = QStringLiteral("injected rejection");
                      }
                      return false;
                  }
                  it->inspection.noBorder = noBorder;
                  // KWin removes the server decoration after noBorder changes;
                  // the controller must rely on its captured eligibility.
                  it->inspection.serverDecorated = !noBorder;
                  ++it->writes;
                  return true;
              })
    {
    }

    QHash<QString, FakeMember> members;
    MemberChromeVisibilityController controller;
};

} // namespace

class MemberChromeVisibilityControllerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hidesOnlyEligibleNativeTitlesAndRestoresExactBaseline();
    void preservesPreexistingBorderlessBaseline();
    void restoresBeforeMemberMovesBetweenGroups();
    void hiddenGroupAppliesChoiceToNewMember();
    void rejectedMutationRollsBackAppliedMembersAndChoice();
    void removedContainerRestoresNativeTitles();
    void shutdownRestorationIsIdempotent();
};

void MemberChromeVisibilityControllerTest::
    hidesOnlyEligibleNativeTitlesAndRestoresExactBaseline()
{
    Fixture fixture;
    fixture.members.insert(QStringLiteral("native"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("client"),
                           {{.noBorder = false,
                             .serverDecorated = false,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("already-borderless"),
                           {{.noBorder = true,
                             .serverDecorated = false,
                             .userCanSetNoBorder = true}});
    auto first = topology({container(QStringLiteral("group"),
                                     QStringLiteral("native"),
                                     QStringLiteral("client"))},
                          {QStringLiteral("already-borderless")});
    QVERIFY(fixture.controller.synchronize(first));

    MemberChromeVisibilitySummary summary;
    QVERIFY(fixture.controller.setVisible(QStringLiteral("group"), false, &summary));
    QCOMPARE(summary.changedMembers, 1);
    QCOMPARE(summary.unchangedClientDecoratedMembers, 1);
    QVERIFY(fixture.members[QStringLiteral("native")].inspection.noBorder);
    QVERIFY(!fixture.members[QStringLiteral("client")].inspection.noBorder);
    QVERIFY(!fixture.controller.nativeTitleVisible(QStringLiteral("group"),
                                                   QStringLiteral("native")));
    QVERIFY(!fixture.controller.nativeTitleVisible(QStringLiteral("group"),
                                                   QStringLiteral("client")));

    QVERIFY(fixture.controller.setVisible(QStringLiteral("group"), true));
    QVERIFY(!fixture.members[QStringLiteral("native")].inspection.noBorder);
    QCOMPARE(fixture.members[QStringLiteral("native")].writes, 2);
    QCOMPARE(fixture.members[QStringLiteral("client")].writes, 0);
}

void MemberChromeVisibilityControllerTest::preservesPreexistingBorderlessBaseline()
{
    Fixture fixture;
    fixture.members.insert(QStringLiteral("native"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("borderless"),
                           {{.noBorder = true,
                             .serverDecorated = false,
                             .userCanSetNoBorder = true}});
    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("group"), QStringLiteral("native"),
                            QStringLiteral("borderless"))})));
    QVERIFY(fixture.controller.setVisible(QStringLiteral("group"), false));
    QVERIFY(fixture.controller.setVisible(QStringLiteral("group"), true));
    QVERIFY(!fixture.members[QStringLiteral("native")].inspection.noBorder);
    QVERIFY(fixture.members[QStringLiteral("borderless")].inspection.noBorder);
    QCOMPARE(fixture.members[QStringLiteral("borderless")].writes, 0);
}

void MemberChromeVisibilityControllerTest::restoresBeforeMemberMovesBetweenGroups()
{
    Fixture fixture;
    fixture.members.insert(QStringLiteral("moving"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("peer-a"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("peer-b"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("peer-c"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("a"), QStringLiteral("moving"),
                            QStringLiteral("peer-a")),
                  container(QStringLiteral("b"), QStringLiteral("peer-b"),
                            QStringLiteral("peer-c"))})));
    QVERIFY(fixture.controller.setVisible(QStringLiteral("a"), false));
    QVERIFY(fixture.members[QStringLiteral("moving")].inspection.noBorder);

    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("a"), QStringLiteral("peer-a"),
                            QStringLiteral("peer-c")),
                  container(QStringLiteral("b"), QStringLiteral("peer-b"),
                            QStringLiteral("moving"))})));
    QVERIFY(!fixture.members[QStringLiteral("moving")].inspection.noBorder);
    QVERIFY(fixture.members[QStringLiteral("peer-c")].inspection.noBorder);
    QCOMPARE(fixture.members[QStringLiteral("moving")].writes, 2);
}

void MemberChromeVisibilityControllerTest::hiddenGroupAppliesChoiceToNewMember()
{
    Fixture fixture;
    for (const auto &id : {QStringLiteral("first"), QStringLiteral("second"),
                           QStringLiteral("third")}) {
        fixture.members.insert(id, {{.noBorder = false,
                                     .serverDecorated = true,
                                     .userCanSetNoBorder = true}});
    }
    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("group"), QStringLiteral("first"),
                            QStringLiteral("second"))}, {QStringLiteral("third")})));
    QVERIFY(fixture.controller.setVisible(QStringLiteral("group"), false));
    const auto expanded = topology(
        {container(QStringLiteral("group"), QStringLiteral("first"),
                   QStringLiteral("third"))},
        {QStringLiteral("second")});
    QVERIFY(fixture.controller.synchronize(expanded));
    QVERIFY(fixture.members[QStringLiteral("third")].inspection.noBorder);
    QVERIFY(!fixture.members[QStringLiteral("second")].inspection.noBorder);
}

void MemberChromeVisibilityControllerTest::
    rejectedMutationRollsBackAppliedMembersAndChoice()
{
    Fixture fixture;
    fixture.members.insert(QStringLiteral("first"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("second"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("group"), QStringLiteral("first"),
                            QStringLiteral("second"))})));
    fixture.members[QStringLiteral("second")].rejectWrite = true;
    QString error;
    QVERIFY(!fixture.controller.setVisible(QStringLiteral("group"), false, nullptr,
                                           &error));
    QVERIFY(error.contains(QStringLiteral("injected rejection")));
    QVERIFY(fixture.controller.visible(QStringLiteral("group")));
    QVERIFY(!fixture.members[QStringLiteral("first")].inspection.noBorder);
    QVERIFY(!fixture.members[QStringLiteral("second")].inspection.noBorder);
}

void MemberChromeVisibilityControllerTest::removedContainerRestoresNativeTitles()
{
    Fixture fixture;
    fixture.members.insert(QStringLiteral("first"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("second"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("group"), QStringLiteral("first"),
                            QStringLiteral("second"))})));
    QVERIFY(fixture.controller.setVisible(QStringLiteral("group"), false));
    QVERIFY(fixture.controller.synchronize(
        topology({}, {QStringLiteral("first"), QStringLiteral("second")})));
    QVERIFY(!fixture.members[QStringLiteral("first")].inspection.noBorder);
    QVERIFY(!fixture.members[QStringLiteral("second")].inspection.noBorder);
    QCOMPARE(fixture.members[QStringLiteral("first")].writes, 2);
    QCOMPARE(fixture.members[QStringLiteral("second")].writes, 2);
}

void MemberChromeVisibilityControllerTest::shutdownRestorationIsIdempotent()
{
    Fixture fixture;
    fixture.members.insert(QStringLiteral("first"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    fixture.members.insert(QStringLiteral("second"),
                           {{.noBorder = false,
                             .serverDecorated = true,
                             .userCanSetNoBorder = true}});
    QVERIFY(fixture.controller.synchronize(
        topology({container(QStringLiteral("group"), QStringLiteral("first"),
                            QStringLiteral("second"))})));
    QVERIFY(fixture.controller.toggle(QStringLiteral("group")));
    QVERIFY(fixture.controller.restoreForShutdown());
    QVERIFY(fixture.controller.restoreForShutdown());
    QVERIFY(!fixture.members[QStringLiteral("first")].inspection.noBorder);
    QVERIFY(!fixture.members[QStringLiteral("second")].inspection.noBorder);
    QCOMPARE(fixture.members[QStringLiteral("first")].writes, 2);
    QCOMPARE(fixture.members[QStringLiteral("second")].writes, 2);
}

QTEST_GUILESS_MAIN(MemberChromeVisibilityControllerTest)
#include "tst_memberchromevisibilitycontroller.moc"
