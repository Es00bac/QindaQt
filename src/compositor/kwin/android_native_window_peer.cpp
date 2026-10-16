// SPDX-License-Identifier: GPL-3.0-or-later
#include "android_native_window_peer.h"
#include <wayland/clientconnection.h>
#include <wayland/surface.h>
#include <wayland_server.h>
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
    if (!window || window->isDeleted() || window->isInternal() || !window->isClient()) return std::nullopt;
    // AGENT-GUARD: never qobject_cast or dynamic_cast to KWin::WaylandWindow.
    // The fork does not export that class's metaobject or typeinfo, so the
    // reference left this whole plugin unloadable ("undefined symbol
    // KWin::WaylandWindow::staticMetaObject"). A native Wayland client window
    // is one whose surface belongs to a client connection other than
    // Xwayland's; X11 windows' surfaces belong to the Xwayland connection.
    const auto *surface=window->surface();
    auto *const client=surface ? surface->client() : nullptr;
    auto *const server=KWin::waylandServer();
    if (!client || !server || client==server->xWaylandConnection()) return std::nullopt;
    const auto id=window->internalId();
    // AGENT-CONTRACT: the wl_client's kernel credentials, the same source
    // WaylandWindow::pid() reads. Never generalize this to X11/client PID
    // properties or title/app_id matching.
    const auto pid=client->processId();
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
