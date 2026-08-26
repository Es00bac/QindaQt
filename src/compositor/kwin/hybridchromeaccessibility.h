// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridsemanticcommand.h"

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QAccessibleInterface;

namespace QindaQt::Compositor::KWinIntegration {

struct HybridChromeAccessibleActions final
{
    // The adapter invokes only requests produced from the currently published
    // render plan. The compositor callback must still reject stale topology IDs.
    std::function<bool(const HybridSemanticRequest &, QString *)> dispatch;
};

// Exposes paint-only chrome as a virtual QAccessible tree. It creates no input
// surface or focus-taking window: native input stays with KWin, while assistive
// technology invokes the same coordinate-free semantic requests as shortcuts.
class HybridChromeAccessibilityAdapter final
{
public:
    explicit HybridChromeAccessibilityAdapter(
        HybridChromeAccessibleActions actions = {});
    ~HybridChromeAccessibilityAdapter();

    HybridChromeAccessibilityAdapter(const HybridChromeAccessibilityAdapter &) = delete;
    HybridChromeAccessibilityAdapter &operator=(
        const HybridChromeAccessibilityAdapter &) = delete;

    [[nodiscard]] bool updatePlan(const HybridChrome::ChromeRenderPlan &plan,
                                  QString *error = nullptr);
    [[nodiscard]] bool updatePlan(
        const HybridChrome::ChromeRenderPlan &plan,
        const QMap<QString, QString> &tabRepresentatives,
        QString *error = nullptr);
    [[nodiscard]] bool updatePlan(
        const HybridChrome::ChromeRenderPlan &plan,
        const QMap<QString, QString> &tabRepresentatives,
        bool visible,
        QString *error = nullptr);
    void clear() noexcept;

    [[nodiscard]] QString rootNodeId() const;
    [[nodiscard]] QStringList nodeIds() const;
    [[nodiscard]] QString focusedNodeId() const;
    [[nodiscard]] bool setFocusedNode(const QString &nodeId,
                                      QString *error = nullptr);
    [[nodiscard]] bool invoke(const QString &nodeId,
                              QString *error = nullptr) const;
    [[nodiscard]] bool invoke(const QString &nodeId,
                              const QString &actionName,
                              QString *error = nullptr) const;
    [[nodiscard]] QAccessibleInterface *interfaceForNode(
        const QString &nodeId) const;

    [[nodiscard]] static QString groupNodeId(const QString &containerId);
    [[nodiscard]] static QString tabListNodeId(const QString &containerId);
    [[nodiscard]] static QString tabNodeId(const QString &containerId,
                                           const QString &pageId);
    [[nodiscard]] static QString actionNodeId(
        const QString &containerId,
        HybridChrome::WindowAction action);
    [[nodiscard]] static QString dockPageActionName();
    [[nodiscard]] static QString reorderPagePreviousActionName();
    [[nodiscard]] static QString reorderPageNextActionName();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Compositor::KWinIntegration
