// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/android_renderer_evidence.h"
#include <QUuid>
namespace QindaQt::Compositor {
namespace {
bool canonical(const QString &value) {
    if (value.size()!=36) return false;
    const QUuid uuid=QUuid::fromString(value);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces)==value;
}
bool eligible(const NativeWindowPeer &peer) {
    return peer.nativeWayland && peer.kernelProcessId>1 && canonical(peer.windowId);
}
}
AdmittedRendererDescriptor::AdmittedRendererDescriptor(
    const Services::AndroidRuntime::DescriptorRuntimePort &renderer,QString runtime,QString incarnation)
    : m_renderer(renderer),m_runtime(std::move(runtime)),m_incarnation(std::move(incarnation)) {}
std::optional<AndroidPlatformOrigin> AdmittedRendererDescriptor::observe(const NativeWindowPeer &peer) const
{
    if (!eligible(peer) || !canonical(m_runtime) || !canonical(m_incarnation)) return std::nullopt;
    const auto process=m_renderer.authenticatedPeer();
    if (!process || process->processId!=peer.kernelProcessId) return std::nullopt;
    const auto snapshot=m_renderer.snapshot();
    using Services::AndroidRuntime::RuntimeState;
    if (snapshot.attachmentId!=m_runtime
        || (snapshot.state!=RuntimeState::Ready && snapshot.state!=RuntimeState::Starting)) return std::nullopt;
    // Recheck after copied getters; a queued death event is not the fence.
    const auto current=m_renderer.authenticatedPeer();
    if (!current || current->processId!=process->processId || current->userId!=process->userId) return std::nullopt;
    return AndroidPlatformOrigin{peer.windowId,m_runtime,m_incarnation,peer.kernelProcessId};
}
std::optional<AndroidPlatformOrigin> androidPlatformOrigin(
    const NativeWindowPeer &peer,const AndroidRendererEvidenceSource &source)
{
    if (!eligible(peer)) return std::nullopt;
    const auto candidate=source.observe(peer);
    if (!candidate || candidate->windowId!=peer.windowId || candidate->kernelProcessId!=peer.kernelProcessId
        || !canonical(candidate->runtimeAttachmentId) || !canonical(candidate->rendererIncarnation)) return std::nullopt;
    return candidate;
}
} // namespace QindaQt::Compositor
