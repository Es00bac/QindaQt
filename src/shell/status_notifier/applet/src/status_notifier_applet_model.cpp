// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_model.h"

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

namespace QindaQt::StatusNotifierApplet {

namespace {

using namespace QindaQt::StatusNotifier;

[[nodiscard]] const ItemDescriptor *matchDescriptor(
    const QList<ItemDescriptor> &descriptors,
    const TrayItemPresentation &item)
{
    // AGENT-NOTE: ItemDescriptor carries no owner key, so the exact-owner
    // match runs over `identity`: the S1 registry guarantees one user-visible
    // identity is claimed by at most one live owner, which makes this lookup
    // unambiguous for any consistent presentation+descriptor pair. A caller
    // passing mismatched snapshots gets the fail-closed miss (title=identity,
    // no menu) instead of another item's facts.
    for (const auto &descriptor : descriptors) {
        if (descriptor.identity == item.identity) {
            return &descriptor;
        }
    }
    return nullptr;
}

[[nodiscard]] QString keyboardTextFor(const TrayItemPresentation &item, RequestKind kind)
{
    for (const auto &action : item.keyboardActions) {
        if (action.kind == kind) {
            return action.keyboardDescription;
        }
    }
    return {};
}

[[nodiscard]] StatusNotifierItemRow projectRow(
    const TrayItemPresentation &item,
    const ItemDescriptor *descriptor)
{
    StatusNotifierItemRow row;
    row.uniqueName = item.owner.uniqueName;
    row.objectPath = item.owner.objectPath;
    row.generation = item.owner.generation;
    row.identity = item.identity;
    row.accessibleName = item.accessibleName;
    row.accessibleDescription = item.accessibleDescription;
    row.accessibleStatusText = item.accessibleStatusText;
    row.keyboardActivateText = keyboardTextFor(item, RequestKind::Activate);
    row.keyboardContextMenuText = keyboardTextFor(item, RequestKind::ContextMenu);
    row.secondaryActivatePointerOnly = true;

    // AGENT-GUARD: descriptor facts (title, attention state, menu) are
    // matched by exact OwnerKey. A presentation item whose descriptor is
    // absent projects with title=identity and no menu — fail-closed
    // withholding, never a guess from another owner's descriptor.
    if (descriptor != nullptr) {
        row.title = descriptor->title.isEmpty() ? item.identity : descriptor->title;
        row.needsAttention = descriptor->status == ItemStatus::NeedsAttention;
        row.active = descriptor->status == ItemStatus::Active;
        row.hasMenu = !descriptor->menu.entries.isEmpty();
        row.menuEntryCount = static_cast<int>(descriptor->menu.entries.size());
    } else {
        row.title = item.identity;
    }
    return row;
}

} // namespace

StatusNotifierAppletProjection StatusNotifierAppletModel::project(
    const QindaQt::StatusNotifier::TrayPresentation &presentation,
    const QList<QindaQt::StatusNotifier::ItemDescriptor> &descriptors,
    bool readGranted,
    const StatusNotifierAppletTexts &texts)
{
    StatusNotifierAppletProjection projection;

    // AGENT-GUARD: read-denied withholding. Without the status-items read
    // grant the projection exposes the registered refusal code and NO rows,
    // whatever the presentation carries; observation must not leak through
    // row counts, titles, or overflow text.
    if (!readGranted) {
        projection.phase = AppletPhase::Unavailable;
        projection.phaseReason = QLatin1String(kReasonStatusItemsReadNotGranted);
        return projection;
    }

    switch (presentation.state) {
    case PresentationState::Loading:
        projection.phase = AppletPhase::Loading;
        return projection;
    case PresentationState::Empty:
        projection.phase = AppletPhase::Empty;
        return projection;
    case PresentationState::Ready:
        projection.phase = AppletPhase::Ready;
        break;
    case PresentationState::Degraded:
        // Degraded keeps the last-known-good rows actionable; the S1
        // diagnostic names the cause and becomes the phase reason verbatim.
        projection.phase = AppletPhase::Degraded;
        projection.phaseReason = presentation.diagnostic;
        break;
    }

    // Row order is the S1 presentation's stable order; truncation is truthful
    // through overflowCount/overflowText rather than silently dropping items.
    const qsizetype total = presentation.items.size();
    const qsizetype presented = qMin(total, kMaxPresentedItems);
    projection.rows.reserve(presented);
    for (qsizetype i = 0; i < presented; ++i) {
        const TrayItemPresentation &item = presentation.items.at(i);
        projection.rows.append(projectRow(item, matchDescriptor(descriptors, item)));
    }
    projection.presentedCount = static_cast<int>(presented);
    projection.overflowCount = static_cast<int>(total - presented);
    if (projection.overflowCount > 0) {
        const QString &pattern = projection.overflowCount == 1
            ? texts.overflowMoreItem
            : texts.overflowMoreItems;
        projection.overflowText = pattern.arg(projection.overflowCount);
    }
    return projection;
}

QList<StatusNotifierMenuRow> StatusNotifierAppletModel::projectMenu(
    const QindaQt::StatusNotifier::MenuPayload &menu,
    int maxDepth)
{
    QList<StatusNotifierMenuRow> rows;
    if (maxDepth < 1) {
        return rows;
    }

    // AGENT-GUARD: defense-in-depth flattening. The S1 admission gate already
    // validated parent kinds, ordering, and the depth-4 bound, but this
    // projection is a separate trust boundary: invisible entries, entries at
    // or beyond maxDepth, entries with forward/self/unknown parents or
    // non-submenu parents, and the descendants of anything dropped are all
    // omitted. Iteration is capped at the shared S1 node budget so a hostile
    // in-process caller cannot grow the flattened output past it either.
    const auto &entries = menu.entries;
    const qsizetype entryCount = qMin(entries.size(), kMaxMenuNodes);
    QList<int> depths(entryCount, -1); // -1: dropped
    QList<qsizetype> keptIndices;
    keptIndices.reserve(entryCount);

    for (qsizetype i = 0; i < entryCount; ++i) {
        const MenuEntry &entry = entries.at(i);
        if (!entry.visible) {
            continue;
        }
        int depth = 0;
        if (entry.parentId >= 0) {
            if (entry.parentId >= i) {
                continue; // Forward or self reference: invalid flat encoding.
            }
            const int parentDepth = depths.at(entry.parentId);
            if (parentDepth < 0) {
                continue; // Parent dropped or invalid: the subtree goes with it.
            }
            if (entries.at(entry.parentId).kind != MenuEntry::Kind::SubMenu) {
                continue; // Children live only beneath submenus.
            }
            depth = parentDepth + 1;
        }
        if (depth >= maxDepth) {
            continue;
        }
        depths[i] = depth;
        keptIndices.append(i);
    }

    rows.reserve(keptIndices.size());
    for (const qsizetype index : keptIndices) {
        const MenuEntry &entry = entries.at(index);
        StatusNotifierMenuRow row;
        row.depth = depths.at(index);
        switch (entry.kind) {
        case MenuEntry::Kind::Item:
            row.kind = QStringLiteral("item");
            break;
        case MenuEntry::Kind::Separator:
            row.kind = QStringLiteral("separator");
            break;
        case MenuEntry::Kind::SubMenu:
            row.kind = QStringLiteral("submenu");
            break;
        }
        row.label = entry.label;
        row.enabled = entry.enabled;
        row.visible = entry.visible;
        for (const qsizetype candidate : keptIndices) {
            if (entries.at(candidate).parentId == index) {
                row.hasChildren = true;
                break;
            }
        }
        rows.append(row);
    }
    return rows;
}

} // namespace QindaQt::StatusNotifierApplet
