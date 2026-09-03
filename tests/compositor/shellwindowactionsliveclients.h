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

int runShellWindowActionsLiveWindow(QGuiApplication &application,
                                    const QString &title);
int runUnauthorizedIdentityClient();

[[nodiscard]] std::optional<quint64> parseLiveClientNativeWindowId(
    const QByteArray &output, const QString &expectedTitle);
[[nodiscard]] bool isFailClosedUnauthorizedIdentityReply(
    const QByteArray &output);

} // namespace QindaQt::Compositor::TestSupport
