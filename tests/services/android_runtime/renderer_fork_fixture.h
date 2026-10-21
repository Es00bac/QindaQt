// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "runtime_fixtures.h"
#include "qindaqt/services/android_runtime/descriptors.h"
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
namespace RendererForkFixture {
using namespace RuntimeFixtures;
const QString incarnation=QStringLiteral("c1234567-1234-4234-8234-123456789abc");
// Only owns an inert fixture child and private control descriptor; no guest,
// ambient process, package helper or installed Android executable is touched.
struct Child {
    pid_t pid=-1;
    int control=-1;
    ~Child() { if (control>=0) ::close(control); retire(); }
    void retire() {
        if (pid<=0) return;
        int status=0;
        if (::waitpid(pid,&status,WNOHANG)==0) {
            ::kill(pid,SIGTERM); static_cast<void>(::waitpid(pid,&status,0));
        }
        pid=-1;
    }
};
inline void childSignals() {
    struct sigaction action{}; action.sa_handler=SIG_DFL; ::sigemptyset(&action.sa_mask);
    static_cast<void>(::sigaction(SIGTERM,&action,nullptr));
}
inline void startRenderer(Child &child) {
    child.pid=::fork();
    if (child.pid==0) { childSignals(); ::pause(); ::_exit(0); }
}
inline int startCollector(Child &child) {
    auto pair=createPrivateProducerDescriptors(); std::array<int,2> control{-1,-1};
    if (pair[0]<0) return -1;
    if (::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,control.data())!=0) {
        ::close(pair[0]); ::close(pair[1]); return -1;
    }
    child.pid=::fork();
    if (child.pid==0) {
        childSignals(); ::close(pair[0]); ::close(control[1]);
        RuntimeSnapshot value=snapshot(); char command=0;
        while (::read(control[0],&command,1)==1) {
            if (command=='c') { ::close(pair[1]); [[maybe_unused]] const auto ignored=::write(control[0],"x",1); ::pause(); ::_exit(0); }
            if (command=='u') { value.state=RuntimeState::Unavailable; ++value.revision; }
            if (command=='r') { value.attachmentId=incarnation; value.revision=1; }
            const auto packet=encodePacket({PacketKind::Snapshot,key(),value});
            static_cast<void>(::send(pair[1],packet.constData(),static_cast<size_t>(packet.size()),MSG_NOSIGNAL));
        }
        ::_exit(0);
    }
    ::close(pair[1]); ::close(control[0]); child.control=control[1];
    return pair[0];
}
} // namespace RendererForkFixture
