// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

namespace QindaQt::Shell::GlobalMenu
{

// The complete authority offered to the panel applet, mirroring
// NotificationCenterAppletAccess: shell composition publishes authoritative
// state; QML only reads it and requests an activation, and every request is
// re-checked against the current tree here before it leaves this facade. G2
// wires the shell-owned transport composition; absence or loss of its exact
// authenticated provider keeps `available` false and `items` empty.
//
// AGENT-CONTRACT (threading): an instance lives on, and all publishers and
// QML must use it on, the Qt GUI thread; there is no internal locking. A
// publisher on another thread must marshal through the GUI thread first.
//
// AGENT-CONTRACT (G1 lineage handoff): `activationRequested` carries only an
// action id by design. Before executing anything, the shell-side consumer
// must capture the window/epoch/revision lineage it currently observed for
// this facade and run Ownership::InvocationGuard against that captured
// lineage — looking up the action in whatever tree is "current" at execution
// time would recreate the request/content race the guard exists to close.
class GlobalMenuAppletAccess final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(QString reasonCode READ reasonCode NOTIFY phaseChanged)

public:
    explicit GlobalMenuAppletAccess(QObject *parent = nullptr);

    [[nodiscard]] bool available() const noexcept;
    // Top-level items plus recursively owned submenu children. Every map has
    // {id, kind, text, mnemonicIndex, shortcutText, enabled, checkable,
    // checked, children}; hidden entries are omitted and separators exist only
    // in submenu children. `activate()` still admits actions only.
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString reasonCode() const;

    Q_INVOKABLE void activate(const QString &actionId);

    // Shell composition mirrors authoritative export state. QML cannot call
    // either publisher through the meta-object boundary. publishTree() is
    // fail-closed: a tree that fails canonical validation publishes the
    // unavailable state instead of any part of its content.
    void publishTree(const Protocol::MenuTree &tree);
    void publishUnavailable();
    void publishDegraded(const QString &reasonCode);

Q_SIGNALS:
    void activationRequested(QString actionId);
    void availableChanged();
    void itemsChanged();
    void phaseChanged();

private:
    void setAvailable(bool available);
    void setTopLevelProjection(QVariantList projection);
    void setPhase(QString phase, QString reasonCode);

    bool m_available = false;
    Protocol::MenuTree m_tree;
    QVariantList m_topLevelProjection;
    QString m_phase = QStringLiteral("unavailable");
    QString m_reasonCode;
};

} // namespace QindaQt::Shell::GlobalMenu
