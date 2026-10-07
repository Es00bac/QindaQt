// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/agent_usage/agent_usage_source.h>
#include <functional>
#include <memory>
namespace QindaQt::Services::AgentUsage {
// No constructor IO. Calls and destruction must occur on the owning thread.
// A refresh already in progress is ignored. Children are terminated on expiry
// or destruction; public errors never contain subprocess output.
class AgentUsageCollector final : public AgentUsageSource {
    Q_OBJECT
public:
    AgentUsageCollector(QString feedDirectory, QString codexProgram,
                        std::function<QDateTime()> clock, QObject *parent = nullptr);
    ~AgentUsageCollector() override;
    UsageSnapshot snapshot() const override;
public slots:
    void refresh() override;
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
