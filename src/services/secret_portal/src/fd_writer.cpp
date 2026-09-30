// SPDX-License-Identifier: LGPL-3.0-or-later
#include "fd_writer_p.h"
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <QSocketNotifier>
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
namespace QindaQt::Services::SecretPortal::Private {
namespace {
ssize_t pipeWrite(int fd,const unsigned char *bytes,std::size_t size) {
    sigset_t blocked,previous,pending;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    if(pthread_sigmask(SIG_BLOCK,&blocked,&previous)!=0) {errno=EIO;return -1;}
    sigpending(&pending);const bool wasPending=sigismember(&pending,SIGPIPE)==1;
    const auto result=write(fd,bytes,size);const int failure=errno;
    if(result<0 && failure==EPIPE && !wasPending) {timespec noWait{};(void)sigtimedwait(&blocked,nullptr,&noWait);}
    pthread_sigmask(SIG_SETMASK,&previous,nullptr);errno=failure;return result;
}
}
class FdWriter::Data {
public:
    Data(FdWriter &object,int borrowed,std::function<bool()> allowed,std::function<void(bool)> done)
      :q(object),admitted(std::move(allowed)),completed(std::move(done)) {
        fd=fcntl(borrowed,F_DUPFD_CLOEXEC,3);if(fd<0) return;
        struct stat status{};const int flags=fcntl(fd,F_GETFL);
        if(flags<0 || (flags&O_ACCMODE)==O_RDONLY || fstat(fd,&status)!=0) {closeFd();return;}
        if(S_ISSOCK(status.st_mode)) {
            int type=0;socklen_t length=sizeof(type);socket=getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&length)==0 && type==SOCK_STREAM;
            if(!socket) {closeFd();return;}
        } else if(!S_ISFIFO(status.st_mode) || fcntl(fd,F_SETFL,flags|O_NONBLOCK)!=0) {closeFd();return;}
        watcher=std::make_unique<QSocketNotifier>(fd,QSocketNotifier::Write,&q);watcher->setEnabled(false);
        QObject::connect(watcher.get(),&QSocketNotifier::activated,&q,[this] {send();});
    }
    ~Data() {clear();}
    void closeFd() {if(fd>=0) close(fd);fd=-1;}
    void clear() {watcher.reset();closeFd();if(pages) pages->clear();pages.reset();}
    void finish(bool success) {auto callback=std::move(completed);clear();if(callback) callback(success);}
    void send() {
        if(!pages || pages->size()!=SecretSize || fd<0 || !admitted || !admitted()) {finish(false);return;}
        const auto bytes=pages->bytes();
        const auto written=socket?::send(fd,bytes.data()+offset,bytes.size()-offset,MSG_DONTWAIT|MSG_NOSIGNAL)
            :pipeWrite(fd,bytes.data()+offset,bytes.size()-offset);
        if(written<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) {watcher->setEnabled(true);return;}
        if(written<=0) {finish(false);return;}
        offset+=static_cast<std::size_t>(written);
        if(offset==SecretSize) {finish(admitted());return;}
        watcher->setEnabled(true);
    }
    FdWriter &q;int fd=-1;bool socket=false;std::size_t offset=0;SecretPages pages;
    std::function<bool()> admitted;std::function<void(bool)> completed;std::unique_ptr<QSocketNotifier> watcher;
};
FdWriter::FdWriter(int fd,std::function<bool()> admitted,std::function<void(bool)> completed,QObject *parent)
  :QObject(parent),d(std::make_unique<Data>(*this,fd,std::move(admitted),std::move(completed))) {}
FdWriter::~FdWriter()=default;
bool FdWriter::valid() const {return d->fd>=0;}
void FdWriter::start(SecretPages pages) {d->pages=std::move(pages);d->send();}
void FdWriter::cancel() {d->completed={};d->clear();}
}
