// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace QindaQt::Tests::Portal {

inline constexpr auto FrontendService = "org.freedesktop.portal.Desktop";
inline constexpr auto FrontendInterface = "org.freedesktop.portal.Settings";
inline constexpr auto FallbackService =
    "org.freedesktop.impl.portal.desktop.kde";
inline constexpr auto DocumentsService = "org.freedesktop.portal.Documents";
inline constexpr auto PermissionStoreService =
    "org.freedesktop.impl.portal.PermissionStore";

QString configuredPath(const char *environmentName, const char *fallback);
bool waitUntil(const std::function<bool()> &condition, int timeoutMilliseconds);

class ChildProcesses final {
public:
    ~ChildProcesses();

    QProcess *start(const QString &program, const QStringList &arguments,
                    const QProcessEnvironment &environment, QString *error);
    static void stop(QProcess &process);

private:
    std::vector<std::unique_ptr<QProcess>> m_processes;
};

struct Runtime final {
    std::unique_ptr<QTemporaryDir> root;
    QString portalDirectory;
    QProcessEnvironment environment;
};

std::optional<Runtime> stageRuntime(QString *error);
bool waitForService(const QString &name);
bool startCore(Runtime &runtime, ChildProcesses &children,
               QProcess **frontend, QString *error);
bool commitColorScheme(const QString &scheme, QString *error);

} // namespace QindaQt::Tests::Portal
