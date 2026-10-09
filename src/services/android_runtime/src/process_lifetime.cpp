// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_lifetime_p.h"
#include <QFile>
#include <array>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
namespace QindaQt::Services::AndroidRuntime::ProcessLifetime {
bool pidfdMatches(int fd,int pid)
{
    std::array<char,64> target{};
    const auto path=QStringLiteral("/proc/self/fd/%1").arg(fd).toLocal8Bit();
    const auto size=::readlink(path.constData(),target.data(),target.size());
    if (size!=18 || QByteArray(target.data(),size)!=QByteArrayLiteral("anon_inode:[pidfd]")) return false;
    QFile info(QStringLiteral("/proc/self/fdinfo/%1").arg(fd));
    if (!info.open(QIODevice::ReadOnly)) return false;
    const auto bytes=info.read(4097); if (bytes.size()>4096) return false;
    for (const auto &line:bytes.split('\n')) {
        if (line.startsWith("Pid:")) {
            bool ok=false; const int actual=line.mid(4).trimmed().toInt(&ok);
            return ok && actual==pid;
        }
    }
    return false;
}
bool pinExecutable(int fd,struct stat &pin)
{
    return fd>=0 && ::fstat(fd,&pin)==0 && S_ISREG(pin.st_mode);
}
bool live(int lifetimeFd,int pid,const struct stat &pin)
{
    pollfd lifetime{lifetimeFd,POLLIN,0};
    if (::poll(&lifetime,1,0)!=0) return false;
    const auto path=QStringLiteral("/proc/%1/exe").arg(pid).toLocal8Bit();
    const int executable=::open(path.constData(),O_PATH|O_CLOEXEC);
    if (executable<0) return false;
    struct stat current{}; const bool observed=::fstat(executable,&current)==0;
    ::close(executable);
    // AGENT-GUARD: pidfd is held from observed birth; a numeric PID never
    // rebinds. Recheck after procfs observation before publishing copied truth.
    return observed && current.st_dev==pin.st_dev && current.st_ino==pin.st_ino
        && current.st_size==pin.st_size && current.st_mode==pin.st_mode
        && current.st_mtim.tv_sec==pin.st_mtim.tv_sec && current.st_mtim.tv_nsec==pin.st_mtim.tv_nsec
        && current.st_ctim.tv_sec==pin.st_ctim.tv_sec && current.st_ctim.tv_nsec==pin.st_ctim.tv_nsec
        && ::poll(&lifetime,1,0)==0;
}
} // namespace QindaQt::Services::AndroidRuntime::ProcessLifetime
