// SPDX-License-Identifier: GPL-3.0-or-later

#include "color_settings_test_support.h"
#include "src/apps/settings/color/color_route_composition.h"

#include <qindaqt/services/display_color_discovery/profile_discovery.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <unistd.h>

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsColor;
using QindaQt::Apps::SettingsColor::TestSupport::writeFixtureProfile;

// Fresh-XDG-home composition rows. The environment is redirected before any
// QStandardPaths lookup, so no host profile directory is read or written; the
// home itself lives under the test binary directory, never the shared tmpfs.
class ColorRouteCompositionTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void compositionProvisionsTheUserImportRoot();
  void freshUserImportSucceedsThroughThePublicProvider();

private:
  QString userRootPath() const {
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericDataLocation) +
           QStringLiteral("/color/icc");
  }

  std::unique_ptr<QTemporaryDir> m_home;
};

void ColorRouteCompositionTest::initTestCase() {
  m_home = std::make_unique<QTemporaryDir>(
      QCoreApplication::applicationDirPath() +
      QStringLiteral("/color-composition-home-XXXXXX"));
  QVERIFY(m_home->isValid());
  qSetEnv("XDG_DATA_HOME", m_home->path() + QStringLiteral("/data"));
  qSetEnv("XDG_DATA_DIRS", m_home->path() + QStringLiteral("/system-data"));
}

void ColorRouteCompositionTest::compositionProvisionsTheUserImportRoot() {
  const QString rootPath = userRootPath();
  QVERIFY(!QFileInfo::exists(rootPath));

  const ColorRouteComposition composition;

  const QFileInfo root(rootPath);
  QVERIFY2(root.exists() && root.isDir(), qPrintable(rootPath));
  // AGENT-CONTRACT: The C1 writer (ImportRootAccess::open) admits only an
  // existing, EUID-owned, non-group/other-writable root; the composition's
  // provisioning must satisfy exactly that contract on a fresh home.
  QCOMPARE(root.ownerId(), static_cast<quint32>(::geteuid()));
  QVERIFY(!(root.permissions() &
            (QFileDevice::WriteGroup | QFileDevice::WriteOther)));
}

void ColorRouteCompositionTest::freshUserImportSucceedsThroughThePublicProvider() {
  const ColorRouteComposition composition;
  QVERIFY(QFileInfo::exists(userRootPath()));

  const QString source = writeFixtureProfile(QDir(m_home->path()),
                                             QStringLiteral("fresh.icc"));
  QVERIFY(!source.isEmpty());
  const DisplayColor::ProfileDiscovery discovery(
      {{userRootPath(), DisplayColor::DiscoveryOrigin::UserImported}});
  const DisplayColor::ImportResult result = discovery.importUserProfile(source);
  QCOMPARE(result.status, DisplayColor::ImportStatus::Imported);
  QVERIFY(QFileInfo::exists(userRootPath() + QStringLiteral("/fresh.icc")));
}

QTEST_GUILESS_MAIN(ColorRouteCompositionTest)
#include "tst_color_route_composition.moc"
