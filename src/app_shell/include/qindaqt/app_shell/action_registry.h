// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/app_shell_types.h"

#include <QObject>
#include <QVariantList>

namespace QindaQt::AppShell {

// A GUI-thread-confined, application-owned projection of stable commands.
// The registry emits activation intent; it never executes domain behavior or
// registers global shortcuts. Replacing the registry is atomic on validation
// failure, and snapshot values may be retained by QML consumers.
class ActionRegistry final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList menus READ menus NOTIFY menusChanged FINAL)

public:
    explicit ActionRegistry(QObject *parent = nullptr);

    [[nodiscard]] QVariantList menus() const;
    [[nodiscard]] QList<ActionSpec> actions() const;

    [[nodiscard]] Error replaceActions(const QList<ActionSpec> &actions);
    [[nodiscard]] Error setEnabled(const QString &actionId, bool enabled);
    [[nodiscard]] Error setChecked(const QString &actionId, bool checked);
    [[nodiscard]] Error requestActivation(const QString &actionId);

signals:
    void menusChanged();
    void activationRequested(const QString &actionId);

private:
    [[nodiscard]] Error validate(const QList<ActionSpec> &actions) const;
    [[nodiscard]] Error verifyThread() const;
    void rebuildSnapshot();

    QList<ActionSpec> m_actions;
    QVariantList m_menus;
};

} // namespace QindaQt::AppShell
