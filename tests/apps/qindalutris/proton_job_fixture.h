// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixture for the ProtonInstallJob rows: a private target root, a
// scripted downloader serving real .tar.gz files built with the host's tar,
// the production QProcessRunner (process-group containment, short stop
// timings) and a fake SystemProbe.

#include "job_fakes.h"
#include "proton_install_job.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace QindaQt::QindaLutris::TestSupport {

inline const QString kTool = QStringLiteral("GE-Proton99-1-x86_64");

inline GeProtonRelease testRelease() {
  GeProtonRelease release;
  release.tagName = QStringLiteral("GE-Proton99-1");
  release.toolName = kTool;
  release.tarballName = kTool + QStringLiteral(".tar.gz");
  release.checksumName = kTool + QStringLiteral(".sha512sum");
  release.tarballUrl = geProtonAssetUrl(release.tagName, release.tarballName);
  release.checksumUrl = geProtonAssetUrl(release.tagName, release.checksumName);
  return release;
}

// Builds a real .tar.gz with the host's tar from <workDir>/src. extraArgs go
// before the member list (-P keeps '../' names for escape fixtures).
inline QByteArray makeTarball(const QString &workDir, const QStringList &members,
                              const QStringList &extraArgs = {}) {
  const QString out = workDir + QStringLiteral("/out.tar.gz");
  QFile::remove(out);
  QProcess tar;
  QStringList args{QStringLiteral("-czf"), out};
  args << extraArgs << QStringLiteral("-C") << workDir + QStringLiteral("/src") << members;
  tar.start(QStringLiteral("tar"), args);
  if (!tar.waitForFinished(20000) || tar.exitCode() != 0) {
    qWarning() << "tar failed" << tar.readAllStandardError();
    return {};
  }
  QFile file(out);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

inline QByteArray sumFor(const QByteArray &tarball, const QString &name) {
  return QCryptographicHash::hash(tarball, QCryptographicHash::Sha512).toHex() + "  " +
         name.toUtf8() + "\n";
}

// Writes an executable shell script (a stand-in `tar` for cancel rows).
inline QString writeScript(const QString &path, const QByteArray &body) {
  QFile script(path);
  if (script.open(QIODevice::WriteOnly)) {
    script.write("#!/bin/sh\n" + body);
    script.close();
    script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  }
  return path;
}

struct ProtonFixture {
  QTemporaryDir dir{QDir::homePath() + QStringLiteral("/proton-XXXXXX")};
  QString root = dir.filePath(QStringLiteral("compatibilitytools.d"));
  QString work = dir.filePath(QStringLiteral("work"));
  QString source = work + QStringLiteral("/src/") + kTool;
  FakeDownloader downloader;
  QProcessRunner runner{ProcessContainment::ProcessGroup};
  FakeProbe probe;
  ProtonInstallJob job{root, &downloader, &runner, &probe};
  std::optional<ProtonJobResult> result;

  ProtonFixture() {
    runner.setStopTimings(1000, 2000);
    QDir().mkpath(source + QStringLiteral("/files"));
    writeFile(source + QStringLiteral("/proton"), 64);
    writeFile(source + QStringLiteral("/compatibilitytool.vdf"), 32);
    QObject::connect(&job, &ProtonInstallJob::finished,
                     [this](const ProtonJobResult &r) { result = r; });
  }

  void serve(const QByteArray &tarball, const QByteArray &sums) {
    const GeProtonRelease release = testRelease();
    downloader.serve(release.tarballUrl, tarball);
    downloader.serve(release.checksumUrl, sums);
  }

  void serveBuild() {
    const QByteArray tarball = makeTarball(work, {kTool});
    serve(tarball, sumFor(tarball, kTool + QStringLiteral(".tar.gz")));
  }

  bool run(const GeProtonRelease &release = testRelease()) {
    job.start(release);
    return wait();
  }

  bool wait() { return QTest::qWaitFor([this] { return result.has_value(); }, 20000); }

  QStringList rootEntries() const {
    return QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
  }

  // Every file named `name` under the fixture, outside the work/ sources.
  QStringList filesNamed(const QString &name) const {
    QStringList found;
    QDirIterator it(dir.path(), {name}, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QString path = it.next();
      if (!path.startsWith(work + QLatin1Char('/'))) {
        found.append(path);
      }
    }
    return found;
  }
};

} // namespace QindaQt::QindaLutris::TestSupport
