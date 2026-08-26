// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "layoutnode.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Core {

class ContainerPage final
{
public:
    ContainerPage(QString pageId, LayoutNode root);

    [[nodiscard]] const QString &id() const noexcept { return m_id; }
    [[nodiscard]] const LayoutNode &root() const noexcept { return m_root; }

private:
    QString m_id;
    LayoutNode m_root;

    friend class WindowContainer;
};

struct SplitRequest final
{
    QString targetWindowId;
    QString newWindowId;
    QString newLeafNodeId;
    QString splitNodeId;
    SplitOrientation orientation = SplitOrientation::Horizontal;
    // Proportion assigned to the first child after insertion, strictly (0, 1).
    double ratio = 0.5;
    InsertPosition position = InsertPosition::Second;
};

struct DetachedWindow final
{
    QString windowId;
    QString leafNodeId;
    QString sourcePageId;
};

class WindowContainer final
{
public:
    static constexpr int JsonSchemaVersion = 1;

    explicit WindowContainer(QString containerId);

    [[nodiscard]] const QString &id() const noexcept { return m_id; }
    [[nodiscard]] const QVector<ContainerPage> &pages() const noexcept { return m_pages; }
    [[nodiscard]] const QString &activePageId() const noexcept { return m_activePageId; }
    [[nodiscard]] const ContainerPage *page(const QString &pageId) const noexcept;
    [[nodiscard]] const LayoutNode *findNode(const QString &nodeId) const noexcept;
    [[nodiscard]] const LayoutNode *findWindow(const QString &windowId) const noexcept;
    [[nodiscard]] std::optional<QString> singleWindowId() const;

    // AGENT-NOTE: IDs are caller-owned persisted handles. Structural IDs
    // (container/page/node) share one namespace; external window IDs share another.
    // The first page becomes active; subsequent additions preserve active page.
    [[nodiscard]] bool addPage(QString pageId,
                               QString leafNodeId,
                               QString windowId,
                               QString *error = nullptr);
    [[nodiscard]] bool addPage(ContainerPage page, QString *error = nullptr);
    [[nodiscard]] bool activatePage(const QString &pageId, QString *error = nullptr);
    [[nodiscard]] bool movePage(const QString &pageId,
                                qsizetype destinationIndex,
                                QString *error = nullptr);
    [[nodiscard]] bool splitWindow(const SplitRequest &request, QString *error = nullptr);
    [[nodiscard]] bool swapWindows(const QString &firstWindowId,
                                   const QString &secondWindowId,
                                   QString *error = nullptr);
    [[nodiscard]] bool setSplitRatio(const QString &splitNodeId,
                                     double ratio,
                                     QString *error = nullptr);

    [[nodiscard]] std::optional<DetachedWindow> detachWindow(const QString &windowId,
                                                             QString *error = nullptr);
    [[nodiscard]] bool removeWindow(const QString &windowId, QString *error = nullptr);

    [[nodiscard]] ValidationResult validate() const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<WindowContainer> fromJson(const QJsonObject &object,
                                                                 QString *error = nullptr);

private:
    [[nodiscard]] bool containsStructuralId(const QString &id) const noexcept;

    QString m_id;
    QVector<ContainerPage> m_pages;
    QString m_activePageId;
};

} // namespace QindaQt::Core
