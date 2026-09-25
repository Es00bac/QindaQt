// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "install_preflight.h"
#include "job_fakes.h"

using namespace QindaQt::QindaLutris;
using namespace QindaQt::QindaLutris::TestSupport;

// The shared install preflight (ADR-0275 section 5) and its Vulkan driver
// heuristic, over manifests shaped like Mesa's and the loader's.
namespace {

QByteArray manifest(const QByteArray &library, const QByteArray &arch = {}) {
  QByteArray icd = "\"library_path\": \"" + library + "\"";
  if (!arch.isEmpty()) {
    icd += ", \"library_arch\": \"" + arch + "\"";
  }
  return "{\"file_format_version\": \"1.0.1\", \"ICD\": {" + icd + ", \"api_version\": \"1.4.0\"}}";
}

} // namespace

class tst_install_preflight : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void vulkanManifests_data() {
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("usable");
    QTest::newRow("radeon 64") << manifest("/usr/lib64/libvulkan_radeon.so", "64")
                               << QStringLiteral("radeon_icd.x86_64.json") << true;
    QTest::newRow("radeon 32 only") << manifest("/usr/lib/libvulkan_radeon.so", "32")
                                    << QStringLiteral("radeon_icd.i686.json") << false;
    QTest::newRow("i686 name without arch") << manifest("/usr/lib/libvulkan_intel.so")
                                            << QStringLiteral("intel_icd.i686.json") << false;
    QTest::newRow("nvidia no arch") << manifest("libGLX_nvidia.so.0")
                                    << QStringLiteral("nvidia_icd.json") << true;
    QTest::newRow("lavapipe") << manifest("/usr/lib64/libvulkan_lvp.so", "64")
                              << QStringLiteral("lvp_icd.x86_64.json") << false;
    QTest::newRow("swiftshader") << manifest("/usr/lib64/libvk_swiftshader.so")
                                 << QStringLiteral("vk_swiftshader_icd.json") << false;
    QTest::newRow("no library") << QByteArray("{\"ICD\": {}}") << QStringLiteral("x.json") << false;
    QTest::newRow("not json") << QByteArray("garbage") << QStringLiteral("x.json") << false;
  }
  void vulkanManifests() {
    QFETCH(QByteArray, json);
    QFETCH(QString, fileName);
    QFETCH(bool, usable);
    QCOMPARE(isUsableVulkanIcdManifest(json, fileName), usable);
  }

  void preflightOrderAndMessages() {
    QTemporaryDir dir(QDir::homePath() + QStringLiteral("/preflight-XXXXXX"));
    FakeProbe probe;
    PreflightRequest request;
    request.displayName = QStringLiteral("Battle.net");
    request.spacePath = dir.filePath(QStringLiteral("Games/battlenet"));
    request.minimumFreeBytes = qint64(2) * 1024 * 1024 * 1024;
    request.protonBuildPath = makeProtonBuild(dir.path(), QStringLiteral("GE-Proton11-6-x86_64"));

    PreflightOutcome outcome = runInstallPreflight(request, probe);
    QVERIFY(outcome.ok);
    QCOMPARE(outcome.umuRunBinary, QStringLiteral("/usr/bin/umu-run"));

    probe.freeBytes = qint64(512) * 1024 * 1024;
    outcome = runInstallPreflight(request, probe);
    QVERIFY(!outcome.ok);
    QCOMPARE(outcome.message,
             QStringLiteral("There is not enough free disk space for Battle.net: it needs at least "
                            "2.0 GB, and only 512.0 MB is free. Free up some space, then try again."));

    probe.freeBytes = std::nullopt; // unknown space never blocks
    QVERIFY(runInstallPreflight(request, probe).ok);

    probe.vulkan = false;
    QVERIFY(runInstallPreflight(request, probe).message.contains(QStringLiteral("Vulkan")));
    probe.umu.clear();
    QVERIFY(runInstallPreflight(request, probe).message.contains(QStringLiteral("umu-launcher")));
    request.protonBuildPath = QStringLiteral("GE-Proton");
    QVERIFY(runInstallPreflight(request, probe).message.contains(QStringLiteral("No Proton build")));
  }
};

QTEST_GUILESS_MAIN(tst_install_preflight)
#include "tst_install_preflight.moc"
