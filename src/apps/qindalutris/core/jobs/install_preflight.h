// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the host facts an install depends on (ADR-0275 section 5
// preflight), behind a seam so tests never read the real machine. Every
// method is cheap, synchronous and side-effect free.
class SystemProbe {
public:
  virtual ~SystemProbe();
  // Free bytes on the filesystem holding path (or its nearest existing
  // ancestor); nullopt when unknown.
  [[nodiscard]] virtual std::optional<qint64> availableBytes(const QString &path) const = 0;
  // Absolute path of umu-run; empty when not installed.
  [[nodiscard]] virtual QString umuRunBinary() const = 0;
  // True when at least one Vulkan ICD manifest is installed.
  [[nodiscard]] virtual bool hasVulkanDriver() const = 0;
};

// Production probe: QStorageInfo, PATH lookup, and the Vulkan loader's ICD
// manifest directories (XDG config/data dirs, /etc, /usr/share) or an
// explicit VK_DRIVER_FILES / VK_ICD_FILENAMES override; each manifest must
// pass isUsableVulkanIcdManifest().
class HostSystemProbe final : public SystemProbe {
public:
  [[nodiscard]] std::optional<qint64> availableBytes(const QString &path) const override;
  [[nodiscard]] QString umuRunBinary() const override;
  [[nodiscard]] bool hasVulkanDriver() const override;
};

// AGENT-NOTE: heuristic for "a working Vulkan driver" (ADR-0275 section 5).
// A manifest counts only when it names a library_path, is not a software
// rasterizer (lavapipe `lvp`, SwiftShader) -- games on those are unplayable
// and the user needs their real GPU driver -- and is not 32-bit only
// (library_arch "32", or an .i686/.i386 manifest name without an arch
// field): Proton's 64-bit side needs a 64-bit driver. Pure.
[[nodiscard]] bool isUsableVulkanIcdManifest(const QByteArray &json, const QString &fileName);

struct PreflightRequest final {
  QString displayName;     // "Battle.net", or the game title
  QString spacePath;       // where the prefix will live
  qint64 minimumFreeBytes = 0;
  QString protonBuildPath; // absolute directory of the pinned build
};

struct PreflightOutcome final {
  bool ok = false;
  QString message;       // ONE plain sentence naming the fix (package)
  QString umuRunBinary;  // resolved when ok
  QStringList details;   // for the job log
};

// Checks, in order: a pinned Proton build (a real directory holding a
// `proton` script, never a floating alias), umu-run, a Vulkan driver, and
// free space. The first missing piece wins; each message names the package
// that provides it.
[[nodiscard]] PreflightOutcome runInstallPreflight(const PreflightRequest &request,
                                                   const SystemProbe &probe);

// "2.0 GB"-style size for plain messages.
[[nodiscard]] QString plainSize(qint64 bytes);

} // namespace QindaQt::QindaLutris
