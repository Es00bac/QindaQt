// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>

#include <qindaqt/shell/global_menu/protocol/menu_item_lookup.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtCore/QVariantMap>

#include <limits>
#include <utility>

namespace QindaQt::Shell::GlobalMenu
{

namespace
{

QVariantMap projectItem(const Protocol::MenuItem &item, const QString &generation)
{
    QVariantList children;
    children.reserve(item.children.size());
    for (const Protocol::MenuItem &child : item.children) {
        if (!child.visible) {
            continue;
        }
        children.append(projectItem(child, generation));
    }
    QString kind = QStringLiteral("action");
    if (item.kind == Protocol::MenuItemKind::Submenu) {
        kind = QStringLiteral("submenu");
    } else if (item.kind == Protocol::MenuItemKind::Separator) {
        kind = QStringLiteral("separator");
    }
    return {{QStringLiteral("generation"), generation},
            {QStringLiteral("id"), item.id},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("text"), item.text},
            {QStringLiteral("mnemonicIndex"), item.mnemonicIndex},
            {QStringLiteral("shortcutText"), item.shortcutText},
            {QStringLiteral("enabled"), item.enabled},
            {QStringLiteral("checkable"), item.checkable},
            {QStringLiteral("checked"), item.checked},
            {QStringLiteral("children"), children}};
}

QVariantList projectTopLevel(const Protocol::MenuTree &tree, const QString &generation)
{
    QVariantList projection;
    projection.reserve(tree.items.size());
    for (const Protocol::MenuItem &item : tree.items) {
        // Hidden entries are not presented anywhere in G0, so they are
        // omitted rather than rendered as disabled ghosts. Separators carry
        // no activation or label and are likewise not projected.
        if (item.kind == Protocol::MenuItemKind::Separator || !item.visible) {
            continue;
        }
        projection.append(projectItem(item, generation));
    }
    return projection;
}

bool admitsAction(const Protocol::MenuTree &tree, const QString &actionId)
{
    const Protocol::MenuItem *item = Protocol::findMenuItemById(tree.items, actionId);
    return item != nullptr && item->kind == Protocol::MenuItemKind::Action
        && item->enabled && item->visible;
}

} // namespace

GlobalMenuAppletAccess::GlobalMenuAppletAccess(QObject *parent)
    : QObject(parent)
{
}

bool GlobalMenuAppletAccess::available() const noexcept
{
    return m_available;
}

QVariantList GlobalMenuAppletAccess::items() const
{
    return m_topLevelProjection;
}

QString GlobalMenuAppletAccess::phase() const
{
    return m_phase;
}

QString GlobalMenuAppletAccess::reasonCode() const
{
    return m_reasonCode;
}

bool GlobalMenuAppletAccess::rendererPresent() const noexcept
{
    return m_rendererCount > 0;
}

bool GlobalMenuAppletAccess::applicationAvailable() const noexcept
{
    return m_applicationAvailable;
}

bool GlobalMenuAppletAccess::applicationProjectionRetained() const noexcept
{
    return !m_applicationProjection.isEmpty();
}

void GlobalMenuAppletAccess::attachRenderer()
{
    const bool wasPresent = rendererPresent();
    if (m_rendererCount != std::numeric_limits<quint32>::max()) {
        ++m_rendererCount;
    }
    if (!wasPresent && rendererPresent()) {
        Q_EMIT rendererPresentChanged();
    }
}

void GlobalMenuAppletAccess::detachRenderer()
{
    if (m_rendererCount == 0) {
        return;
    }
    --m_rendererCount;
    if (m_rendererCount == 0) {
        Q_EMIT rendererPresentChanged();
    }
}

void GlobalMenuAppletAccess::activate(const QString &actionId, const QString &generation)
{
    const quint64 presented = presentingDesktop() ? m_desktopGeneration
                                                  : m_applicationGeneration;
    if (generation != QString::number(presented))
        return;
    activate(actionId);
}

