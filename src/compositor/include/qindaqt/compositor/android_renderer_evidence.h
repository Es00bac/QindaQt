// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/descriptor_runtime_port.h"
#include <QString>
#include <optional>
namespace QindaQt::Compositor {

// Fresh compositor-managed native wl_client facts, never client PID properties.
// The caller must prove the window remains live; values are not cache authority.
struct NativeWindowPeer {
    QString windowId;
    bool nativeWayland=false;
    qint64 kernelProcessId=0;
};
struct AndroidPlatformOrigin {
    QString windowId;
    QString runtimeAttachmentId;
    QString rendererIncarnation;
    qint64 kernelProcessId=0;
    // Deliberately no AppKey/package/task identity or force-stop permission.
};

// AGENT-CONTRACT: compositor kernel facts meet admitted bootstrap renderer role,
// never guest package/task identity (ADR-0373). Read fresh for every window. The
// provider must withdraw on process, channel, runtime or renderer replacement.
class AndroidRendererEvidenceSource {
public:
    virtual ~AndroidRendererEvidenceSource()=default;
    virtual std::optional<AndroidPlatformOrigin> observe(const NativeWindowPeer &peer) const=0;
};

// Narrow in-process renderer case: trusted bootstrap has qualified this exact
// native producer as the Wayland renderer for this runtime. A separate broker/
// collector must NEVER be passed as a renderer just because it supplied app
// metadata. Borrows a GUI-thread descriptor port; it must outlive this object.
// No title/app_id/argv/public bus-name matching or automatic origin assignment.
// Calls stay on the port thread; refusals return unknown without a side effect.
// This additive static C++ boundary requires consumer rebuilds, no wire ABI change.
class AdmittedRendererDescriptor final : public AndroidRendererEvidenceSource {
public:
    AdmittedRendererDescriptor(const Services::AndroidRuntime::DescriptorRuntimePort &renderer,
                               QString runtimeAttachmentId,QString rendererIncarnation);
    std::optional<AndroidPlatformOrigin> observe(const NativeWindowPeer &peer) const override;
private:
    const Services::AndroidRuntime::DescriptorRuntimePort &m_renderer;
    QString m_runtime;
    QString m_incarnation;
};

// General consumer gate also validates the provider's exact returned binding.
// Empty means unknown origin, not Native and not a guest identity inference.
[[nodiscard]] std::optional<AndroidPlatformOrigin> androidPlatformOrigin(
    const NativeWindowPeer &peer,const AndroidRendererEvidenceSource &source);
} // namespace QindaQt::Compositor
