// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/agent_usage/agent_usage_source.h>
using namespace QindaQt::Services::AgentUsage;
class FixtureSource final : public AgentUsageSource {
public:
    mutable int snapshots = 0;
    int refreshes = 0;
    bool projectFreshness = false;
    QDateTime clock = QDateTime::fromString(QStringLiteral("2026-10-07T12:00:00Z"), Qt::ISODate);
    UsageSnapshot values;
    UsageSnapshot snapshot() const override {
        ++snapshots; auto result=values;
        if (projectFreshness) for (auto &row : result)
            if (row.state==UsageState::Ready && row.observedAt.msecsTo(clock)>900000)
                row.state=UsageState::Stale;
        return result;
    }
    void refresh() override { ++refreshes; emit snapshotChanged(); }
};
inline ProviderUsage fixtureProvider()
{
    ProviderUsage row;
    row.providerId = QStringLiteral("claude");
    row.displayName = QStringLiteral("Claude");
    row.state = UsageState::Stale;
    row.scope = QStringLiteral("report-period");
    row.tokenScope = QStringLiteral("context");
    row.costScope = QStringLiteral("session");
    row.totalTokens = 0;
    row.reportedCostUsd = 1.25;
    row.observedAt = QDateTime::fromString(QStringLiteral("2026-10-07T12:00:00Z"), Qt::ISODate);
    row.quotaWindows = {{QStringLiteral("5-hour"), 25.0,
        QDateTime::fromString(QStringLiteral("2026-10-07T17:00:00Z"), Qt::ISODate)}};
    return row;
}
