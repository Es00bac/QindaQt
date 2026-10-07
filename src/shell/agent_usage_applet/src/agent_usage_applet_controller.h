// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "agent_usage_presentation.h"
#include <QObject>
#include <QPointer>
namespace QindaQt::Shell::AgentUsageApplet {
// Same-thread facade. Source is borrowed and may disappear; denial neither
// snapshots nor refreshes it. Collection/files/processes stay outside this module.
class AgentUsageAppletController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList providerRows READ rows NOTIFY changed)
    Q_PROPERTY(bool readGranted READ readGranted CONSTANT)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY changed)
public:
    explicit AgentUsageAppletController(Services::AgentUsage::AgentUsageSource *source,
                                       bool granted, QObject *parent=nullptr);
    QVariantList rows() const { return m_rows; }
    bool readGranted() const { return m_granted; }
    QString diagnostic() const;
    Q_INVOKABLE void refresh();
    // Re-project source-owned observation freshness only; never collect.
    Q_INVOKABLE void checkFreshness();
signals:
    void changed();
private:
    void publish();
    QPointer<Services::AgentUsage::AgentUsageSource> m_source;
    bool m_granted;
    QVariantList m_rows;
};
}