void GlobalMenuAppletAccess::activate(const QString &actionId)
{
    // AGENT-GUARD: this authority check is the complete boundary offered to
    // QML. A disabled/invisible/unknown/non-action id must never reach
    // `activationRequested`, mirroring NotificationCenterAppletAccess::toggle().
    // The presented channel decides where an admitted id goes, so a desktop
    // id can never reach the application's dbusmenu `Event` path and an
    // application id can never run a shell command.
    if (!m_available) {
        return;
    }
    if (presentingDesktop()) {
        if (m_desktopState == DesktopState::Active && admitsAction(m_desktopTree, actionId)) {
            Q_EMIT desktopActivationRequested(actionId);
        }
        return;
    }
    if (!admitsAction(m_tree, actionId)) {
        return;
    }
    Q_EMIT activationRequested(actionId);
}

void GlobalMenuAppletAccess::publishTree(const Protocol::MenuTree &tree)
{
    // AGENT-GUARD: the facade re-validates independently of MenuExporter so a
    // future composition path cannot smuggle an unbounded or malformed tree
    // past the exporter into QML. Rejection means "no menu", never a partial
    // tree: QML renders the unavailable placeholder instead.
    if (!Protocol::validateMenuTree(tree).accepted) {
        publishUnavailable();
        return;
    }
    m_tree = tree;
    m_applicationGeneration = ++m_generationCounter;
    m_applicationProjection =
        projectTopLevel(tree, QString::number(m_applicationGeneration));
    m_applicationAvailable = true;
    m_applicationPhase = QStringLiteral("ready");
    m_applicationReasonCode.clear();
    present();
}

void GlobalMenuAppletAccess::publishUnavailable()
{
    m_tree = Protocol::MenuTree{};
    m_applicationProjection.clear();
    m_applicationAvailable = false;
    m_applicationPhase = QStringLiteral("unavailable");
    m_applicationReasonCode.clear();
    present();
}

void GlobalMenuAppletAccess::publishDegraded(const QString &reasonCode)
{
    m_tree = Protocol::MenuTree{};
    m_applicationProjection.clear();
    m_applicationAvailable = false;
    m_applicationPhase = QStringLiteral("degraded");
    m_applicationReasonCode = reasonCode;
    present();
}

void GlobalMenuAppletAccess::beginTransition()
{
    if (m_applicationProjection.isEmpty()) {
        // Nothing retained to hold the slot open: this is indistinguishable
        // from unavailable for presentation purposes.
        publishUnavailable();
        return;
    }
    m_applicationAvailable = false;
    m_applicationPhase = QStringLiteral("loading");
    m_applicationReasonCode.clear();
    present();
}

void GlobalMenuAppletAccess::publishDesktopTree(const Protocol::MenuTree &tree,
                                                const QString &title,
                                                const QString &iconName)
{
    if (!Protocol::validateMenuTree(tree).accepted) {
        withdrawDesktopTree();
        return;
    }
    if (m_desktopState != DesktopState::Absent && m_desktopTree.items == tree.items
        && m_desktopTitle == title && m_desktopIconName == iconName) {
        // AGENT-GUARD: identical content keeps its generation. Minting a new
        // one would rebuild every delegate and close an open popup for no
        // visible change (the same rule the transport applies to Unchanged).
        m_desktopTree = tree;
        m_desktopState = DesktopState::Active;
        present();
        return;
    }
    m_desktopTree = tree;
    m_desktopTitle = title;
    m_desktopIconName = iconName;
    m_desktopGeneration = ++m_generationCounter;
    m_desktopProjection = projectTopLevel(tree, QString::number(m_desktopGeneration));
    m_desktopState = DesktopState::Active;
    present();
}

void GlobalMenuAppletAccess::retainDesktopTreeInert()
{
    if (m_desktopState == DesktopState::Absent) {
        return;
    }
    m_desktopState = DesktopState::Inert;
    present();
}

void GlobalMenuAppletAccess::withdrawDesktopTree()
{
    m_desktopState = DesktopState::Absent;
    m_desktopTree = Protocol::MenuTree{};
    m_desktopProjection.clear();
    m_desktopTitle.clear();
    m_desktopIconName.clear();
    present();
}

