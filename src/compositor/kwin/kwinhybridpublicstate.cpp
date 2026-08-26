// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridinteractionruntime.h"
#include "hybridcontainerplacement.h"
#include "hybridshortcutmanager.h"
#include "kwinchromemanager.h"
#include "kwinhybridgroupstacking.h"
#include "kwininteractionfilter.h"

namespace QindaQt::Compositor::KWinIntegration {

bool KWinHybridSession::ready() const noexcept
{
    return !m_shutdown && m_runtime && m_runtime->ready();
}

bool KWinHybridSession::inputFilterInstalled() const noexcept
{
    return ready() && m_inputFilter && m_inputFilter->installed();
}

quint64 KWinHybridSession::topologyRevision() const noexcept
{
    return m_runtime ? m_runtime->topology().revision() : 0;
}

qsizetype KWinHybridSession::containerCount() const noexcept
{
    return m_runtime ? m_runtime->topology().containerIds().size() : 0;
}

bool KWinHybridSession::isContainerMaximized(
    const QString &containerId) const noexcept
{
    return m_placement && m_placement->isMaximized(containerId);
}

QJsonObject KWinHybridSession::diagnostics() const
{
    const int chromeOverlayCount = m_chromeManager
        ? int(m_chromeManager->overlayCount()) : 0;
    const int visibleChromeOverlayCount = m_chromeManager
        ? int(m_chromeManager->visibleOverlayCount()) : 0;
    const int anchoredChromeSceneItemCount = m_chromeManager
        ? int(m_chromeManager->anchoredOverlayCount()) : 0;
    const int visibleAnchoredChromeSceneItemCount = m_chromeManager
        ? int(m_chromeManager->visibleAnchoredOverlayCount()) : 0;
    const int quarantinedContainerCount = m_chromeManager
        ? int(m_chromeManager->quarantinedContainerCount()) : 0;
    const int publishedGroupStackingCount = m_groupStacking
        ? int(m_groupStacking->publishedGroupCount()) : 0;
    return {{QStringLiteral("ready"), ready()},
            {QStringLiteral("inputFilterInstalled"), inputFilterInstalled()},
            {QStringLiteral("shortcutRegistered"),
             ready() && m_shortcuts && m_shortcuts->registered()},
            {QStringLiteral("topologyRevision"), QString::number(topologyRevision())},
            {QStringLiteral("containerCount"), int(containerCount())},
            {QStringLiteral("chromeOverlayCount"), chromeOverlayCount},
            {QStringLiteral("visibleChromeOverlayCount"),
             visibleChromeOverlayCount},
            {QStringLiteral("anchoredChromeSceneItemCount"),
             anchoredChromeSceneItemCount},
            {QStringLiteral("visibleAnchoredChromeSceneItemCount"),
             visibleAnchoredChromeSceneItemCount},
            {QStringLiteral("quarantinedContainerCount"),
             quarantinedContainerCount},
            {QStringLiteral("publishedGroupStackingCount"),
             publishedGroupStackingCount},
            {QStringLiteral("lastGroupStackingFailure"),
             m_lastGroupStackingFailure}};
}

QJsonArray KWinHybridSession::publicContainers() const
{
    QJsonArray result;
    if (!ready()) {
        return result;
    }
    const auto revision = QString::number(topologyRevision());
    for (const auto &containerId : m_runtime->topology().containerIds()) {
        result.append(QJsonObject{{QStringLiteral("id"), containerId},
                                  {QStringLiteral("revision"), revision},
                                  {QStringLiteral("authority"),
                                   QStringLiteral("hybrid-process")}});
    }
    return result;
}

std::optional<QJsonObject> KWinHybridSession::publicSnapshot(
    const QString &containerId) const
{
    if (!ready()) {
        return std::nullopt;
    }
    const auto *container = m_runtime->topology().container(containerId);
    if (!container) {
        return std::nullopt;
    }
    // AGENT-CONTRACT: The public endpoint adds protocol/status/authority.
    // This process-local provider owns only the actual topology revision and
    // the same schema-v1 value snapshot used by Hybrid mutation commands.
    return QJsonObject{{QStringLiteral("revision"),
                        QString::number(topologyRevision())},
                       {QStringLiteral("snapshot"), container->toJson()}};
}

} // namespace QindaQt::Compositor::KWinIntegration
