// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_types.h"

#include <QVector>

#include <algorithm>

namespace QindaQt::ApplicationCatalog {

using QindaQt::ShellLauncher::ApplicationEntry;

// One folder of the nested application tree. `id` is the stable category
// token (main presentation group id or registered XDG additional-category
// token); `label` is its human-readable form. Entries sit either directly in
// the folder or inside child folders; a child folder exists only while it
// holds at least one entry.
struct CategoryNode final
{
    QString id;
    QString label;
    // Entries whose primary category is this folder and that have no
    // registered additional category folded into a child folder.
    QVector<ApplicationEntry> entries;
    QVector<CategoryNode> children;

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return entries.isEmpty() && children.isEmpty();
    }
    // The direct child folder with this token, or nullptr. Children exist
    // only at group level; group ids are never child tokens.
    [[nodiscard]] const CategoryNode *child(const QString &token) const noexcept
    {
        const auto match = std::find_if(children.cbegin(), children.cend(),
                                        [&token](const CategoryNode &candidate) {
                                            return candidate.id == token;
                                        });
        return match == children.cend() ? nullptr : &*match;
    }
    friend bool operator==(const CategoryNode &,
                           const CategoryNode &) = default;
};

// Builds the deterministic Finder-style application tree:
// - top-level folders are the launcher's fixed presentation groups (same
//   twelve, same order, same locale-independent XDG mapping), and entries
//   with no recognized category land in Other;
// - an entry's additional registered XDG categories (the Desktop Menu
//   Specification's additional-category list, e.g. TextEditor or
//   StrategyGame) become child folders under the entry's primary group;
// - every entry appears exactly once: in the child folder for its first
//   matching additional category, or directly in its primary group;
// - child folders are pruned when empty and ordered by label, entries by the
//   catalog's display order.
//
// AGENT-NOTE: Full menu-spec <Menu> merging (applications.menu,
// .directory names, merging rules) is deliberately out of scope; this is the
// documented approximation shared by the shell launcher's flat grouping and
// any file-manager application browser (ADR-0164).
[[nodiscard]] CategoryNode buildCategoryTree(
    const QVector<ApplicationEntry> &applications);

} // namespace QindaQt::ApplicationCatalog
