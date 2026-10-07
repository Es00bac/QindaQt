// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/agent_usage/agent_usage_collector.h>
#include "parsers.h"
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Services::AgentUsage {
class AgentUsageCollector::Private {
public:
    AgentUsageCollector *owner;
    QString directory,program;
    std::function<QDateTime()> clock;
    UsageSnapshot rows;
    QProcess process;
    QTimer deadline;
    QByteArray buffer;
    qsizetype output=0;
    bool active=false,initialized=false,limitsDone=false,usageDone=false,anySuccess=false;
    ProviderUsage codex;
    Private(AgentUsageCollector *o,QString dir,QString prog,std::function<QDateTime()> time)
        :owner(o),directory(std::move(dir)),program(std::move(prog)),clock(std::move(time))
    {
        for(const auto &id:{"codex","claude","kimi","glm","deepseek","mistral","opencode","other"})
            rows.append(QindaQt::Services::AgentUsage::Private::emptyProvider(QString::fromLatin1(id)));
        deadline.setSingleShot(true);
        QObject::connect(&deadline,&QTimer::timeout,owner,[this] { finish("Metadata request timed out",UsageState::Error); });
        QObject::connect(&process,&QProcess::started,owner,[this] {
            send({{"id",0},{"method","initialize"},{"params",QJsonObject{{"clientInfo",QJsonObject{
                {"name","qindaqt_agent_usage"},{"title","QindaQt agent usage"},{"version","0.1.0"}}}}}});
        });
        QObject::connect(&process,&QProcess::readyReadStandardOutput,owner,[this] { read(); });
        QObject::connect(&process,&QProcess::readyReadStandardError,owner,[this] {
            output+=process.readAllStandardError().size();
            if(output>65536) finish("Metadata output exceeded limit",UsageState::Error);
        });
        QObject::connect(&process,&QProcess::errorOccurred,owner,[this](QProcess::ProcessError) {
            if(active) finish("Metadata program unavailable",UsageState::Unavailable);
        });
        QObject::connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),owner,
            [this](int,QProcess::ExitStatus) { if(active) finish("Metadata program ended early",UsageState::Error); });
    }
    void send(const QJsonObject &o) { process.write(QJsonDocument(o).toJson(QJsonDocument::Compact)+'\n'); }
    void finish(const QString &detail,UsageState state)
    {
        if(!active) return;
        active=false;deadline.stop();
        codex.state=state;codex.detail=detail;
        if(anySuccess) codex.observedAt=clock().toUTC();
        rows[0]=codex;
        if(process.state()!=QProcess::NotRunning) process.kill();
        emit owner->snapshotChanged();
    }
    void read()
    {
        const auto bytes=process.readAllStandardOutput();output+=bytes.size();
        if(output>65536) { finish("Metadata output exceeded limit",UsageState::Error);return; }
        buffer+=bytes;
        while(active&&buffer.contains('\n')) {
            const auto at=buffer.indexOf('\n');const auto line=buffer.left(at);buffer.remove(0,at+1);
            QJsonParseError e;const auto doc=QJsonDocument::fromJson(line,&e);
            if(e.error!=QJsonParseError::NoError||!doc.isObject()) {finish("Invalid metadata response",UsageState::Error);return;}
            const auto o=doc.object();
            // AGENT-GUARD: Drop notifications/server requests and unrequested
            // fields. Never answer auth refresh, log payloads, or start turns.
            if(o.contains("method")||!o.value("id").isDouble()) continue;
            const int id=o.value("id").toInt(-1);
            if(id==0&&!initialized) {
                if(!o.value("result").isObject()) {finish("Metadata initialization unavailable",UsageState::Unavailable);return;}
                initialized=true;send({{"method","initialized"},{"params",QJsonObject{}}});
                send({{"id",1},{"method","account/rateLimits/read"},{"params",QJsonObject{}}});
                send({{"id",2},{"method","account/usage/read"},{"params",QJsonObject{}}});
            } else if(initialized&&((id==1&&!limitsDone)||(id==2&&!usageDone))) {
                auto candidate=codex;
                bool ok=o.value("result").isObject()&&
                    (id==1?QindaQt::Services::AgentUsage::Private::parseRateLimits(o.value("result").toObject(),candidate):
                           QindaQt::Services::AgentUsage::Private::parseCodexUsage(o.value("result").toObject(),candidate));
                if(ok) {codex=candidate;anySuccess=true;}
                if(id==1) limitsDone=true;else usageDone=true;
                if(limitsDone&&usageDone)
                    finish(anySuccess?"Available reported metadata; unknown metrics absent":"Usage metadata unavailable",
                           anySuccess?UsageState::Ready:UsageState::Unavailable);
            }
        }
    }
};
AgentUsageCollector::AgentUsageCollector(QString dir,QString program,std::function<QDateTime()> clock,QObject *parent)
    :AgentUsageSource(parent),d(std::make_unique<Private>(this,std::move(dir),std::move(program),std::move(clock))) {}
