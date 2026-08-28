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
    // The tree revision the requesting UI observed. An older same-epoch
    // revision is stale after a newer adoption and must fail.
    quint64 revision = 0;
    QString actionId;
};

struct InvocationResult final {
    bool accepted = false;
    // One of: "no-active-provider", "stale-owner", "unknown-action",
    // "not-invocable", "disabled". Empty when accepted.
    QString reasonCode;
};

// Authorizes one action invocation against the currently authenticated owner
// and its lineage. The request's (windowId, epoch, revision), the presented
// tree's lineage, and the selector's current lineage must all agree; any
// mismatch is rejected as `stale-owner` before the action is looked up, so a
// request issued against a menu that has since changed owner or been
// republished can never reach an action that belongs to a different provider
// or to content the current provider never published.
class InvocationGuard final
{
public:
    [[nodiscard]] static InvocationResult evaluate(const ActiveProviderSelector &selector,
                                                   const Protocol::MenuTree &tree,
                                                   const InvocationRequest &request);
};

} // namespace QindaQt::Shell::GlobalMenu::Ownership
