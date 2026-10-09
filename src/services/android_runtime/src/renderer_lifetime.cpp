// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/android_runtime/renderer_lifetime.h"
#include "process_lifetime_p.h"
#include <QUuid>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Services::AndroidRuntime {
namespace {
bool canonical(const QString &value) {
    if (value.size()!=36) return false;
    const QUuid uuid=QUuid::fromString(value);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces)==value;
}
bool closeOnExec(int fd) {
    const int flags=::fcntl(fd,F_GETFD);
    return flags>=0 && ::fcntl(fd,F_SETFD,flags|FD_CLOEXEC)==0;
}
}
class HeldRendererLifetime::Private {
public:
    int lifetime=-1;
    int executable=-1;
    int pid=0;
    struct stat pin{};
    bool valid=false;
    Private(int lifetimeFd,int executableFd,int observedPid)
        :lifetime(lifetimeFd),executable(executableFd),pid(observedPid) {
        if (lifetime==executable) { executable=-1; return; }
        valid=pid>1 && closeOnExec(lifetime) && closeOnExec(executable)
            && ProcessLifetime::pidfdMatches(lifetime,pid) && ProcessLifetime::pinExecutable(executable,pin)
            && ProcessLifetime::live(lifetime,pid,pin);
    }
    ~Private() { if (lifetime>=0) ::close(lifetime); if (executable>=0) ::close(executable); }
};
HeldRendererLifetime::HeldRendererLifetime(int lifetimeFd,int executableFd,int pid)
    :d(std::make_unique<Private>(lifetimeFd,executableFd,pid)) {}
HeldRendererLifetime::~HeldRendererLifetime()=default;
std::optional<qint64> HeldRendererLifetime::processId() const {
    if (!d->valid) return std::nullopt;
    // Once an observed lifetime/object is lost, this incarnation cannot rebind.
    if (!ProcessLifetime::live(d->lifetime,d->pid,d->pin)) { d->valid=false; return std::nullopt; }
    return d->pid;
}
RuntimeRendererBinding::RuntimeRendererBinding(const DescriptorRuntimePort &collector,const HeldRendererLifetime &renderer,
                                              QString runtime,QString incarnation)
    :m_collector(collector),m_renderer(renderer),m_runtime(std::move(runtime)),m_incarnation(std::move(incarnation)) {}
std::optional<RuntimeRendererObservation> RuntimeRendererBinding::observe() const {
    if (!canonical(m_runtime) || !canonical(m_incarnation)) return std::nullopt;
    const auto collector=m_collector.authenticatedPeer(); const auto renderer=m_renderer.processId();
    if (!collector || !renderer) return std::nullopt;
    const auto status=m_collector.snapshot();
    if (status.attachmentId!=m_runtime || (status.state!=RuntimeState::Ready && status.state!=RuntimeState::Starting))
        return std::nullopt;
    const auto currentCollector=m_collector.authenticatedPeer(); const auto currentRenderer=m_renderer.processId();
    if (!currentCollector || !currentRenderer || currentCollector->processId!=collector->processId
        || currentCollector->userId!=collector->userId || currentRenderer!=renderer) return std::nullopt;
    return RuntimeRendererObservation{m_runtime,m_incarnation,*renderer};
}
} // namespace QindaQt::Services::AndroidRuntime
