// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <qindaqt/services/agent_usage/agent_usage_types.h>
namespace QindaQt::Services::AgentUsage {
// AGENT-CONTRACT: Same-thread QObject ownership; consumers retain no references
// into a source. Missing metrics stay absent; refresh performs no inference.
class AgentUsageSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~AgentUsageSource() override = default;
    virtual UsageSnapshot snapshot() const = 0;
public slots:
    virtual void refresh() = 0;
signals:
    void snapshotChanged();
};
}
