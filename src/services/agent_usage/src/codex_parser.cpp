// SPDX-License-Identifier: GPL-3.0-or-later
#include "parsers.h"
#include <cmath>
#include <QTimeZone>
namespace QindaQt::Services::AgentUsage::Private {
static bool finite(const QJsonValue &v,double max,double &n)
{
    if(!v.isDouble()) return false;
    n=v.toDouble();return std::isfinite(n)&&n>=0&&n<=max;
}
bool parseRateLimits(const QJsonObject &result,ProviderUsage &u)
{
    auto parseBucket=[&](const QJsonObject &b,const QString &label) {
        for(const auto &key:{QString("primary"),QString("secondary")}) {
            if(b.value(key).isNull()||b.value(key).isUndefined()) continue;
            if(!b.value(key).isObject()) return false;
            const auto o=b.value(key).toObject();QuotaWindow w;w.label=label+" "+key;
            double n=0;if(!finite(o.value("usedPercent"),100,n)) return false;w.usedPercent=n;
            if(!o.value("resetsAt").isNull()&&!o.value("resetsAt").isUndefined()) {
                if(!finite(o.value("resetsAt"),253402300799.0,n)||std::floor(n)!=n) return false;
                w.resetAt=QDateTime::fromSecsSinceEpoch(static_cast<qint64>(n),QTimeZone::UTC);
            }
            u.quotaWindows.append(w);
        }
        return true;
    };
    // AGENT-GUARD: The map is authoritative when present. The legacy view
    // duplicates its bucket and must never create a second quota allowance.
    if(result.contains("rateLimitsByLimitId")&&!result.value("rateLimitsByLimitId").isNull()) {
        if(!result.value("rateLimitsByLimitId").isObject()) return false;
        const auto map=result.value("rateLimitsByLimitId").toObject();if(map.size()>8) return false;
        int i=0;for(auto it=map.begin();it!=map.end();++it) {
            if(!it.value().isObject()||!parseBucket(it.value().toObject(),QString("Limit %1").arg(++i))) return false;
        }
        return true;
    }
    if(result.value("rateLimits").isNull()) return true;
    return result.value("rateLimits").isObject()&&parseBucket(result.value("rateLimits").toObject(),"Codex");
}
bool parseCodexUsage(const QJsonObject &result,ProviderUsage &u)
{
    if(!result.value("summary").isObject()) return false;
    auto v=result.value("summary").toObject().value("lifetimeTokens");
    if(v.isNull()||v.isUndefined()) return true;
    double n=0;if(!finite(v,9007199254740991.0,n)||std::floor(n)!=n) return false;
    u.totalTokens=static_cast<quint64>(n);u.tokenScope="lifetime";return true;
}
}
