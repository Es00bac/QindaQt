// SPDX-License-Identifier: GPL-3.0-or-later
#include "collector_process_p.h"
#include "collector_recipe_p.h"
#include "qindaqt/services/android_runtime/descriptors.h"
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <unistd.h>
namespace QindaQt::Services::AndroidRuntime {
CollectorProcess::CollectorProcess(std::function<bool()> contextLive):m_live(std::move(contextLive)) {}
CollectorProcess::~CollectorProcess() {
    m_port.reset(); // private close makes the owned helper exit without replay
    if (m_process.state()!=QProcess::NotRunning && !m_process.waitForFinished(200)) {
        m_process.terminate(); if (!m_process.waitForFinished(200)) { m_process.kill(); m_process.waitForFinished(200); }
    }
    if (m_executable>=0) ::close(m_executable);
}
bool CollectorProcess::start(int executableFd) {
    if (m_executable>=0) { if (executableFd>=0 && executableFd!=m_executable) ::close(executableFd); return false; }
    m_executable=executableFd;
    if (m_executable<0 || !m_live || !m_live()) return false;
    QByteArray key(32,'\0');
    if (::getrandom(key.data(),32,0)!=32) return false;
    auto pair=createPrivateProducerDescriptors(); if (pair[0]<0) return false;
    const int parentPid=static_cast<int>(::getpid());
    const int lifetime=holdProducerLifetime(parentPid);
    const int parentCode=::open("/proc/self/exe",O_PATH|O_CLOEXEC);
    const int record=::memfd_create("qindaqt.android.collector",MFD_CLOEXEC|MFD_ALLOW_SEALING);
    const auto runtime=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"parentPid",parentPid},
        {"key",QString::fromLatin1(key.toHex())},{"runtime",runtime},
        {"recipe",QString::fromLatin1(collectorRecipeDigest())}}).toJson(QJsonDocument::Compact);
    const auto cleanup=[&]{
        ::close(pair[0]); ::close(pair[1]);
        if (lifetime>=0) ::close(lifetime);
        if (parentCode>=0) ::close(parentCode);
        if (record>=0) ::close(record);
    };
    if (lifetime<0 || parentCode<0 || record<0 || ::write(record,bytes.constData(),static_cast<size_t>(bytes.size()))!=bytes.size()
        || ::lseek(record,0,SEEK_SET)<0 || ::fcntl(record,F_ADD_SEALS,F_SEAL_SEAL|F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_WRITE)<0) {
        cleanup(); return false;
    }
    // Duplicate above the inherited fixed slots before the async-safe modifier.
    const int socketCopy=::fcntl(pair[1],F_DUPFD_CLOEXEC,20), recordCopy=::fcntl(record,F_DUPFD_CLOEXEC,20);
    const int codeCopy=::fcntl(m_executable,F_DUPFD_CLOEXEC,20),lifeCopy=::fcntl(lifetime,F_DUPFD_CLOEXEC,20);
    const int parentCopy=::fcntl(parentCode,F_DUPFD_CLOEXEC,20);
    if (socketCopy<0 || recordCopy<0 || codeCopy<0 || lifeCopy<0 || parentCopy<0) {
        for (int fd:{socketCopy,recordCopy,codeCopy,lifeCopy,parentCopy}) if (fd>=0) ::close(fd);
        cleanup(); return false;
    }
    m_process.setProgram(QStringLiteral("/proc/self/fd/5"));
    QProcessEnvironment environment; environment.insert(QStringLiteral("PATH"),QStringLiteral("/usr/bin:/bin"));
    environment.insert(QStringLiteral("LANG"),QStringLiteral("C.UTF-8"));
    m_process.setProcessEnvironment(environment);
    m_process.setStandardOutputFile(QProcess::nullDevice()); m_process.setStandardErrorFile(QProcess::nullDevice());
    m_process.setChildProcessModifier([=] {
        if (::prctl(PR_SET_PDEATHSIG,SIGTERM)!=0 || ::getppid()!=parentPid
            || ::dup2(socketCopy,collectorSocket)<0 || ::dup2(recordCopy,collectorRecord)<0
            || ::dup2(codeCopy,collectorExecutable)<0 || ::dup2(lifeCopy,parentLifetime)<0
            || ::dup2(parentCopy,parentExecutable)<0) ::_exit(126);
        ::close(socketCopy); ::close(recordCopy); ::close(codeCopy); ::close(lifeCopy); ::close(parentCopy);
    });
    m_process.start();
    const bool started=m_process.waitForStarted(500);
    for (int fd:{socketCopy,recordCopy,codeCopy,lifeCopy,parentCopy}) ::close(fd);
    ::close(pair[1]); ::close(lifetime); ::close(parentCode); ::close(record);
    if (!started || !m_live()) { ::close(pair[0]); return false; }
    const auto pid=m_process.processId();
    m_port=std::make_unique<DescriptorRuntimePort>(pair[0],holdProducerLifetime(static_cast<int>(pid)),
        ::fcntl(m_executable,F_DUPFD_CLOEXEC,20),static_cast<int>(pid),::getuid(),key);
    QObject::connect(m_port.get(),&RuntimePort::snapshotChanged,this,[this]{ Q_EMIT snapshotChanged(); });
    QObject::connect(m_port.get(),&RuntimePort::launchFinished,this,[this](const LaunchResult &value){
        if (admitted()) Q_EMIT launchFinished(value);
    });
    QEventLoop admission; QTimer deadline; deadline.setSingleShot(true);
    QObject::connect(m_port.get(),&RuntimePort::snapshotChanged,&admission,&QEventLoop::quit);
    QObject::connect(&deadline,&QTimer::timeout,&admission,&QEventLoop::quit);
    deadline.start(500); if (!m_port->authenticatedPeer()) admission.exec();
    if (!admitted()) { m_port.reset(); return false; }
    m_guardTimer.setInterval(500);
    QObject::connect(&m_guardTimer,&QTimer::timeout,this,[this]{
        if (!admitted()) { m_guardTimer.stop(); m_port.reset(); Q_EMIT snapshotChanged(); }
    });
    m_guardTimer.start();
    return true;
}
bool CollectorProcess::admitted() const { return m_live && m_live() && m_port && m_port->authenticatedPeer().has_value(); }
RuntimeSnapshot CollectorProcess::snapshot() const { return admitted()?m_port->snapshot():RuntimeSnapshot{}; }
void CollectorProcess::requestRefresh() { if (admitted()) m_port->requestRefresh(); else Q_EMIT snapshotChanged(); }
LaunchSubmission CollectorProcess::requestLaunch(const LaunchRequest &request) {
    if (!admitted()) return {false,QStringLiteral("Android session or helper admission was lost")};
    return m_port->requestLaunch(request);
}
} // namespace QindaQt::Services::AndroidRuntime
