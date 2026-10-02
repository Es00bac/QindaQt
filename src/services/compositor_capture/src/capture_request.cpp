// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/compositor_capture/capture_request.h>

namespace QindaQt::CompositorCapture {
namespace {

// KWin's CaptureInteractive kinds (screenshotdbusinterface2.cpp).
constexpr uint InteractiveWindow = 0;
constexpr int InteractiveTimeoutMilliseconds = 120000;

} // namespace

QList<int> allowedDelays()
{
    return {0, 3, 5, 10};
}

bool isWindowMode(CaptureMode mode)
{
    return mode == CaptureMode::ActiveWindow || mode == CaptureMode::WindowUnderPointer;
}

QString captureModeId(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Region:
        return QStringLiteral("region");
    case CaptureMode::AllScreens:
        return QStringLiteral("all-screens");
    case CaptureMode::CurrentScreen:
        return QStringLiteral("current-screen");
    case CaptureMode::ActiveWindow:
        return QStringLiteral("active-window");
    case CaptureMode::WindowUnderPointer:
        return QStringLiteral("window-under-pointer");
    }
    return {};
}

std::optional<CaptureMode> captureModeFromId(QStringView id)
{
    for (const auto mode : {CaptureMode::Region, CaptureMode::AllScreens, CaptureMode::CurrentScreen,
                            CaptureMode::ActiveWindow, CaptureMode::WindowUnderPointer}) {
        if (id == captureModeId(mode))
            return mode;
    }
    return std::nullopt;
}

KWinCaptureCall kwinCallFor(const CaptureOptions &options)
{
    KWinCaptureCall call;
    // AGENT-NOTE: native-resolution keeps every physical pixel on scaled
    // outputs; "hide-caller-windows" (KWin's default) keeps this tool's own
    // windows out of every capture. Both are stated so a KWin default change
    // cannot silently alter what is saved.
    call.options = {
        {QStringLiteral("native-resolution"), true},
        {QStringLiteral("include-cursor"), options.includePointer},
        {QStringLiteral("hide-caller-windows"), true},
    };
    if (isWindowMode(options.mode)) {
        call.options.insert(QStringLiteral("include-decoration"), options.includeDecorations);
        call.options.insert(QStringLiteral("include-shadow"), false);
    }
    switch (options.mode) {
    case CaptureMode::Region:
    case CaptureMode::AllScreens:
        call.method = QStringLiteral("CaptureWorkspace");
        break;
    case CaptureMode::CurrentScreen:
        call.method = QStringLiteral("CaptureActiveScreen");
        break;
    case CaptureMode::ActiveWindow:
        call.method = QStringLiteral("CaptureActiveWindow");
        break;
    case CaptureMode::WindowUnderPointer:
        call.method = QStringLiteral("CaptureInteractive");
        call.leadingArguments = {QVariant::fromValue(InteractiveWindow)};
        call.timeoutMilliseconds = InteractiveTimeoutMilliseconds;
        break;
    }
    return call;
}

} // namespace QindaQt::CompositorCapture
