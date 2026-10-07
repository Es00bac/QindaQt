// SPDX-License-Identifier: GPL-3.0-or-later
#include "report_feed.h"
#include "parsers.h"
#include <QFile>
#include <QRegularExpression>
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
namespace QindaQt::Services::AgentUsage::Private {
static ProviderUsage readOne(int directory,const QString &id,QDateTime now)
{
    auto u=emptyProvider(id);
    const auto name=QFile::encodeName(id+".json");
    const int fd=::openat(directory,name.constData(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0) {
        if(errno!=ENOENT) {u.state=UsageState::Error;u.detail="Usage report unreadable";}
        return u;
    }
    struct stat st{};
    // AGENT-GUARD: Validate the opened inode, not a raceable pathname.
    // Links, foreign-owned files and special files cannot become report input.
    if(::fstat(fd,&st)!=0||!S_ISREG(st.st_mode)||st.st_uid!=::geteuid()||
       st.st_nlink!=1||st.st_size<0||st.st_size>65536||(st.st_mode&0022)!=0) {
        ::close(fd);u.state=UsageState::Error;u.detail="Invalid usage report";return u;
    }
    QFile file;
    if(!file.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)) {
        ::close(fd);u.state=UsageState::Error;u.detail="Usage report unreadable";return u;
    }
    return parseReport(file.read(65537),id,now);
}
UsageSnapshot readReports(const QString &directory,QDateTime now)
{
    QStringList ids{"claude","kimi","glm","deepseek","mistral","opencode","other"};
    UsageSnapshot rows;
    const int fd=::open(QFile::encodeName(directory).constData(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0) {
        const int error=errno;
        for(const auto &id:ids) {
            auto u=emptyProvider(id);
            if(error!=ENOENT) {u.state=UsageState::Error;u.detail="Report directory unavailable";}
            rows.append(u);
        }
        return rows;
    }
    struct stat st{};
    if(::fstat(fd,&st)!=0||st.st_uid!=::geteuid()||(st.st_mode&0022)!=0) {
        ::close(fd);
        for(const auto &id:ids) {auto u=emptyProvider(id);u.state=UsageState::Error;u.detail="Invalid report directory";rows.append(u);}
        return rows;
    }
    bool omitted=false;
    const int scanfd=::dup(fd);
    DIR *stream=scanfd>=0?::fdopendir(scanfd):nullptr;
    if(!stream) {if(scanfd>=0) ::close(scanfd);omitted=true;}
    else {
        int inspected=0;
        QStringList custom;
        const QRegularExpression pattern("^[a-z][a-z0-9-]{0,31}$");
        while(const auto *entry=::readdir(stream)) {
            const auto name=QString::fromLocal8Bit(entry->d_name);
            if(name=="."||name=="..") continue;
            if(++inspected>64) {omitted=true;break;}
            if(!name.endsWith(".json")) continue;
            const auto id=name.chopped(5);
            if(id=="codex"||ids.contains(id)||!pattern.match(id).hasMatch()) continue;
            if(custom.size()>=31) {omitted=true;break;}
            custom.append(id);
        }
        ::closedir(stream);
        std::sort(custom.begin(),custom.end());
        ids.append(custom);
    }
    for(const auto &id:ids) rows.append(readOne(fd,id,now));
    ::close(fd);
    if(omitted) {
        auto u=emptyProvider("report-feed");u.displayName="Report feed";u.state=UsageState::Error;
        u.detail="Report discovery limit reached; some entries omitted";rows.append(u);
    }
    return rows;
}
}