bool GlobalMenuAppletAccess::desktopMenuShown() const noexcept
{
    return m_desktopShown;
}

QString GlobalMenuAppletAccess::desktopMenuTitle() const
{
    return m_presentedTitle;
}

QString GlobalMenuAppletAccess::desktopMenuIconName() const
{
    return m_presentedIconName;
}

bool GlobalMenuAppletAccess::presentingDesktop() const noexcept
{
    return m_applicationProjection.isEmpty() && m_desktopState != DesktopState::Absent;
}

void GlobalMenuAppletAccess::present()
{
    const bool desktop = presentingDesktop();
    QVariantList projection = desktop ? m_desktopProjection : m_applicationProjection;
    const bool available = desktop ? m_desktopState == DesktopState::Active
                                   : m_applicationAvailable;
    QString phase = desktop ? (m_desktopState == DesktopState::Active
                                   ? QStringLiteral("ready")
                                   : QStringLiteral("loading"))
                            : m_applicationPhase;
    QString reasonCode = desktop ? QString{} : m_applicationReasonCode;
    QString title = desktop ? m_desktopTitle : QString{};
    QString iconName = desktop ? m_desktopIconName : QString{};

    // AGENT-NOTE: items, then available, then phase: the order every
    // publisher emitted in before the desktop channel existed. The QML
    // renderer and its tests depend on a shrinking projection landing before
    // availability moves.
    if (m_topLevelProjection != projection) {
        m_topLevelProjection = std::move(projection);
        Q_EMIT itemsChanged();
    }
    if (m_available != available) {
        m_available = available;
        Q_EMIT availableChanged();
    }
    if (m_phase != phase || m_reasonCode != reasonCode) {
        m_phase = std::move(phase);
        m_reasonCode = std::move(reasonCode);
        Q_EMIT phaseChanged();
    }
    if (m_desktopShown != desktop || m_presentedTitle != title
        || m_presentedIconName != iconName) {
        m_desktopShown = desktop;
        m_presentedTitle = std::move(title);
        m_presentedIconName = std::move(iconName);
        Q_EMIT desktopMenuShownChanged();
    }
}

void GlobalMenuAppletAccess::requestConfirmation(const QString &token,
                                                 const QString &title,
                                                 const QString &text)
{
    if (token.isEmpty()) {
        return;
    }
    const QString previous = m_confirmation.value(QStringLiteral("token")).toString();
    if (!previous.isEmpty()) {
        // An unanswered question is declined, never silently accepted.
        m_confirmation.clear();
        Q_EMIT confirmationResolved(previous, false);
    }
    m_confirmation = {{QStringLiteral("token"), token},
                      {QStringLiteral("title"), title},
                      {QStringLiteral("text"), text}};
    m_confirmationClaimed = false;
    Q_EMIT confirmationChanged();
}

bool GlobalMenuAppletAccess::claimConfirmation(const QString &token)
{
    if (token.isEmpty() || m_confirmationClaimed
        || m_confirmation.value(QStringLiteral("token")).toString() != token) {
        return false;
    }
    m_confirmationClaimed = true;
    return true;
}

void GlobalMenuAppletAccess::withdrawConfirmation(const QString &token)
{
    if (token.isEmpty()
        || m_confirmation.value(QStringLiteral("token")).toString() != token) {
        return;
    }
    m_confirmation.clear();
    Q_EMIT confirmationChanged();
}

QVariantMap GlobalMenuAppletAccess::confirmation() const
{
    return m_confirmation;
}

void GlobalMenuAppletAccess::resolveConfirmation(const QString &token, bool accepted)
{
    // AGENT-GUARD: only the exact outstanding token resolves, and only once:
    // a stale dialog (or a second click) must never run a session action.
    if (token.isEmpty()
        || m_confirmation.value(QStringLiteral("token")).toString() != token) {
        return;
    }
    m_confirmation.clear();
    Q_EMIT confirmationChanged();
    Q_EMIT confirmationResolved(token, accepted);
}

} // namespace QindaQt::Shell::GlobalMenu
