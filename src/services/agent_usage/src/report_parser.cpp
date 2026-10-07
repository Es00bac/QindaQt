// SPDX-License-Identifier: GPL-3.0-or-later
#include "parsers.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QMap>
#include <cmath>
namespace QindaQt::Services::AgentUsage::Private {
ProviderUsage emptyProvider(const QString &id)
{
    ProviderUsage u; u.providerId=id;
    const QMap<QString, QString> names{{"codex","Codex"},{"claude","Claude"},{"kimi","Kimi"},
        {"glm","GLM"},{"deepseek","DeepSeek"},{"mistral","Mistral"},{"opencode","OpenCode"},{"other","Other"}};
    u.displayName=names.value(id,id); u.detail="No usage report"; return u;
}
// Qt JSON keeps the last duplicate key. Reject duplicates before projection so
// hostile reports cannot silently change identity or scope.
bool uniqueReportKeys(const QByteArray &bytes)
{
    QVector<QSet<QString>> objects;
    for(qsizetype i=0;i<bytes.size();++i) {
        const char c=bytes[i];
        if(c=='{') objects.append(QSet<QString>{});
        else if(c=='}') {if(!objects.isEmpty()) objects.removeLast();}
        else if(c=='"') {
            const auto start=i++;
            while(i<bytes.size()) {
                if(bytes[i]=='\\') {i+=2;continue;}
                if(bytes[i]=='"') break;
                ++i;
            }
            auto next=i+1;
            while(next<bytes.size()&&(bytes[next]==' '||bytes[next]=='\n'||bytes[next]=='\r'||bytes[next]=='\t')) ++next;
            if(next<bytes.size()&&bytes[next]==':'&&!objects.isEmpty()) {
                auto wrapped=QByteArray("[")+bytes.mid(start,i-start+1)+"]";
                const auto parsed=QJsonDocument::fromJson(wrapped).array();
                if(parsed.isEmpty()) return false;
                auto name=parsed.at(0).toString();
                if(objects.last().contains(name)) return false;
                objects.last().insert(name);
            }
        }
    }
    return true;
}
static bool keys(const QJsonObject &o, const QSet<QString> &allowed)
{
    for (auto i=o.begin();i!=o.end();++i) if (!allowed.contains(i.key())) return false;
    return true;
}
static bool number(const QJsonValue &v,double max,double &out)
{
    if (!v.isDouble()) return false;
    out=v.toDouble(); return std::isfinite(out)&&out>=0&&out<=max;
}
static bool scope(const QJsonValue &v)
{
    return v.isString() && QSet<QString>{"lifetime","session","context","billing-period","report-period","unreported"}.contains(v.toString());
}
ProviderUsage parseReport(const QByteArray &bytes,const QString &id,QDateTime now)
{
    auto u=emptyProvider(id);
    auto fail=[&] { auto invalid=emptyProvider(id);invalid.state=UsageState::Error;invalid.detail="Invalid usage report";return invalid; };
    if (bytes.size()>65536||!uniqueReportKeys(bytes)) return fail();
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()) return fail();
    const auto o=doc.object();
    if(!keys(o,{"schemaVersion","providerId","source","observedAt","scope","tokenScope","costScope",
        "inputTokens","outputTokens","totalTokens","reportedCostUsd","quotaWindows"})||
       o.value("schemaVersion")!=QJsonValue(1)||o.value("providerId")!=QJsonValue(id)) return fail();
    if(!QSet<QString>{"claude-statusline","provider-report","manual-report"}.contains(o.value("source").toString())||
       !scope(o.value("scope"))||!scope(o.value("tokenScope"))||!scope(o.value("costScope"))) return fail();
    const auto date=o.value("observedAt").toString();
    u.observedAt=QDateTime::fromString(date,Qt::ISODateWithMs);
    if(!date.endsWith('Z')||!u.observedAt.isValid()||u.observedAt>now.addSecs(300)) return fail();
    u.source=o.value("source").toString();u.scope=o.value("scope").toString();
    u.tokenScope=o.value("tokenScope").toString();u.costScope=o.value("costScope").toString();
    auto token=[&](const QString &key,std::optional<quint64> &target) {
        if(!o.contains(key)||o.value(key).isNull()) return true;
        double n=0;if(!number(o.value(key),9007199254740991.0,n)||std::floor(n)!=n) return false;
        target=static_cast<quint64>(n);return true;
    };
    if(!token("inputTokens",u.inputTokens)||!token("outputTokens",u.outputTokens)||!token("totalTokens",u.totalTokens)) return fail();
    if(o.contains("reportedCostUsd")&&!o.value("reportedCostUsd").isNull()) {
        double n=0;if(!number(o.value("reportedCostUsd"),1e12,n)) return fail();u.reportedCostUsd=n;
    }
    if(o.contains("quotaWindows")) {
        if(!o.value("quotaWindows").isArray()||o.value("quotaWindows").toArray().size()>16) return fail();
        for(const auto &v:o.value("quotaWindows").toArray()) {
            if(!v.isObject()) return fail();
            const auto q=v.toObject();
            if(!keys(q,{"label","usedPercent","resetAt"})||!q.value("label").isString()) return fail();
            const auto label=q.value("label").toString();
            if(!QSet<QString>{"five-hour","seven-day","daily","weekly","monthly","primary","secondary","quota","spend-limit"}.contains(label)) return fail();
            QuotaWindow w;w.label=label;
            if(q.contains("usedPercent")&&!q.value("usedPercent").isNull()) {
                double n=0;if(!number(q.value("usedPercent"),label=="spend-limit"?1000000:100,n)) return fail();w.usedPercent=n;
            }
            if(q.contains("resetAt")&&!q.value("resetAt").isNull()) {
                const auto s=q.value("resetAt").toString();auto t=QDateTime::fromString(s,Qt::ISODateWithMs);
                if(!s.endsWith('Z')||!t.isValid()) return fail();
                w.resetAt=t;
            }
            u.quotaWindows.append(w);
        }
    }
    u.state=u.observedAt.secsTo(now)>900?UsageState::Stale:UsageState::Ready;
    u.detail=u.state==UsageState::Stale?"Usage report is stale":"Reported metadata";
    return u;
}
}
