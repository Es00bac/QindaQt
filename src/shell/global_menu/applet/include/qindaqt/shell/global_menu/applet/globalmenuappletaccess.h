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
    Q_PROPERTY(bool rendererPresent READ rendererPresent NOTIFY rendererPresentChanged)

public:
    explicit GlobalMenuAppletAccess(QObject *parent = nullptr);

    [[nodiscard]] bool available() const noexcept;
    // Top-level items plus recursively owned submenu children. Every map has
    // {id, generation, kind, text, mnemonicIndex, shortcutText, enabled, checkable,
    // checked, children}; hidden entries are omitted and separators exist only
    // in submenu children. `activate()` still admits actions only.
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString reasonCode() const;
    [[nodiscard]] bool rendererPresent() const noexcept;
    Q_INVOKABLE void attachRenderer();
    Q_INVOKABLE void detachRenderer();

    // C++ callers capture the current tree synchronously. QML must provide
    // the generation embedded in its rendered item; stale generations fail closed.
    void activate(const QString &actionId);
    Q_INVOKABLE void activate(const QString &actionId, const QString &generation);

    // Shell composition mirrors authoritative export state. QML cannot call
    // either publisher through the meta-object boundary. publishTree() is
    // fail-closed: a tree that fails canonical validation publishes the
    // unavailable state instead of any part of its content.
    void publishTree(const Protocol::MenuTree &tree);
    void publishUnavailable();
    void publishDegraded(const QString &reasonCode);

    // AGENT-CONTRACT (transition pairing): shell composition calls
    // beginTransition() only when a genuinely DIFFERENT provider is being
    // bound (focus switch to another exporting window, registrar endpoint
    // replacement): the last projection is retained so the panel slot keeps
    // its extent and delegates, but `available` drops to false — the retained
    // entries are inert presentation, never actionable for a new focus.
    // Transient identity withdrawals do NOT open a transition: the transport
    // coordinator revokes execution authority at the selector instead, so an
    // open popup and armed delegates survive same-provider churn. A
    // transition ends through publishTree() (replacement's first tree),
    // publishUnavailable(), or publishDegraded(); calling beginTransition()
    // without a retained projection is an inert no-op beyond keeping the
    // unavailable state.
    void beginTransition();

Q_SIGNALS:
    void activationRequested(QString actionId);
    void availableChanged();
    void itemsChanged();
    void phaseChanged();
    void rendererPresentChanged();

private:
    void setAvailable(bool available);
    void setTopLevelProjection(QVariantList projection);
    void setPhase(QString phase, QString reasonCode);

    quint64 m_generation = 0;
    bool m_available = false;
    Protocol::MenuTree m_tree;
    QVariantList m_topLevelProjection;
    QString m_phase = QStringLiteral("unavailable");
    QString m_reasonCode;
    quint32 m_rendererCount = 0;
};

} // namespace QindaQt::Shell::GlobalMenu
