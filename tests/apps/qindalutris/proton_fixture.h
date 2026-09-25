// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTest>

#include "proton_catalog.h"

// Shared synthetic Proton trees for the ADR-0275 rows. A "build" is a
// directory holding a shell stub named `proton` (never run) plus, usually,
// a `version` file -- the shape app-emulation/ge-proton-bin installs.
namespace ProtonFixture {

using QindaQt::QindaLutris::ProtonBuild;

inline void writeFile(const QString &path, const QByteArray &bytes,
                      bool executable = false) {
  QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(bytes), qint64(bytes.size()));
  file.close();
  if (executable) {
    QVERIFY(QFile::setPermissions(
        path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  }
}

// The version line a fixture build gets unless a test says otherwise.
inline QByteArray defaultVersion(const QString &name) {
  return "1700000000 " + name.toUtf8();
}

// Creates <root>/<name> and returns its canonical path. An empty version
// writes no version file; displayName, when given, writes a
// compatibilitytool.vdf.
inline QString makeBuild(const QString &root, const QString &name,
                         const QByteArray &version,
                         const QByteArray &displayName = {}) {
  const QString dir = root + QLatin1Char('/') + name;
  writeFile(dir + QStringLiteral("/proton"), "#!/bin/sh\nexit 0\n", true);
  if (!version.isEmpty()) {
    writeFile(dir + QStringLiteral("/version"), version + "\n");
  }
  if (!displayName.isEmpty()) {
    writeFile(dir + QStringLiteral("/compatibilitytool.vdf"),
              "\"compatibilitytools\"\n{\n  \"compat_tools\"\n  {\n"
              "    \"" + name.toUtf8() + "\"\n    {\n"
              "      \"install_path\" \".\"\n"
              "      \"display_name\" \"" + displayName + "\"\n"
              "    }\n  }\n}\n");
  }
  return QDir(dir).canonicalPath();
}

inline QString makeBuild(const QString &root, const QString &name) {
  return makeBuild(root, name, defaultVersion(name));
}

// A catalog entry for pure tests (no files behind it).
inline ProtonBuild build(const QString &name, ProtonBuild::Origin origin,
                         const QString &version = {},
                         const QString &path = {}) {
  ProtonBuild out;
  out.name = name;
  out.displayName = name;
  out.versionText = version.isEmpty() ? QString::fromUtf8(defaultVersion(name))
                                      : version;
  out.path = path.isEmpty() ? QStringLiteral("/roots/") + name : path;
  out.origin = origin;
  out.removable = origin == ProtonBuild::Origin::User;
  out.pinnable =
      !QindaQt::QindaLutris::isRollingProtonChannel(name, origin);
  return out;
}

inline QStringList names(const QVector<ProtonBuild> &builds) {
  QStringList out;
  for (const ProtonBuild &b : builds) {
    out.append(b.name);
  }
  return out;
}

} // namespace ProtonFixture
