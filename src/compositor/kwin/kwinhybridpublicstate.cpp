// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridinteractionruntime.h"
#include "hybridcontainerplacement.h"
#include "hybridshortcutmanager.h"
#include "kwinchromemanager.h"
#include "kwinhybridgroupstacking.h"
#include "kwininteractionfilter.h"
#include "kwintaskidentitymanager.h"
#include "managedwindowregistry.h"

#include <window.h>

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

HybridChromePlanBuilder::ShadedLabel KWinHybridSession::shadedBadgeLabel(
    const QString &containerId) const
{
    const auto *container = m_runtime ? m_runtime->topology().container(containerId)
                                      : nullptr;
    if (container == nullptr) {
        return {};
    }
    // Mirrors synchronizeChrome()'s shaded branch: the generated name is used
    // only when no rename exists (ADR-0163/0168), and the title lookup is the
    // same caption read.
    // AGENT-NOTE: only the title fields matter to shadedLabel(); the metrics
    // and style chromePlanOptions() would add are unused by it, and that
    // helper is file-local to kwinhybridsession.cpp. Keeping this local avoids
    // widening that boundary for a diagnostic.
    HybridChromePlanOptions options;
    options.shaded = true;
    const auto appearance = m_appearance.appearance(containerId);
    options.containerTitle = appearance.name;
    options.containerTitleIsGenerated = false;
    if (options.containerTitle.isEmpty()) {
        options.containerTitle = m_appearance.assignedDisplayName(containerId);
        options.containerTitleIsGenerated = !options.containerTitle.isEmpty();
    }
    const HybridWindowTitleLookup titleLookup = [this](const QString &windowId) {
        const auto *window = m_registry.window(windowId);
        return window ? window->caption() : QString{};
    };
    return HybridChromePlanBuilder::shadedLabel(*container, options, titleLookup);
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
    const int shadedContainerCount = m_placement
        ? int(m_placement->shadedContainerIds().size()) : 0;
    QJsonArray shadedStripFrames;
    if (m_placement) {
        for (const auto &shadedId : m_placement->shadedContainerIds()) {
            if (const auto frame = m_placement->shadedFrame(shadedId)) {
                // ADR-0189: the label and the width it was measured at, so a
                // nested row can prove the strip was sized for the title the
                // badge actually paints instead of a constant. Diagnostics
                // only; nothing reads these to make a decision.
                const auto label = shadedBadgeLabel(shadedId);
                // The *published* plan's own label and reserved rect, so a
                // nested row sees what is actually painted rather than a
                // recomputation that could agree while the plan does not.
                QString plannedLabel;
                QRectF plannedLabelRect;
                if (m_chromeManager) {
                    if (const auto planned = m_chromeManager->plan(shadedId)) {
                        plannedLabel = planned->badgeLabelText;
                        // AGENT-GUARD: the chrome manager stores the *global*
                        // plan; only the scene overlay localizes a copy. So
                        // translate here, because a pixel probe needs the rect
                        // relative to the strip frame it adds as the origin —
                        // reporting the global rect and adding the origin
                        // again double-counts it and finds no ink at all.
                        plannedLabelRect = planned->badgeLabelRect.translated(
                            -planned->outerFrame.topLeft());
                    }
                }
                shadedStripFrames.append(QJsonObject{
                    {QStringLiteral("containerId"), shadedId},
                    {QStringLiteral("x"), frame->x()},
                    {QStringLiteral("y"), frame->y()},
                    {QStringLiteral("width"), frame->width()},
                    {QStringLiteral("height"), frame->height()},
                    {QStringLiteral("badgeLabel"), label.text},
                    {QStringLiteral("badgeLabelWidth"), label.width},
                    {QStringLiteral("paintedBadgeLabel"), plannedLabel},
                    {QStringLiteral("paintedBadgeLabelRectX"), plannedLabelRect.x()},
                    {QStringLiteral("paintedBadgeLabelRectY"), plannedLabelRect.y()},
                    {QStringLiteral("paintedBadgeLabelRectWidth"),
                     plannedLabelRect.width()},
                    {QStringLiteral("paintedBadgeLabelRectHeight"),
                     plannedLabelRect.height()}});
            }
        }
    }
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
            {QStringLiteral("shadedContainerCount"), shadedContainerCount},
            {QStringLiteral("shadedStripFrames"), shadedStripFrames},
            {QStringLiteral("iconifiedWindowCount"), int(iconifiedWindowCount())},
            {QStringLiteral("visibleIconChipCount"), int(visibleIconChipCount())},
            {QStringLiteral("iconifiedWindows"), iconifiedWindowsJson()},
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
        // AGENT-CONTRACT: naming is reported, never assigned, from here.
        // `name` is the user's rename override and is empty when the container
        // has never been renamed; `displayName` is what a surface would paint
        // and is empty only for a container no surface has named yet
        // (ADR-0163). Without these a live session offers no way at all to
        // tell whether a rename took effect.
        QJsonObject entry{{QStringLiteral("id"), containerId},
                          {QStringLiteral("revision"), revision},
                          {QStringLiteral("name"),
                           m_appearance.appearance(containerId).name},
                          {QStringLiteral("displayName"),
                           m_appearance.assignedDisplayName(containerId)},
                          {QStringLiteral("shaded"),
                           m_placement && m_placement->isShaded(containerId)},
                          {QStringLiteral("authority"),
                           QStringLiteral("hybrid-process")}};
        // The published chrome plan's geometry (ADR-0193): where the outer
        // frame, the title row and each tab are, so a harness can aim a
        // finger at a named tab instead of guessing from member frames.
        if (m_chromeManager) {
            if (const auto plan = m_chromeManager->plan(containerId)) {
                const auto rectJson = [](const QRectF &rect) {
                    return QJsonObject{{QStringLiteral("x"), rect.x()},
                                       {QStringLiteral("y"), rect.y()},
                                       {QStringLiteral("width"), rect.width()},
                                       {QStringLiteral("height"), rect.height()}};
                };
                QJsonArray tabs;
                for (const auto &tab : plan->tabs) {
                    QJsonObject tabJson = rectJson(tab.rect);
                    tabJson.insert(QStringLiteral("tabId"), tab.tabId);
                    tabJson.insert(QStringLiteral("title"), tab.title);
                    tabJson.insert(QStringLiteral("active"), tab.active);
                    tabs.append(tabJson);
                }
                entry.insert(QStringLiteral("outerFrame"), rectJson(plan->outerFrame));
                entry.insert(QStringLiteral("outerTitleBar"), rectJson(plan->outerTitleBar));
                entry.insert(QStringLiteral("tabs"), tabs);
            }
        }
        result.append(entry);
    }
    return result;
}

QVector<TaskContainerIdentity> KWinHybridSession::taskIdentityPlans() const
{
    return m_taskIdentity ? m_taskIdentity->plans()
                          : QVector<TaskContainerIdentity>{};
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
