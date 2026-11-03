// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>

#include <optional>

// Typed JSON bodies of QindaLink mirror protocol 1. Every builder returns a complete encoded
// frame ready for the socket; every parser takes a frame payload.
//
// AGENT-CONTRACT: field names and value ranges are defined in
// docs/wiki/reference/qindalink-mirror-v1.md and read by phone/mirror_server/.../Session.java.
// Privacy: Text bodies carry what the user typed; callers never log a built frame.
namespace qindaqt::link_mirror {

struct StartRequest {
    QString packageName;
    QSize size;
    int dpi = 160;
    QString codec = QStringLiteral("h264");
    int bitrateBps = 8'000'000;
    int maxFps = 60;
};

enum class TouchAction { Down, Move, Up, Cancel };

[[nodiscard]] QByteArray helloFrame(const QString &token);
[[nodiscard]] QByteArray startFrame(const StartRequest &request);
[[nodiscard]] QByteArray resizeFrame(QSize size, int dpi);
[[nodiscard]] QByteArray touchFrame(TouchAction action, int pointerId, QPointF position, double pressure);
[[nodiscard]] QByteArray scrollFrame(QPointF position, double horizontal, double vertical);
[[nodiscard]] QByteArray keyFrame(bool down, int keycode, int metaState, int repeat);
[[nodiscard]] QByteArray textFrame(const QString &text);
[[nodiscard]] QByteArray syncFrame();
[[nodiscard]] QByteArray stopFrame();
// Keepalive; the server ends a greeted session after 30 s without any frame.
[[nodiscard]] QByteArray pingFrame();
[[nodiscard]] QByteArray inventoryListFrame(const QStringList &packages, int iconSize);

struct ReadyInfo {
    int version = 0;
    int sdk = 0;
    QString release;
    QString model;
    QStringList missing;
};

struct DisplayInfo {
    int displayId = -1;
    QSize size;
    int dpi = 0;
    int flags = 0;
};

struct ServerError {
    QString code;
    QString message;
    bool fatal = true;
};

struct VideoFormat {
    QString codec;
    QSize size;
};

struct AppEntry {
    QString packageName;
    QString activity;
    QString label;
    QImage icon; // null when the server sent no icon or it failed to decode
};

[[nodiscard]] std::optional<ReadyInfo> parseReady(const QByteArray &payload);
[[nodiscard]] std::optional<DisplayInfo> parseDisplay(const QByteArray &payload);
[[nodiscard]] std::optional<ServerError> parseError(const QByteArray &payload);
[[nodiscard]] std::optional<VideoFormat> parseVideoFormat(const QByteArray &payload);
[[nodiscard]] std::optional<AppEntry> parseApp(const QByteArray &payload);
[[nodiscard]] std::optional<int> parseInventoryEnd(const QByteArray &payload);
// app-exited: the reason string ("no-tasks"), or nullopt for a malformed body.
[[nodiscard]] std::optional<QString> parseAppExited(const QByteArray &payload);

} // namespace qindaqt::link_mirror
