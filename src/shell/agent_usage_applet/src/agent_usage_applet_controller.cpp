// SPDX-License-Identifier: GPL-3.0-or-later
#include "agent_usage_applet_controller.h"
namespace QindaQt::Shell::AgentUsageApplet {
AgentUsageAppletController::AgentUsageAppletController(
    Services::AgentUsage::AgentUsageSource *source, bool granted, QObject *parent)
    : QObject(parent), m_source(source), m_granted(granted)
{
    if (m_source && m_granted) {
        connect(m_source, &Services::AgentUsage::AgentUsageSource::snapshotChanged,
                this, &AgentUsageAppletController::publish);
        connect(m_source, &QObject::destroyed, this, [this] {
            m_rows.clear();
            emit changed();
        });
        publish();
    }
}
QString AgentUsageAppletController::diagnostic() const
{
    if (!m_granted) return QStringLiteral("Agent usage read access is denied by policy.");
    if (!m_source) return QStringLiteral("Agent usage source is unavailable.");
    return QStringLiteral("Local reports only. Missing limits, tokens and cost remain unknown.");
}
void AgentUsageAppletController::refresh()
{
    if (m_granted && m_source) m_source->refresh();
}
void AgentUsageAppletController::publish()
{
    m_rows = m_granted && m_source ? providerRows(m_source->snapshot()) : QVariantList{};
    emit changed();
}
}
