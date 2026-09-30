// SPDX-License-Identifier: GPL-3.0-or-later
#include "import_passwords_p.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <stdexcept>
namespace qindaqt::keyring::importer {
Passwords::Passwords(int transferred,service::ProcessPromptProvider &provider,std::function<bool()> admitted)
    :fd_(transferred),provider_(provider),admitted_(std::move(admitted)) {
    if(fd_<0) return;
    struct stat info{};bool safe=false;
    if(fstat(fd_,&info)==0 && info.st_uid==geteuid()) {
        if(S_ISFIFO(info.st_mode)) {
            const auto name="/proc/self/fd/"+QByteArray::number(fd_);char target[128];
            const auto length=readlink(name.constData(),target,sizeof(target));
            safe=length>0 && QByteArray(target,static_cast<qsizetype>(length)).startsWith("pipe:[");
        } else if(S_ISSOCK(info.st_mode)) {
            struct ucred peer{};socklen_t size=sizeof(peer);int type=0;socklen_t typeSize=sizeof(type);
            safe=getsockopt(fd_,SOL_SOCKET,SO_PEERCRED,&peer,&size)==0 && peer.uid==geteuid()
                && getsockopt(fd_,SOL_SOCKET,SO_TYPE,&type,&typeSize)==0 && type==SOCK_STREAM;
        }
    }
    const int flags=fcntl(fd_,F_GETFL);
    if(!safe || flags<0 || (flags&O_ACCMODE)==O_WRONLY || fcntl(fd_,F_SETFL,flags|O_NONBLOCK)!=0
        || fcntl(fd_,F_SETFD,FD_CLOEXEC)!=0) {close(fd_);fd_=-1;throw std::runtime_error("Password transport unavailable");}
}
Passwords::~Passwords(){if(fd_>=0) close(fd_);}
bool Passwords::read(unsigned char *output,std::size_t bytes) {
    std::size_t used=0;
    while(used<bytes && deadline_.elapsed()<30000) {
        QCoreApplication::processEvents();if(!admitted_()) return false;
        const auto count=::read(fd_,output+used,bytes-used);
        if(count>0) {used+=static_cast<std::size_t>(count);continue;}
        if(count==0 || (errno!=EAGAIN && errno!=EINTR)) return false;
        pollfd input{fd_,POLLIN,0};if(poll(&input,1,50)<0 && errno!=EINTR) return false;
    }
    return used==bytes && admitted_();
}
ImportPassword Passwords::take(const QString &id,const QString &label,bool existing) {
    QCoreApplication::processEvents();if(!admitted_()) return {CollectionImportError::OwnerLost,SecureBuffer{}};
    if(fd_>=0) {
        deadline_.start();
        std::array<unsigned char,4> header{};
        if(!read(header.data(),header.size())) return {admitted_()?CollectionImportError::Cancelled:CollectionImportError::OwnerLost,SecureBuffer{}};
        const std::size_t size=(std::size_t(header[0])<<24)|(std::size_t(header[1])<<16)|(std::size_t(header[2])<<8)|header[3];
        if(!size) return {CollectionImportError::Cancelled,SecureBuffer{}};
        if(size>4096) return {CollectionImportError::InvalidInput,SecureBuffer{}};
        SecureBuffer password(size);
        if(!read(password.bytes().data(),size)) return {admitted_()?CollectionImportError::Cancelled:CollectionImportError::OwnerLost,SecureBuffer{}};
        return {CollectionImportError::None,std::move(password)};
    }
    QEventLoop wait;SecureBuffer password;bool cancelled=true,completed=false;
    const auto token=provider_.begin({existing?"unlock":"create",id,label,"QindaQt one-time keyring import",{}},
        [&](SecureBuffer value,bool refused){password=std::move(value);cancelled=refused;completed=true;wait.quit();});
    QTimer lifetime;QObject::connect(&lifetime,&QTimer::timeout,&wait,[&]{if(!admitted_()) wait.quit();});lifetime.start(100);
    if(!completed) wait.exec();
    if(!completed) provider_.cancel(token);
    if(!admitted_()) return {CollectionImportError::OwnerLost,SecureBuffer{}};
    if(cancelled || !password.size()) return {CollectionImportError::Cancelled,SecureBuffer{}};
    return {CollectionImportError::None,std::move(password)};
}
}
