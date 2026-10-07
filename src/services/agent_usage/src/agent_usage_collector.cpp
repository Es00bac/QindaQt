// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/agent_usage/agent_usage_collector.h>
#include "parsers.h"
#include "report_feed.h"
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
    bool limitsSuccess=false,usageSuccess=false;
    ProviderUsage codex;
    std::optional<ProviderUsage> lastKnown;
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
        codex.state=anySuccess&&state!=UsageState::Ready?UsageState::Ready:state;
        codex.detail=anySuccess&&state!=UsageState::Ready?"Partial metadata; "+detail:detail;
        if(anySuccess) {
            codex.observedAt=clock().toUTC();lastKnown=codex;
        } else if(lastKnown) {
            codex=*lastKnown;codex.state=UsageState::Stale;
            codex.detail="Last-known metadata; "+detail;
        }
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
                if(id==1) {limitsDone=true;limitsSuccess=ok;} else {usageDone=true;usageSuccess=ok;}
                if(limitsDone&&usageDone)
                    finish(anySuccess?(limitsSuccess&&usageSuccess?"Available reported metadata; unknown metrics absent":
                                      "Partial metadata; some account metadata unavailable"):"Usage metadata unavailable",
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
    const auto reports=QindaQt::Services::AgentUsage::Private::readReports(d->directory,d->clock().toUTC());
    d->rows.resize(1);d->rows+=reports;
    d->codex=QindaQt::Services::AgentUsage::Private::emptyProvider("codex");d->codex.source="codex-app-server";
    d->codex.scope="report-period";d->codex.tokenScope="unreported";d->codex.costScope="unreported";
    d->limitsSuccess=false;d->usageSuccess=false;
    d->buffer.clear();d->output=0;d->initialized=false;d->limitsDone=false;d->usageDone=false;d->anySuccess=false;
    d->codex.detail="Collecting usage metadata";
    d->rows[0]=d->lastKnown?*d->lastKnown:d->codex;
    if(d->lastKnown) {d->rows[0].state=UsageState::Stale;d->rows[0].detail="Refreshing last-known metadata";}
    d->active=true;d->deadline.start(5000);
    d->process.setProgram(d->program);d->process.setArguments({"app-server","--listen","stdio://"});
    d->process.start();
    emit snapshotChanged();
}
}
