// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "validationresult.h"

#include <QJsonObject>
#include <QSet>
#include <QString>

#include <memory>
#include <optional>

namespace QindaQt::Core {

enum class SplitOrientation {
    Horizontal,
    Vertical,
};

enum class InsertPosition {
    First,
    Second,
};

class WindowContainer;

class LayoutNode final
{
public:
    LayoutNode(const LayoutNode &other);
    LayoutNode(LayoutNode &&other) noexcept;
    LayoutNode &operator=(const LayoutNode &other);
    LayoutNode &operator=(LayoutNode &&other) noexcept;
    ~LayoutNode();

    [[nodiscard]] static LayoutNode makeLeaf(QString nodeId, QString windowId);
    [[nodiscard]] static LayoutNode makeSplit(QString nodeId,
                                              SplitOrientation orientation,
                                              // Ratio is the first child's share and must be in (0, 1).
                                              double ratio,
                                              LayoutNode first,
                                              LayoutNode second);

    [[nodiscard]] const QString &id() const noexcept { return m_id; }
    [[nodiscard]] bool isLeaf() const noexcept { return m_kind == Kind::Leaf; }
    [[nodiscard]] bool isSplit() const noexcept { return m_kind == Kind::Split; }
    // AGENT-NOTE: windowId() is empty for splits; use isLeaf() before consuming it.
    [[nodiscard]] const QString &windowId() const noexcept { return m_windowId; }
    [[nodiscard]] std::optional<SplitOrientation> orientation() const noexcept;
    [[nodiscard]] std::optional<double> ratio() const noexcept;
    [[nodiscard]] const LayoutNode *firstChild() const noexcept { return m_first.get(); }
    [[nodiscard]] const LayoutNode *secondChild() const noexcept { return m_second.get(); }

    [[nodiscard]] const LayoutNode *findNode(const QString &nodeId) const noexcept;
    [[nodiscard]] const LayoutNode *findWindow(const QString &windowId) const noexcept;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<LayoutNode> fromJson(const QJsonObject &object,
                                                            QString *error = nullptr);

private:
    enum class Kind {
        Leaf,
        Split,
    };

    enum class RemovalOutcome {
        NotFound,
        RemoveThisNode,
        RemovedDescendant,
    };

    LayoutNode(QString nodeId, QString windowId);
    LayoutNode(QString nodeId,
               SplitOrientation orientation,
               double ratio,
               LayoutNode first,
               LayoutNode second);

    [[nodiscard]] bool splitWindow(const QString &targetWindowId,
                                   LayoutNode newLeaf,
                                   QString splitNodeId,
                                   SplitOrientation orientation,
                                   double ratio,
                                   InsertPosition position);
    [[nodiscard]] bool setSplitRatio(const QString &splitNodeId, double ratio) noexcept;
    [[nodiscard]] RemovalOutcome removeWindow(const QString &windowId,
                                              QString *removedLeafNodeId);
    [[nodiscard]] LayoutNode *findWindowMutable(const QString &windowId) noexcept;
    [[nodiscard]] ValidationResult validate(QSet<QString> &structuralIds,
                                            QSet<QString> &windowIds,
                                            const QString &path) const;

    Kind m_kind;
    QString m_id;
    QString m_windowId;
    SplitOrientation m_orientation = SplitOrientation::Horizontal;
    double m_ratio = 0.5;
    std::unique_ptr<LayoutNode> m_first;
    std::unique_ptr<LayoutNode> m_second;

    friend class WindowContainer;
};

} // namespace QindaQt::Core
