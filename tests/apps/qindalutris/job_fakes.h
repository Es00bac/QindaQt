// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Scripted seams for the QindaLutris install-job rows (ADR-0275). Nothing
// here touches the network, a real installer, umu or Wine: downloads are
// bytes written by the fake, installers are side-effect lambdas, and host
// facts are plain fields.

#include "downloader.h"
#include "install_preflight.h"
#include "installer_planning.h"
#include "process_runner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTimer>

#include <functional>

namespace QindaQt::QindaLutris::TestSupport {

// Mirrors NetworkDownloader's contract (downloader.h). Failure reasons come
// from the real DownloadGuard, so the fake cannot drift from production
// wording or decisions.
class FakeDownloader final : public Downloader {
public:
  enum class Mode { Serve, HttpError, Truncated, BadRedirect, Hang };
  struct Script {
    Mode mode = Mode::HttpError;
    QByteArray body;
  };

  QHash<QString, Script> scripts; // keyed by QUrl::toString()
  Script fallback;
  QList<QUrl> requested;
  int cancels = 0;

  void serve(const QUrl &url, const QByteArray &body) { scripts.insert(url.toString(), {Mode::Serve, body}); }
  void script(const QUrl &url, Mode mode) { scripts.insert(url.toString(), {mode, {}}); }

  void start(const QUrl &url, const QString &destinationFile) override {
    requested.append(url);
    const quint64 generation = ++m_generation;
    const Script scripted = scripts.value(url.toString(), fallback);
    if (scripted.mode == Mode::Hang) {
      return;
    }
    QTimer::singleShot(0, this, [this, generation, url, destinationFile, scripted] {
      if (generation != m_generation) {
        return;
      }
      DownloadGuard guard;
      if (const auto refused = guard.checkStart(url)) {
        Q_EMIT finished(false, *refused);
        return;
      }
      switch (scripted.mode) {
      case Mode::Serve: {
        QFile file(destinationFile);
        if (!file.open(QIODevice::WriteOnly) || file.write(scripted.body) != scripted.body.size()) {
          Q_EMIT finished(false, QStringLiteral("fake write failed"));
          return;
        }
        file.close();
        Q_EMIT progress(scripted.body.size(), scripted.body.size());
        Q_EMIT finished(true, {});
        return;
      }
      case Mode::HttpError:
        Q_EMIT finished(false, *guard.checkHeaders(404, -1));
        return;
      case Mode::Truncated: {
        (void)guard.checkHeaders(200, 1000);
        Q_EMIT progress(400, 1000);
        Q_EMIT finished(false, *guard.checkCompletion(400));
        return;
      }
      case Mode::BadRedirect:
        Q_EMIT finished(false,
                        *guard.checkRedirect(QUrl(QStringLiteral("https://downloads.evil.example/x"))));
        return;
      case Mode::Hang:
        return;
      }
    });
  }

  void cancel() override {
    ++m_generation;
    ++cancels;
  }

private:
  quint64 m_generation = 0;
};

// A runner whose "program" is a lambda run from the event loop.
class FakeRunner final : public ProcessRunner {
public:
  QList<ProcessRunSpec> specs;
  ProcessRunResult result = [] {
    ProcessRunResult ok;
    ok.started = true;
    ok.exitCode = 0;
    ok.treeStopped = true; // as a scope-backed production run reports
    ok.trackedStopped = true;
    return ok;
  }();
  std::function<void(const ProcessRunSpec &)> sideEffect;
  bool hang = false;
  int cancels = 0;

  void start(const ProcessRunSpec &spec) override {
    specs.append(spec);
    const quint64 generation = ++m_generation;
    if (hang) {
      return;
    }
    QTimer::singleShot(0, this, [this, generation, spec] {
      if (generation != m_generation) {
        return;
      }
      if (sideEffect) {
        sideEffect(spec);
      }
      Q_EMIT finished(result);
    });
  }

  // Mirrors the production contract: cancel() stops the "tree" and then
  // reports finished(cancelled) from the event loop.
  bool treeStopsOnCancel = true;
  void cancel() override {
    ++cancels;
    const quint64 generation = ++m_generation;
    QTimer::singleShot(0, this, [this, generation] {
      if (generation != m_generation) {
        return;
      }
      ProcessRunResult stopped;
      stopped.started = true;
      stopped.cancelled = true;
      stopped.treeStopped = treeStopsOnCancel;
      stopped.trackedStopped = treeStopsOnCancel;
      stopped.stopDetail = QStringLiteral("fake tree stop");
      Q_EMIT finished(stopped);
    });
  }

private:
  quint64 m_generation = 0;
};

class FakeProbe final : public SystemProbe {
public:
  std::optional<qint64> freeBytes = qint64(500) * 1024 * 1024 * 1024;
  QString umu = QStringLiteral("/usr/bin/umu-run");
  bool vulkan = true;

  std::optional<qint64> availableBytes(const QString &) const override { return freeBytes; }
  QString umuRunBinary() const override { return umu; }
  bool hasVulkanDriver() const override { return vulkan; }
};

// Records every planner request and answers with an argv plan in the shape
// the real umu plan builder produces.
struct RecordingPlanner {
  QList<InstallerPlanRequest> requests;
  bool refuse = false;

  InstallerPlanner planner() {
    return [this](const InstallerPlanRequest &request) {
      requests.append(request);
      InstallerPlan plan;
      if (refuse) {
        plan.reason = QStringLiteral("The pinned Proton build is a floating alias.");
        return plan;
      }
      plan.ok = true;
      plan.spec.program = request.umuRunBinary;
      plan.spec.arguments = request.windowsCommand;
      plan.spec.environment = {{QStringLiteral("WINEPREFIX"), request.prefixPath},
                               {QStringLiteral("PROTONPATH"), request.protonBuildPath},
                               {QStringLiteral("GAMEID"), request.umuId},
                               {QStringLiteral("STORE"), request.umuStore},
                               {QStringLiteral("UMU_RUNTIME_UPDATE"), QStringLiteral("0")}};
      return plan;
    };
  }
};

// Creates a fake pinned build (a directory holding a `proton` script).
inline QString makeProtonBuild(const QString &root, const QString &name) {
  const QString path = root + QLatin1Char('/') + name;
  QDir().mkpath(path);
  QFile script(path + QStringLiteral("/proton"));
  if (script.open(QIODevice::WriteOnly)) {
    script.write("#!/bin/sh\n");
  }
  return path;
}

inline void writeFile(const QString &path, qint64 bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QByteArray(static_cast<qsizetype>(bytes), 'x'));
  }
}

} // namespace QindaQt::QindaLutris::TestSupport
