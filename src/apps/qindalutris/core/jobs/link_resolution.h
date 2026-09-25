// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: confined symlink resolution for Proton builds (ADR-0275
// section 2), shared by the archive listing check (archive_listing.h, links
// looked up in the listing) and the post-extraction check
// (staged_tree_check.h, links read from the real staged tree).
//
// Paths are component lists relative to the folder that holds the build,
// so every inside path starts with the build's top folder name. Resolution
// follows the kernel's PHYSICAL rules: each component that is itself a link
// is replaced by its own resolution before the next `..` is applied. A
// purely lexical reading is wrong -- `e/s -> ../..` then `e/x -> s/../..`
// looks inside but climbs out through s (the reviewer's lexical_escape
// archive). Components that do not exist are taken literally (a dangling
// path cannot be followed further).
//
// readLink(components) returns the raw target when that path is a symlink,
// nullopt otherwise. Absolute targets, climbing above the top folder, and
// more than kMaxLinkHops nested links all count as escapes.
inline constexpr int kMaxLinkHops = 40;

using LinkReader = std::function<std::optional<QString>(const QStringList &)>;

// Where `target`, read from a link inside directory `parent`, really leads;
// nullopt when it escapes the top folder (parent.first()).
[[nodiscard]] std::optional<QStringList> resolveConfined(const QStringList &parent,
                                                         const QString &target,
                                                         const LinkReader &readLink);

// Resolves an in-build path (e.g. a hard-link target) through any links
// among its components; nullopt when that escapes the top folder.
[[nodiscard]] std::optional<QStringList> resolvePathConfined(const QStringList &path,
                                                             const LinkReader &readLink);

} // namespace QindaQt::QindaLutris
