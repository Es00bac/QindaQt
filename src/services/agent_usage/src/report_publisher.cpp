// SPDX-License-Identifier: GPL-3.0-or-later
#include "parsers.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QTimeZone>
#include <QRegularExpression>
#include <QTextStream>
#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
using namespace QindaQt::Services::AgentUsage;
static bool objectField(const QJsonObject &o,const QString &key,QJsonObject &out)
{
    const auto value=o.value(key);
    if(value.isNull()||value.isUndefined()) return true;
    if(!value.isObject()) return false;
    out=value.toObject();return true;
}
static std::optional<QJsonObject> projectClaude(const QJsonObject &raw,QDateTime now)
{
    QJsonObject context,cost,limits;
    if(!objectField(raw,"context_window",context)||!objectField(raw,"cost",cost)||!objectField(raw,"rate_limits",limits))
        return std::nullopt;
    QJsonObject o{{"schemaVersion",1},{"providerId","claude"},{"source","claude-statusline"},
        {"observedAt",now.toString(Qt::ISODateWithMs)},{"scope","report-period"},{"tokenScope","context"},{"costScope","session"}};
    for(const auto &pair:{qMakePair(QString("total_input_tokens"),QString("inputTokens")),
                         qMakePair(QString("total_output_tokens"),QString("outputTokens"))})
        if(context.contains(pair.first)) o[pair.second]=context.value(pair.first);
    if(cost.contains("total_cost_usd")) o["reportedCostUsd"]=cost.value("total_cost_usd");
    QJsonArray windows;
    for(const auto &pair:{qMakePair(QString("five_hour"),QString("five-hour")),
                         qMakePair(QString("seven_day"),QString("seven-day")),
                         qMakePair(QString("spend_limit"),QString("spend-limit"))}) {
        QJsonObject value;
        if(!objectField(limits,pair.first,value)) return std::nullopt;
        if(value.isEmpty()) continue;
        QJsonObject window{{"label",pair.second}};
        if(value.contains("used_percentage")) window["usedPercent"]=value.value("used_percentage");
        const auto reset=value.value("resets_at");
        if(!reset.isNull()&&!reset.isUndefined()) {
            const double seconds=reset.toDouble(-1);
            if(!reset.isDouble()||!std::isfinite(seconds)||seconds<0||seconds>253402300799.0||std::floor(seconds)!=seconds)
                return std::nullopt;
            window["resetAt"]=QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds),QTimeZone::UTC).toString(Qt::ISODateWithMs);
        }
        windows.append(window);
    }
    o["quotaWindows"]=windows;return o;
}
static bool publish(const QByteArray &input,const QString &provider,bool claude,QString directory)
{
    if(input.size()>65536||!Private::uniqueReportKeys(input)) return false;
    QJsonParseError error;auto doc=QJsonDocument::fromJson(input,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()) return false;
    const auto now=QDateTime::currentDateTimeUtc();
    if(claude) {
        const auto projected=projectClaude(doc.object(),now);
        if(!projected) return false;
        doc=QJsonDocument(*projected);
    }
    const auto data=doc.toJson(QJsonDocument::Compact);
    const auto parsed=Private::parseReport(data,provider,now);
    if(parsed.state==UsageState::Error) return false;
    if(directory.isEmpty())
        directory=QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("qindaqt/agent-usage/reports");
    const auto encoded=QFile::encodeName(directory);
    // AGENT-GUARD: Publication is user-owned report data only; no hooks/settings
    // are edited. Descriptor-relative atomic rename avoids symlink races.
    if(!QDir().mkpath(directory,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner)) return false;
    const int dirfd=::open(encoded.constData(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(dirfd<0) return false;
    struct stat st{};
    if(::fstat(dirfd,&st)!=0||st.st_uid!=::geteuid()||(st.st_mode&0022)!=0) {::close(dirfd);return false;}
    const auto name=QFile::encodeName(provider+".json");
    const auto temp=QFile::encodeName(QString(".report-%1-%2").arg(::getpid()).arg(QDateTime::currentMSecsSinceEpoch()));
    const int fd=::openat(dirfd,temp.constData(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0) {::close(dirfd);return false;}
    QFile file;
    bool ok=file.open(fd,QIODevice::WriteOnly,QFileDevice::AutoCloseHandle);
    if(ok) ok=file.write(data+'\n')==data.size()+1&&file.flush()&&::fsync(fd)==0;
    if(file.isOpen()) file.close();else ::close(fd);
    if(ok) ok=::renameat(dirfd,temp.constData(),dirfd,name.constData())==0;
    if(!ok) ::unlinkat(dirfd,temp.constData(),0);
    ::close(dirfd);return ok;
}
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"provider","Provider identifier","id"});
    parser.addOption({"claude-statusline","Project only documented Claude statusline metrics"});
    parser.addOption({"directory","Explicit report directory override","path"});
    parser.process(app);
    const auto provider=parser.value("provider");
    if(!QRegularExpression("^[a-z][a-z0-9-]{0,31}$").match(provider).hasMatch()||provider=="codex"||
       (parser.isSet("claude-statusline")&&provider!="claude")) {
        QTextStream(stderr)<<"Agent usage report rejected\n";return 1;
    }
    QByteArray input;
    QSocketNotifier notifier(STDIN_FILENO,QSocketNotifier::Read);
    auto reject=[&] {notifier.setEnabled(false);QTextStream(stderr)<<"Agent usage report rejected\n";app.exit(1);};
    auto read=[&] {
        char chunk[8192];
        while(true) {
            pollfd ready{STDIN_FILENO,POLLIN,0};
            const int available=::poll(&ready,1,0);
            if(available==0) return;
            if(available<0) {if(errno==EINTR) continue;reject();return;}
            const auto count=::read(STDIN_FILENO,chunk,sizeof(chunk));
            if(count>0) {input.append(chunk,count);if(input.size()>65536) {reject();return;}}
            else if(count==0) {
                notifier.setEnabled(false);
                if(publish(input,provider,parser.isSet("claude-statusline"),parser.value("directory"))) app.exit(0);
                else reject();
                return;
            } else {
                if(errno==EAGAIN||errno==EWOULDBLOCK) return;
                if(errno==EINTR) continue;
                reject();return;
            }
        }
    };
    QObject::connect(&notifier,&QSocketNotifier::activated,&app,[&](QSocketDescriptor,QSocketNotifier::Type) {read();});
    QTimer::singleShot(5000,&app,reject);
    QTimer::singleShot(0,&app,read);
    return app.exec();
}
