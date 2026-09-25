// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include "proton_fixture.h"
#include "proton_pin.h"

using namespace QindaQt::QindaLutris;
using namespace ProtonFixture;

// ADR-0275 pin identity (name + version) and resolution. Pure catalog
// values, plus fixture trees for the reviewer's R1/R1b/R2 reproductions.
class tst_proton_pin : public QObject {
  Q_OBJECT

  using Failure = PinnedBuildResolution::Failure;
  static constexpr ProtonBuild::Origin System = ProtonBuild::Origin::System;
  static constexpr ProtonBuild::Origin User = ProtonBuild::Origin::User;
  static constexpr ProtonBuild::Origin Steam = ProtonBuild::Origin::Steam;

private Q_SLOTS:
  void aliasRefusalTable() {
    const QVector<ProtonBuild> known{
        build(QStringLiteral("GE-Proton11-6"), System),
        build(QStringLiteral("Proton - Experimental"), Steam)};
    const QStringList floating{
        QString(), QStringLiteral("  "), QStringLiteral("GE-Proton"),
        QStringLiteral("ge-proton"), QStringLiteral("GE-Latest"),
        QStringLiteral("UMU-Latest"), QStringLiteral("umu-latest"),
        QStringLiteral("UMU-Proton"), QStringLiteral("latest"),
        QStringLiteral("LATEST"), QStringLiteral("GE-Proton11-7"),
        QStringLiteral("relative/path"),
        QStringLiteral("Proton - Experimental")}; // R1: rolling = floating
    for (const QString &value : floating) {
      QVERIFY2(isFloatingProtonAlias(value, known), qPrintable(value));
    }
    QVERIFY(!isFloatingProtonAlias(QStringLiteral("GE-Proton11-6"), known));
    QVERIFY(!isFloatingProtonAlias(
        QStringLiteral("/usr/share/steam/compatibilitytools.d/GE-Proton11-6"),
        known));
  }

  void resolvesByNameAndVersionNeverSubstitutes() {
    const QVector<ProtonBuild> builds{
        build(QStringLiteral("GE-Proton11-7"), System),
        build(QStringLiteral("GE-Proton11-6-x86_64"), System,
              QStringLiteral("1756415527 GE-Proton11-6")),
    };
    const PinnedBuildResolution exact = resolvePinnedBuild(
        {QStringLiteral("GE-Proton11-6-x86_64"),
         QStringLiteral("1756415527 GE-Proton11-6")}, builds);
    QVERIFY(exact.ok());
    QCOMPARE(exact.build->name, QStringLiteral("GE-Proton11-6-x86_64"));
    QCOMPARE(exact.failure, Failure::None);

    // The WoW case: pinned 11-6 is gone, 11-7 is installed. Refuse.
    const PinnedBuildResolution missing = resolvePinnedBuild(
        {QStringLiteral("GE-Proton11-6"), QStringLiteral("1 GE-Proton11-6")},
        {builds.at(0)});
    QCOMPARE(missing.failure, Failure::NotInstalled);
    QCOMPARE(missing.reason,
             QStringLiteral("GE-Proton11-6 is not installed. Reinstall it or "
                            "choose another Proton build for this game."));
    QVERIFY(!resolvePinnedBuild({QStringLiteral("GE-Proton11"), QStringLiteral("x")},
                                builds).ok());
    QVERIFY(!resolvePinnedBuild({QStringLiteral("ge-proton11-7"),
                                 QStringLiteral("1700000000 GE-Proton11-7")},
                                builds).ok());

    const PinnedBuildResolution alias =
        resolvePinnedBuild({QStringLiteral("GE-Proton"), QStringLiteral("x")}, builds);
    QCOMPARE(alias.failure, Failure::FloatingAlias);
    const PinnedBuildResolution empty = resolvePinnedBuild({}, builds);
    QCOMPARE(empty.failure, Failure::NotChosen);
    QCOMPARE(empty.reason, QStringLiteral("Choose a Proton build for this game."));
    // A pin with no recorded version is never assumed to match.
    const PinnedBuildResolution unconfirmed =
        resolvePinnedBuild({QStringLiteral("GE-Proton11-7"), QString()}, builds);
    QCOMPARE(unconfirmed.failure, Failure::NotConfirmed);
    QVERIFY(unconfirmed.reason.contains(QStringLiteral("Confirm")));
  }

