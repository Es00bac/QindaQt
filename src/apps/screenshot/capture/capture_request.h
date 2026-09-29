// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringView>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Screenshot {

// What the user asked to capture.
//
// AGENT-CONTRACT: the string ids are shared by the command line, the QML
// mode picker and the desktop-controls launch arguments (ADR-0289). Renaming
// one silently breaks a global shortcut.
enum class CaptureMode {
    Region,
    AllScreens,
    CurrentScreen,
    ActiveWindow,
    WindowUnderPointer,
};

struct CaptureOptions {
    CaptureMode mode = CaptureMode::Region;
    // Seconds to wait before capturing. The window offers allowedDelays();
    // the command line accepts 0..kMaxDelaySeconds for scripts.
    int delaySeconds = 0;
    bool includePointer = false;
    // Window modes only; ignored for screen and region captures.
    bool includeDecorations = true;

    friend bool operator==(const CaptureOptions &, const CaptureOptions &) = default;
};

inline constexpr int kMaxDelaySeconds = 60;

// One org.kde.KWin.ScreenShot2 method call without its trailing pipe
// descriptor, which the capture port appends and owns.
struct KWinCaptureCall {
    QString method;
    // Arguments that precede the options map (CaptureInteractive's kind).
    QVariantList leadingArguments;
    QVariantMap options;
    // Interactive picking waits on the user, not on KWin.
    int timeoutMilliseconds = 15000;

    friend bool operator==(const KWinCaptureCall &, const KWinCaptureCall &) = default;
};

[[nodiscard]] QList<int> allowedDelays();
[[nodiscard]] bool isWindowMode(CaptureMode mode);
[[nodiscard]] QString captureModeId(CaptureMode mode);
[[nodiscard]] std::optional<CaptureMode> captureModeFromId(QStringView id);

// The single place capture options become a KWin request.
//
// AGENT-GUARD: region captures deliberately request the whole workspace at
// native resolution. The selection overlay shows that frozen image and the
// crop happens locally, so what the user framed is exactly what is saved and
// the overlay itself can never appear in the result.
[[nodiscard]] KWinCaptureCall kwinCallFor(const CaptureOptions &options);

} // namespace QindaQt::Screenshot
