// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/hybrid/topologycoordinator.h>

#include <QtGlobal>

#include <memory>

namespace QindaQt::Hybrid::Test {

inline Core::WindowContainer splitContainer(const QString &containerId,
                                            const QString &idPrefix,
                                            const QString &firstWindow,
                                            const QString &secondWindow)
{
    Core::WindowContainer container(containerId);
    QString error;
    const bool pageAdded = container.addPage(idPrefix + QStringLiteral("-page"),
                                             idPrefix + QStringLiteral("-leaf-a"),
                                             firstWindow,
                                             &error);
    Q_ASSERT_X(pageAdded, "splitContainer", qPrintable(error));
    const bool split = container.splitWindow(
        {.targetWindowId = firstWindow,
         .newWindowId = secondWindow,
         .newLeafNodeId = idPrefix + QStringLiteral("-leaf-b"),
         .splitNodeId = idPrefix + QStringLiteral("-split"),
         .orientation = Core::SplitOrientation::Horizontal,
         .ratio = 0.5,
         .position = Core::InsertPosition::Second},
        &error);
    Q_ASSERT_X(split, "splitContainer", qPrintable(error));
    return container;
}

inline WindowTopology topology(QStringList independentWindows,
                               QVector<Core::WindowContainer> containers = {},
                               quint64 revision = 0)
{
    QString error;
    auto result = WindowTopology::create(std::move(independentWindows),
                                         std::move(containers),
                                         revision,
                                         &error);
    Q_ASSERT_X(result.has_value(), "topology", qPrintable(error));
    return std::move(*result);
}

inline DockIndependentWindows dockCommand(const QString &containerId,
                                          const QString &idPrefix,
                                          const QString &firstWindow,
                                          const QString &secondWindow)
{
    return {
        .containerId = containerId,
        .pageId = idPrefix + QStringLiteral("-page"),
        .firstWindowId = firstWindow,
        .firstLeafNodeId = idPrefix + QStringLiteral("-leaf-a"),
        .secondWindowId = secondWindow,
        .secondLeafNodeId = idPrefix + QStringLiteral("-leaf-b"),
        .splitNodeId = idPrefix + QStringLiteral("-split"),
        .orientation = Core::SplitOrientation::Horizontal,
        .ratio = 0.5,
        .secondPosition = Core::InsertPosition::Second,
    };
}

class AlwaysReadyTransaction final : public SceneTransaction
{
public:
    SceneStepResult prepare(const WindowTopology &,
                            const WindowTopology &,
                            const TopologyCommand &) override
    {
        return SceneStepResult::ready();
    }
    SceneStepResult commit() override { return SceneStepResult::ready(); }
    void rollback() noexcept override { }
};

class AlwaysReadyFactory final : public SceneTransactionFactory
{
public:
    std::unique_ptr<SceneTransaction> create() override
    {
        return std::make_unique<AlwaysReadyTransaction>();
    }
};

} // namespace QindaQt::Hybrid::Test
