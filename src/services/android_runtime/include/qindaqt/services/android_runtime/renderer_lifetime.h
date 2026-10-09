// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/descriptor_runtime_port.h"
#include <memory>
namespace QindaQt::Services::AndroidRuntime {
// AGENT-CONTRACT: trusted bootstrap transfers two distinct owned descriptors
// held from observed renderer birth: pidfd and already admitted executable.
// PID/file descriptors must never originate from a public message/argv mode.
// This checks the provisioned process, not installed code, renderer role or
// guest package/task/surface identity. Serialize calls/destruction on owner thread.
// No process is stopped; getter refusal is immediate, with no event-loop dependency.
class HeldRendererLifetime final {
public:
    HeldRendererLifetime(int lifetimeFd,int executableFd,int observedPid);
    ~HeldRendererLifetime();
    HeldRendererLifetime(const HeldRendererLifetime &)=delete;
    HeldRendererLifetime &operator=(const HeldRendererLifetime &)=delete;
    [[nodiscard]] std::optional<qint64> processId() const;
private:
    class Private;
    std::unique_ptr<Private> d;
};
struct RuntimeRendererObservation {
    QString runtimeAttachmentId;
    QString rendererIncarnation;
    qint64 processId=0;
    // No guest AppKey/UID/task/surface or application-control authority.
};
// Borrowed collector and held renderer must both outlive this observer. Calls
// stay on the collector's Qt thread. Bootstrap qualified this exact renderer for
// the expected runtime, generated a non-reused incarnation, and supplies fresh
// attachment context; mere matching app inventory never admits renderer role.
// Additive static C++ API; consumer rebuild required, transport v1 unchanged.
class RuntimeRendererBinding final {
public:
    RuntimeRendererBinding(const DescriptorRuntimePort &collector,const HeldRendererLifetime &renderer,
                           QString runtimeAttachmentId,QString rendererIncarnation);
    [[nodiscard]] std::optional<RuntimeRendererObservation> observe() const;
private:
    const DescriptorRuntimePort &m_collector;
    const HeldRendererLifetime &m_renderer;
    QString m_runtime;
    QString m_incarnation;
};
} // namespace QindaQt::Services::AndroidRuntime
