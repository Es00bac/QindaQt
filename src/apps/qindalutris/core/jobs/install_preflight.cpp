// SPDX-License-Identifier: GPL-3.0-or-later
#include "install_preflight.h"

#include "fs_ops.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QStorageInfo>

namespace QindaQt::QindaLutris {

SystemProbe::~SystemProbe() = default;

std::optional<qint64> HostSystemProbe::availableBytes(const QString &path) const {
  const QString existing = nearestExistingDirectory(path);
  if (existing.isEmpty()) {
    return std::nullopt;
  }
  const QStorageInfo storage(existing);
  if (!storage.isValid() || !storage.isReady()) {
    return std::nullopt;
  }
  return storage.bytesAvailable();
}

QString HostSystemProbe::umuRunBinary() const {
  return QStandardPaths::findExecutable(QStringLiteral("umu-run"));
}

bool HostSystemProbe::hasVulkanDriver() const {
  for (const char *variable : {"VK_DRIVER_FILES", "VK_ICD_FILENAMES"}) {
    const QString value = qEnvironmentVariable(variable);
    for (const QString &file : value.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
      if (QFileInfo(file).isFile()) {
        return true;
      }
    }
  }
  QStringList roots;
  for (const QString &base : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation)) {
    roots.append(base + QStringLiteral("/vulkan/icd.d"));
  }
  roots.append(QStringLiteral("/etc/vulkan/icd.d"));
  for (const QString &base : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
    roots.append(base + QStringLiteral("/vulkan/icd.d"));
  }
  for (const QString &root : std::as_const(roots)) {
    if (!QDir(root).entryList({QStringLiteral("*.json")}, QDir::Files | QDir::Readable).isEmpty()) {
      return true;
    }
  }
  return false;
}

QString plainSize(qint64 bytes) {
  return QLocale::c().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

PreflightOutcome runInstallPreflight(const PreflightRequest &request, const SystemProbe &probe) {
  PreflightOutcome outcome;
  const QString name = request.displayName;
  const QFileInfo build(request.protonBuildPath);
  if (request.protonBuildPath.isEmpty() || QDir::isRelativePath(request.protonBuildPath)) {
    outcome.message = QStringLiteral(
        "No Proton build is chosen for %1. Choose one under Proton builds, or install "
        "the app-emulation/ge-proton-bin package.")
                          .arg(name);
    outcome.details.append(QStringLiteral("Preflight: no absolute Proton build path given."));
    return outcome;
  }
  if (!build.isDir() || !QFileInfo(request.protonBuildPath + QStringLiteral("/proton")).isFile()) {
    outcome.message = QStringLiteral(
        "The chosen Proton build is missing or damaged. Choose another one under "
        "Proton builds.");
    outcome.details.append(
        QStringLiteral("Preflight: %1 has no proton script.").arg(request.protonBuildPath));
    return outcome;
  }
  outcome.details.append(QStringLiteral("Preflight: Proton build %1").arg(request.protonBuildPath));

  const QString umu = probe.umuRunBinary();
  if (umu.isEmpty()) {
    outcome.message = QStringLiteral(
        "The game runner umu is not installed. Install the games-util/umu-launcher "
        "package, then try again.");
    outcome.details.append(QStringLiteral("Preflight: umu-run not found on PATH."));
    return outcome;
  }
  outcome.details.append(QStringLiteral("Preflight: umu-run at %1").arg(umu));

  if (!probe.hasVulkanDriver()) {
    outcome.message = QStringLiteral(
        "No Vulkan graphics driver was found. Install your graphics card's Vulkan "
        "driver (media-libs/mesa with the vulkan flag, or x11-drivers/nvidia-drivers), "
        "then try again.");
    outcome.details.append(QStringLiteral("Preflight: no Vulkan ICD manifest found."));
    return outcome;
  }

  const std::optional<qint64> free = probe.availableBytes(request.spacePath);
  if (!free) {
    outcome.details.append(
        QStringLiteral("Preflight: free space unknown for %1; continuing.").arg(request.spacePath));
  } else {
    outcome.details.append(QStringLiteral("Preflight: %1 free, %2 needed")
                               .arg(*free)
                               .arg(request.minimumFreeBytes));
    if (*free < request.minimumFreeBytes) {
      outcome.message = QStringLiteral(
          "There is not enough free disk space for %1: it needs at least %2, and "
          "only %3 is free. Free up some space, then try again.")
                            .arg(name, plainSize(request.minimumFreeBytes), plainSize(*free));
      return outcome;
    }
  }
  outcome.ok = true;
  outcome.umuRunBinary = umu;
  return outcome;
}

} // namespace QindaQt::QindaLutris