AgentUsageCollector::~AgentUsageCollector()
{
    d->active=false;d->deadline.stop();
    if(d->process.state()!=QProcess::NotRunning) {d->process.kill();d->process.waitForFinished(1000);}
}
UsageSnapshot AgentUsageCollector::snapshot() const
{
    auto copy=d->rows;
    for(auto &u:copy) if(u.state==UsageState::Ready&&u.observedAt.isValid()&&u.observedAt.secsTo(d->clock())>900)
        {u.state=UsageState::Stale;u.detail="Usage metadata is stale";}
    return copy;
}
void AgentUsageCollector::refresh()
{
    if(d->active||d->process.state()!=QProcess::NotRunning) return;
    // Enumerate only normalized provider filenames; no arbitrary user files,
    // symlink targets or directories are opened.
    const QDir reports(d->directory);
    if(!QFileInfo(d->directory).isSymLink()) {
        const auto names=reports.entryList({"*.json"},QDir::Files|QDir::NoSymLinks,QDir::Name);
        for(const auto &name:names.mid(0,32)) {
            const auto id=name.chopped(5);
            if(id=="codex"||!QRegularExpression("^[a-z][a-z0-9-]{0,31}$").match(id).hasMatch()) continue;
            bool known=false;for(const auto &row:d->rows) if(row.providerId==id) known=true;
            if(!known&&d->rows.size()<40) d->rows.append(QindaQt::Services::AgentUsage::Private::emptyProvider(id));
        }
    }
    for(qsizetype i=1;i<d->rows.size();++i) {
        auto u=QindaQt::Services::AgentUsage::Private::emptyProvider(d->rows[i].providerId);
        const QString path=QDir(d->directory).filePath(u.providerId+".json");
        const QFileInfo info(path);
        if(info.exists()) {
            if(QFileInfo(d->directory).isSymLink()||info.isSymLink()||!info.isFile()||info.size()>65536) {u.state=UsageState::Error;u.detail="Invalid usage report";}
            else {QFile f;const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd>=0&&f.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)) u=QindaQt::Services::AgentUsage::Private::parseReport(f.read(65537),u.providerId,d->clock().toUTC());
                  else {if(fd>=0) ::close(fd);u.state=UsageState::Error;u.detail="Usage report unreadable";}}
        }
        d->rows[i]=u;
    }
    d->codex=QindaQt::Services::AgentUsage::Private::emptyProvider("codex");d->codex.source="codex-app-server";
    d->codex.scope="report-period";d->codex.tokenScope="unreported";d->codex.costScope="unreported";
    d->buffer.clear();d->output=0;d->initialized=false;d->limitsDone=false;d->usageDone=false;d->anySuccess=false;
    d->active=true;d->deadline.start(5000);
    d->process.setProgram(d->program);d->process.setArguments({"app-server","--listen","stdio://"});
    d->process.start();
    emit snapshotChanged();
}
}
