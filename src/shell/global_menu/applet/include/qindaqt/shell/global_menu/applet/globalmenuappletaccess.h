// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Shell::GlobalMenu
{

// The complete authority offered to the panel applet, mirroring
// NotificationCenterAppletAccess: shell composition publishes authoritative
// state; QML only reads it and requests an activation, and every request is
// re-checked against the current tree here before it leaves this facade. G2
// wires the shell-owned transport composition; absence or loss of its exact
// authenticated provider keeps the application channel unavailable.
//
// Two publication channels feed one presentation (ADR-0260):
// - the APPLICATION channel (publishTree/publishUnavailable/publishDegraded/
//   beginTransition) carries the authenticated active window's menu and is
//   written only by the transport coordinator;
// - the DESKTOP channel (publishDesktopTree/retainDesktopTreeInert/
//   withdrawDesktopTree) carries the shell-owned desktop menu shown while no
//   application is active, and is written only by the desktop menu controller.
// `items`, `available`, `phase`, and `reasonCode` are the PRESENTED state:
// the application channel whenever it has anything to show (a tree, or a
// retained inert projection during a provider transition), otherwise the
// desktop channel when it has a tree, otherwise the application channel's
// unavailable/degraded truth. The desktop channel therefore fills only the
// empty state and never competes with an application's menu.
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
// Desktop-channel activations leave through `desktopActivationRequested`
// instead and never reach the application consumer.
class GlobalMenuAppletAccess final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(QString reasonCode READ reasonCode NOTIFY phaseChanged)
    Q_PROPERTY(bool rendererPresent READ rendererPresent NOTIFY rendererPresentChanged)
    Q_PROPERTY(bool desktopMenuShown READ desktopMenuShown NOTIFY desktopMenuShownChanged)
    Q_PROPERTY(QString desktopMenuTitle READ desktopMenuTitle NOTIFY desktopMenuShownChanged)
    Q_PROPERTY(QString desktopMenuIconName READ desktopMenuIconName NOTIFY desktopMenuShownChanged)
    Q_PROPERTY(QVariantMap confirmation READ confirmation NOTIFY confirmationChanged)

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

    // AGENT-CONTRACT (ADR-0260): the transport coordinator reads ONLY these
    // application-channel facts. A presented desktop menu must never look
    // like a retained application projection (it would start a presentation
    // grace for a provider that is gone) or like an available application
    // menu (it would acknowledge hosting to an application whose tree the
    // facade rejected).
    [[nodiscard]] bool applicationAvailable() const noexcept;
    [[nodiscard]] bool applicationProjectionRetained() const noexcept;

    // C++ callers capture the current tree synchronously. QML must provide
    // the generation embedded in its rendered item; stale generations fail closed.
    // Either form routes to the channel that is presented right now.
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

    // Desktop channel (ADR-0260). publishDesktopTree() is fail-closed like
    // publishTree(): an invalid tree withdraws the desktop channel. `title`
    // and `iconName` name the application whose menu this is (the File
    // Manager), for the active-application indicator.
    // retainDesktopTreeInert() keeps the last desktop tree painted but inert
    // (an application became active and its first tree has not landed yet);
    // it is a no-op without a desktop tree. withdrawDesktopTree() removes it.
    void publishDesktopTree(const Protocol::MenuTree &tree, const QString &title,
                            const QString &iconName);
    void retainDesktopTreeInert();
    void withdrawDesktopTree();
    [[nodiscard]] bool desktopMenuShown() const noexcept;
    [[nodiscard]] QString desktopMenuTitle() const;
    [[nodiscard]] QString desktopMenuIconName() const;

    // A shell-owned provider may ask the renderer to confirm an admitted
    // action before it runs (log out, restart, shut down). QML shows
    // {token, title, text} while it is non-empty and answers once through
    // resolveConfirmation(); only a matching token resolves, exactly once.
    // A new request replaces (and resolves as declined) an unanswered one.
    // claimConfirmation() admits exactly one renderer per token, so several
    // panel instances never stack several dialogs for one question.
    void requestConfirmation(const QString &token, const QString &title,
                             const QString &text);
    void withdrawConfirmation(const QString &token);
    [[nodiscard]] QVariantMap confirmation() const;
    Q_INVOKABLE bool claimConfirmation(const QString &token);
    Q_INVOKABLE void resolveConfirmation(const QString &token, bool accepted);

Q_SIGNALS:
    void activationRequested(QString actionId);
    void desktopActivationRequested(QString actionId);
    void availableChanged();
    void itemsChanged();
    void phaseChanged();
    void rendererPresentChanged();
    void desktopMenuShownChanged();
    void confirmationChanged();
    void confirmationResolved(QString token, bool accepted);

private:
    enum class DesktopState {
        Absent,
        Active,
        Inert,
    };

    [[nodiscard]] bool presentingDesktop() const noexcept;
    void present();

    // One counter mints every projection generation, so a delegate rendered
    // from one channel can never match the other channel's publication.
    quint64 m_generationCounter = 0;

    // Application channel.
    quint64 m_applicationGeneration = 0;
    bool m_applicationAvailable = false;
    Protocol::MenuTree m_tree;
    QVariantList m_applicationProjection;
    QString m_applicationPhase = QStringLiteral("unavailable");
    QString m_applicationReasonCode;

    // Desktop channel.
    DesktopState m_desktopState = DesktopState::Absent;
    quint64 m_desktopGeneration = 0;
    Protocol::MenuTree m_desktopTree;
    QVariantList m_desktopProjection;
    QString m_desktopTitle;
    QString m_desktopIconName;

    // Presented state (what QML and the command search read).
    bool m_available = false;
    QVariantList m_topLevelProjection;
    QString m_phase = QStringLiteral("unavailable");
    QString m_reasonCode;
    bool m_desktopShown = false;
    QString m_presentedTitle;
    QString m_presentedIconName;

    QVariantMap m_confirmation;
    bool m_confirmationClaimed = false;
    quint32 m_rendererCount = 0;
};

} // namespace QindaQt::Shell::GlobalMenu
