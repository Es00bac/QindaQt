// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controllers/controller_policy.h"
#include <QSet>
namespace QindaQt::Controllers {
struct ButtonEdge { QString button; Binding binding; bool down; };
// Value-only edge resolver. Base-page modifier buttons retain their press-time
// binding, so releasing R2 always ends its drag even while a layer is active.
class ButtonLayers {
public:
    QList<ButtonEdge> sample(const Profile &profile, const QMap<QString, bool> &physical);
    const ButtonPage &page(const Profile &profile) const;
    bool heldAction(const QString &action) const;
    void clear();
private:
    QMap<QString, Binding> m_held;
    QSet<QString> m_physical, m_quarantined;
    QStringList m_modifiers;
};
} // namespace QindaQt::Controllers
