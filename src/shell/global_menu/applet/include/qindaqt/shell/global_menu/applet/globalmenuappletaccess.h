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
// re-checked against the current tree here before it leaves this facade. G0
// wires no live publisher, so `available` stays false and `items` stays
// empty until a later milestone's transport calls publishTree().
class GlobalMenuAppletAccess final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)

public:
    explicit GlobalMenuAppletAccess(QObject *parent = nullptr);

    [[nodiscard]] bool available() const noexcept;
    // Top-level items only, each a QVariantMap with exactly
    // {id, text, mnemonicIndex, enabled, checkable, checked}. Submenu
    // expansion is presentation's job in a later milestone; G0 keeps this
    // facade flat and honest about what it actually offers.
    [[nodiscard]] QVariantList items() const;

    Q_INVOKABLE void activate(const QString &actionId);

    // Shell composition mirrors authoritative export state. QML cannot call
    // either publisher through the meta-object boundary. publishTree() is
    // fail-closed: a tree that fails canonical validation publishes the
    // unavailable state instead of any part of its content.
    void publishTree(const Protocol::MenuTree &tree);
    void publishUnavailable();

Q_SIGNALS:
    void activationRequested(QString actionId);
    void availableChanged();
    void itemsChanged();

private:
    void setAvailable(bool available);
    void setTopLevelProjection(QVariantList projection);

    bool m_available = false;
    Protocol::MenuTree m_tree;
    QVariantList m_topLevelProjection;
};

} // namespace QindaQt::Shell::GlobalMenu
