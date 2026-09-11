// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusMessage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QVariant>

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
// Stages a runtime whose portal directory contains only fake `kde` and
// `gnome-keyring` backend declarations plus the real routing file, so the
// real frontend resolves every family against test-owned backends. Dropping
// the Secret row exercises the closed-default mutation control.
std::optional<Runtime> stageRoutingRuntime(bool withSecretRow, QString *error);
bool waitForService(const QString &name);
bool waitForServiceGone(const QString &name);
bool startCore(Runtime &runtime, ChildProcesses &children,
               QProcess **frontend, QString *error);
bool commitColorScheme(const QString &scheme, QString *error);

QDBusMessage frontendPortalCall(const QString &interfaceName,
                                const QString &member,
                                const QVariantList &arguments = {},
                                int timeoutMilliseconds = 5'000);
bool interfaceExported(const QString &interfaceName);
bool verifyFrontendAppearance(QString *error);
QVariant unwrapVariant(QVariant value);

} // namespace QindaQt::Tests::Portal
