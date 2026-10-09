// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/compositor/android_renderer_evidence.h"
#include "qindaqt/services/android_runtime/renderer_lifetime.h"
namespace KWin { class Window; }
namespace QindaQt::Compositor::KWinIntegration {
// Borrowed live managed KWin window on the compositor thread; no pointer is
// retained. Native Wayland kernel credentials only, never client PID metadata.
// Refusal is unknown; this does not publish registry facts or guest app identity.
[[nodiscard]] std::optional<NativeWindowPeer> androidNativeWindowPeer(const KWin::Window *window);
[[nodiscard]] std::optional<AndroidPlatformOrigin> androidNativePlatformOrigin(
    const KWin::Window *window,const Services::AndroidRuntime::RuntimeRendererBinding &binding);
} // namespace QindaQt::Compositor::KWinIntegration
