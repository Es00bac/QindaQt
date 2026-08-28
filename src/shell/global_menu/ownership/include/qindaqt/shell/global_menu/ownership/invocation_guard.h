// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>

#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

struct InvocationRequest final {
    QUuid windowId;
    QUuid epoch;
    QString actionId;
};

struct InvocationResult final {
    bool accepted = false;
    // One of: "no-active-provider", "stale-owner", "unknown-action",
    // "not-invocable", "disabled". Empty when accepted.
    QString reasonCode;
};

// Authorizes one action invocation against the currently authenticated
// owner. Any mismatch between the request's (windowId, epoch) and the
// selector's current lineage is rejected as a stale owner before the action
// is even looked up, so a request issued against a menu that has since
// changed owner (focus moved, window closed, provider replaced) can never
// reach an action that belongs to a different provider. The presented tree
// must carry that same current lineage: a caller that pairs a current request
// with an older tree is rejected identically, closing the gap where an action
// would be authorized against content the current provider never published.
class InvocationGuard final
{
public:
    [[nodiscard]] static InvocationResult evaluate(const ActiveProviderSelector &selector,
                                                     const Protocol::MenuTree &tree,
                                                     const InvocationRequest &request);
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
