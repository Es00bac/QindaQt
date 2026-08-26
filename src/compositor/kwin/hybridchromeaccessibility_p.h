// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridsemanticcommand.h"

#include "qindaqt/hybrid_chrome/chrometypes.h"

#include <QObject>
#include <QMap>
#include <QRect>
#include <QString>
#include <QVector>

namespace QindaQt::Compositor::KWinIntegration::AccessibilityInternal {

enum class NodeRole {
    Group,
    TabList,
    Tab,
    Button,
};

struct NodeData final
{
    struct Action final
    {
        QString name;
        HybridSemanticRequest request;

        friend bool operator==(const Action &, const Action &) = default;
    };

    QString id;
    QString parentId;
    QString name;
    QString description;
    QRect rect;
    NodeRole role = NodeRole::Group;
    bool current = false;
    bool selected = false;
    bool focused = false;
    bool enabled = true;
    bool visible = true;
    QVector<Action> actions;

    friend bool operator==(const NodeData &, const NodeData &) = default;
};

class Backend
{
public:
    virtual ~Backend() = default;
    [[nodiscard]] virtual bool focusNode(const QString &nodeId,
                                         QString *error) = 0;
    [[nodiscard]] virtual bool invokeNode(const QString &nodeId,
                                          const QString &actionName,
                                          QString *error) const = 0;
};

// QObject ownership and semantic accessibility ownership intentionally match.
// QAccessible interfaces borrow these objects and become invalid automatically
// when a plan replacement removes their node.
class NodeObject final : public QObject
{
public:
    NodeObject(NodeData data, Backend &backend, QObject *parent = nullptr);

    NodeData data;
    Backend &backend;
    NodeObject *semanticParent = nullptr;
    QVector<NodeObject *> semanticChildren;
};

void retainAccessibleFactory();
void releaseAccessibleFactory();

[[nodiscard]] QVector<NodeData> buildNodeSpecs(
    const HybridChrome::ChromeRenderPlan &plan,
    const QMap<QString, QString> &tabRepresentatives,
    bool actionsAvailable,
    bool visible,
    QString *error = nullptr);
[[nodiscard]] QString actionToken(HybridChrome::WindowAction action);

} // namespace QindaQt::Compositor::KWinIntegration::AccessibilityInternal
