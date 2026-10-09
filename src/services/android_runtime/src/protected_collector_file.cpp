// SPDX-License-Identifier: GPL-3.0-or-later
#include "protected_collector_file_p.h"
#include <QByteArray>
#include <QStringList>
#include <elf.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Services::AndroidRuntime {
namespace {
int openProtected(const QString &path) {
    if (!path.startsWith(QLatin1Char('/'))) return -1;
    const auto parts=path.split(QLatin1Char('/'),Qt::SkipEmptyParts);
    int directory=::open("/",O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC); if (directory<0) return -1;
    struct stat status{};
    if (::fstat(directory,&status)!=0 || status.st_uid!=0 || (status.st_mode&022)!=0) { ::close(directory); return -1; }
    for (qsizetype i=0;i<parts.size();++i) {
        if (parts[i]==QLatin1String(".") || parts[i]==QLatin1String("..")) { ::close(directory); return -1; }
        const auto component=parts[i].toLocal8Bit(); const bool last=i+1==parts.size();
        const int fd=::openat(directory,component.constData(),O_CLOEXEC|O_NOFOLLOW|(last?O_RDONLY:(O_PATH|O_DIRECTORY)));
        ::close(directory);
        if (fd<0) return -1;
        if (::fstat(fd,&status)!=0 || status.st_uid!=0 || (status.st_mode&022)!=0
            || (last?!S_ISREG(status.st_mode):!S_ISDIR(status.st_mode))) { ::close(fd); return -1; }
        directory=fd;
    }
    return directory;
}
bool same(const struct stat &a,const struct stat &b) {
    return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_mode==b.st_mode && a.st_uid==b.st_uid
        && a.st_size==b.st_size && a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec
        && a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
bool nativeElf(int fd) {
    Elf64_Ehdr header{};
    if (::pread(fd,&header,sizeof(header),0)!=static_cast<ssize_t>(sizeof(header))
        || std::memcmp(header.e_ident,ELFMAG,SELFMAG)!=0 || header.e_ident[EI_CLASS]!=ELFCLASS64
        || header.e_ident[EI_DATA]!=ELFDATA2LSB || (header.e_type!=ET_EXEC && header.e_type!=ET_DYN)) return false;
#if defined(__x86_64__)
    return header.e_machine==EM_X86_64;
#elif defined(__aarch64__)
    return header.e_machine==EM_AARCH64;
#else
    return false;
#endif
}
}
ProtectedCollectorFile::ProtectedCollectorFile(QString path,bool executable):m_path(std::move(path)),m_fd(openProtected(m_path)) {
    if (m_fd>=0 && (::fstat(m_fd,&m_pin)!=0 || (executable && ((m_pin.st_mode&0111)==0 || (m_pin.st_mode&(S_ISUID|S_ISGID))!=0 || !nativeElf(m_fd))))) {
        ::close(m_fd); m_fd=-1;
    }
}
ProtectedCollectorFile::~ProtectedCollectorFile() { if (m_fd>=0) ::close(m_fd); }
bool ProtectedCollectorFile::live() const {
    if (m_fd<0) return false;
    const int current=openProtected(m_path); if (current<0) return false;
    struct stat status{}; const bool result=::fstat(current,&status)==0 && same(m_pin,status); ::close(current); return result;
}
int ProtectedCollectorFile::duplicate() const { return live()?::fcntl(m_fd,F_DUPFD_CLOEXEC,20):-1; }
QByteArray ProtectedCollectorFile::bytes() const {
    if (!live() || m_pin.st_size<0 || m_pin.st_size>4096) return {};
    QByteArray value(static_cast<qsizetype>(m_pin.st_size),'\0');
    if (::pread(m_fd,value.data(),static_cast<size_t>(value.size()),0)!=value.size() || !live()) return {};
    return value;
}
} // namespace QindaQt::Services::AndroidRuntime
