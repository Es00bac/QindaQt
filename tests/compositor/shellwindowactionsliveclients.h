// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <optional>

class QGuiApplication;

namespace QindaQt::Compositor::TestSupport {

inline constexpr char ShellWindowActionsLiveClientMarker[] =
    "QINDAQT_WINDOW_CLIENT=";
inline constexpr char ShellWindowActionsLiveAppMenuService[] =
    "org.qindaqt.LiveIdentityMenu";
inline constexpr char ShellWindowActionsLiveAppMenuPath[] =
    "/org/qindaqt/LiveIdentityMenu";

int runShellWindowActionsLiveWindow(QGuiApplication &application,
                                    const QString &title,
                                    bool announceAppMenu);
int runUnauthorizedIdentityClient();

[[nodiscard]] bool proveUnauthorizedShellCalls(
    const QString &executable, const QString &windowId, const QString &epoch,
    quint64 revision, QString *failure);

[[nodiscard]] std::optional<quint64> parseLiveClientNativeWindowId(
    const QByteArray &output, const QString &expectedTitle);
[[nodiscard]] bool isFailClosedUnauthorizedIdentityReply(
    const QByteArray &output);

} // namespace QindaQt::Compositor::TestSupport
