// SPDX-License-Identifier: GPL-3.0-or-later
#include "android_native_window_peer.h"
#include <waylandwindow.h>
#include <window.h>
#include <QUuid>
namespace QindaQt::Compositor::KWinIntegration {
namespace {
class SplitRendererSource final:public AndroidRendererEvidenceSource {
public:
    explicit SplitRendererSource(const Services::AndroidRuntime::RuntimeRendererBinding &binding):m_binding(binding) {}
    std::optional<AndroidPlatformOrigin> observe(const NativeWindowPeer &peer) const override {
        const auto value=m_binding.observe();
        if (!value || value->processId!=peer.kernelProcessId) return std::nullopt;
        return AndroidPlatformOrigin{peer.windowId,value->runtimeAttachmentId,value->rendererIncarnation,value->processId};
    }
private:
    const Services::AndroidRuntime::RuntimeRendererBinding &m_binding;
};
}
std::optional<NativeWindowPeer> androidNativeWindowPeer(const KWin::Window *window) {
    const auto *native=qobject_cast<const KWin::WaylandWindow *>(window);
    if (!native || native->isDeleted() || native->isInternal() || !native->isClient()) return std::nullopt;
    const auto id=native->internalId();
    // AGENT-CONTRACT: WaylandWindow::pid comes from wl_client kernel credentials.
    // Never generalize this to X11/client PID properties or title/app_id matching.
    const auto pid=native->pid();
    if (id.isNull() || pid<=1) return std::nullopt;
    return NativeWindowPeer{id.toString(QUuid::WithoutBraces),true,pid};
}
std::optional<AndroidPlatformOrigin> androidNativePlatformOrigin(
    const KWin::Window *window,const Services::AndroidRuntime::RuntimeRendererBinding &binding) {
    const auto peer=androidNativeWindowPeer(window);
    if (!peer) return std::nullopt;
    const SplitRendererSource source(binding);
    return androidPlatformOrigin(*peer,source);
}
} // namespace QindaQt::Compositor::KWinIntegration