  // Review R1b: Steam updates "Proton 9.0" in place (9.0-2 -> 9.0-4).
  void inPlaceUpdateIsRefusedWithBothVersions() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString common = tmp.filePath(QStringLiteral("steamapps/common"));
    makeBuild(common, QStringLiteral("Proton 9.0"), "1718395834 proton-9.0-2");
    const QVector<ProtonBuild> before =
        discoverProtonBuilds({{common, Steam}});
    const ProtonPin pin = pinForBuild(before.first());
    QVERIFY(resolvePinnedBuild(pin, before).ok());
    writeFile(common + QStringLiteral("/Proton 9.0/version"),
              "1730000000 proton-9.0-4\n");
    const QVector<ProtonBuild> after = discoverProtonBuilds({{common, Steam}});
    const PinnedBuildResolution changed = resolvePinnedBuild(pin, after);
    QVERIFY(!changed.ok());
    QCOMPARE(changed.failure, Failure::VersionChanged);
    QCOMPARE(changed.reason,
             QStringLiteral("Proton 9.0 has changed since this game was set up "
                            "(was proton-9.0-2, now proton-9.0-4). Confirm the "
                            "new version in QindaLutris before playing."));
    // The explicit re-pin records the new version; then it resolves.
    const std::optional<ProtonPin> confirmed =
        confirmPinnedBuild(pin.name, after);
    QVERIFY(confirmed.has_value());
    QCOMPARE(confirmed->version, QStringLiteral("1730000000 proton-9.0-4"));
    QVERIFY(resolvePinnedBuild(*confirmed, after).ok());
  }

  void systemWinsWhenNameAndVersionAgree() {
    const ProtonBuild user = build(QStringLiteral("GE-Proton11-6"), User, {},
                                   QStringLiteral("/user/GE-Proton11-6"));
    const ProtonBuild sys = build(QStringLiteral("GE-Proton11-6"), System, {},
                                  QStringLiteral("/sys/GE-Proton11-6"));
    const ProtonPin pin = pinForBuild(user);
    QCOMPARE(resolvePinnedBuild(pin, {sys, user}).build->path,
             QStringLiteral("/sys/GE-Proton11-6"));
    // Different contents under the same name: the pin's version decides.
    ProtonBuild rebuilt = sys;
    rebuilt.versionText = QStringLiteral("1800000000 GE-Proton11-6");
    QCOMPARE(resolvePinnedBuild(pin, {rebuilt, user}).build->path,
             QStringLiteral("/user/GE-Proton11-6"));
  }

  // Review R2: a legacy absolute pin to a USER build keeps resolving after
  // Portage installs a same-named SYSTEM build.
  void legacyAbsolutePinSurvivesAShadowingSystemCopy() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString sysRoot = tmp.filePath(QStringLiteral("system"));
    const QString userRoot = tmp.filePath(QStringLiteral("user"));
    const QString userPath = makeBuild(userRoot, QStringLiteral("GE-Proton9-1"));
    const QVector<ProtonRoot> roots{{sysRoot, System}, {userRoot, User}};
    const ProtonPin legacy{userPath + QStringLiteral("/proton"),
                           QString::fromUtf8(defaultVersion(QStringLiteral("GE-Proton9-1")))};
    QVERIFY(resolvePinnedBuild(legacy, discoverProtonBuilds(roots)).ok());
    makeBuild(sysRoot, QStringLiteral("GE-Proton9-1"));
    const PinnedBuildResolution after =
        resolvePinnedBuild(legacy, discoverProtonBuilds(roots));
    QVERIFY2(after.ok(), qPrintable(after.reason));
    QCOMPARE(after.build->path, userPath);
    QCOMPARE(after.build->origin, User);
    // A gone path is reported by its build name, not the directory.
    const PinnedBuildResolution gone = resolvePinnedBuild(
        {QStringLiteral("/old/GE-Proton8-1/proton"), QStringLiteral("v")},
        discoverProtonBuilds(roots));
    QCOMPARE(gone.failure, Failure::NotInstalled);
    QVERIFY(gone.reason.startsWith(QStringLiteral("GE-Proton8-1 is not installed.")));
  }

  void rollingChannelsAndVersionlessBuildsAreNotPinnable() {
    ProtonBuild noVersion = build(QStringLiteral("GE-Proton9-1"), User);
    noVersion.versionText.clear();
    noVersion.pinnable = false;
    const QVector<ProtonBuild> builds{
        build(QStringLiteral("Proton - Experimental"), Steam), noVersion};
    const PinnedBuildResolution rolling = resolvePinnedBuild(
        {QStringLiteral("Proton - Experimental"),
         builds.first().versionText}, builds);
    QCOMPARE(rolling.failure, Failure::NotPinnable);
    QCOMPARE(rolling.reason,
             QStringLiteral("Proton - Experimental is updated by Steam and cannot "
                            "be pinned. Choose another Proton build for this game."));
    QCOMPARE(resolvePinnedBuild({QStringLiteral("GE-Proton9-1"), QString()}, builds)
                 .failure,
             Failure::NotPinnable);
    QVERIFY(!pinForNewEntry(QStringLiteral("Proton - Experimental"), builds, {})
                 .has_value());
    QVERIFY(!confirmPinnedBuild(QStringLiteral("Proton - Experimental"), builds)
                 .has_value());
  }

  // Review R1: on a Steam-only machine nothing is a default.
  void defaultOrderIsPreferredThenSystemThenUserNeverSteam() {
    const ProtonBuild sys = build(QStringLiteral("GE-Proton11-6"), System);
    const ProtonBuild sysOld = build(QStringLiteral("GE-Proton10-25"), System);
    const ProtonBuild user = build(QStringLiteral("GE-Proton11-7"), User);
    const ProtonBuild valve = build(QStringLiteral("Proton 9.0"), Steam);
    const ProtonBuild experimental =
        build(QStringLiteral("Proton - Experimental"), Steam);
    // The catalog order is newest-first within origin.
    const QVector<ProtonBuild> all{sys, sysOld, user, experimental, valve};
    QCOMPARE(chooseDefaultBuild(all, QStringLiteral("Proton 9.0")),
             std::optional<ProtonBuild>(valve)); // explicitly preferred
    QCOMPARE(chooseDefaultBuild(all, QStringLiteral("Proton - Experimental")),
             std::optional<ProtonBuild>(sys)); // preferred but not pinnable
    QCOMPARE(chooseDefaultBuild(all, QStringLiteral("GE-Proton8-1")),
             std::optional<ProtonBuild>(sys));
    QCOMPARE(chooseDefaultBuild({user, valve}, QString()),
             std::optional<ProtonBuild>(user));
    QVERIFY(!chooseDefaultBuild({experimental, valve}, QString()).has_value());
    QVERIFY(!pinForNewEntry(QString(), {experimental, valve}, QString()).has_value());
    QVERIFY(!chooseDefaultBuild({}, QStringLiteral("GE-Proton11-6")).has_value());
  }

  void newEntriesPinANameAndVersion() {
    const ProtonBuild sys = build(QStringLiteral("GE-Proton11-6"), System);
    const ProtonBuild user = build(QStringLiteral("GE-Proton11-7"), User);
    QCOMPARE(pinForNewEntry(QString(), {sys, user}, QString()),
             std::optional<ProtonPin>(pinForBuild(sys)));
    QCOMPARE(pinForNewEntry(QString(), {sys, user}, QStringLiteral("GE-Proton11-7")),
             std::optional<ProtonPin>(pinForBuild(user)));
    // A path from protonChoices() is recorded as the build's name.
    QCOMPARE(pinForNewEntry(user.path, {sys, user}, QString()),
             std::optional<ProtonPin>(pinForBuild(user)));
    QCOMPARE(pinForNewEntry(user.path, {sys, user}, QString())->name,
             QStringLiteral("GE-Proton11-7"));
    QVERIFY(!pinForNewEntry(QStringLiteral("GE-Proton"), {sys, user}, QString()).has_value());
    QVERIFY(!pinForNewEntry(QStringLiteral("GE-Proton9-1"), {sys, user}, QString()).has_value());
    QVERIFY(!pinForNewEntry(QString(), {}, QString()).has_value());
    // confirm keeps a legacy path as a path.
    QCOMPARE(confirmPinnedBuild(user.path + QStringLiteral("/proton"), {sys, user}),
             std::optional<ProtonPin>(
                 ProtonPin{user.path + QStringLiteral("/proton"), user.versionText}));
  }
};

QTEST_GUILESS_MAIN(tst_proton_pin)
#include "tst_proton_pin.moc"
