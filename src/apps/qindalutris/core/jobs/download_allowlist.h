// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the ADR-0275 section 6 download allowlist. QindaLutris
// fetches files ONLY over HTTPS and ONLY from these exact hosts (no suffix or
// wildcard matching, so `downloader.battle.net.evil.com` is refused). Every
// redirect hop is re-checked against the same list by the production
// downloader (network_downloader.h) and every job re-checks before starting.
// Adding a host is a reviewed change to this table plus the wiki page
// docs/wiki/apps/qindalutris-installs.md -- never a runtime setting.

// AGENT-NOTE: placeholder for the compatibility-database refresh location
// (ADR-0275 section 3). `.invalid` is an RFC 2606 reserved TLD, so until the
// real host is decided the entry can never resolve or match a live server.
inline constexpr char kCompatDbRefreshHost[] = "compat-db.qindaqt.invalid";

// The fixed host table, lower-case, in documentation order.
[[nodiscard]] const QStringList &allowedDownloadHosts();

// True only for an absolute https URL whose host is exactly one allowlisted
// host, with no user-info and the default port. Pure.
[[nodiscard]] bool isAllowedDownloadUrl(const QUrl &url);
// Parses in QUrl::StrictMode first; anything strict parsing rejects (spaces,
// backslashes, control characters) is refused before the host is examined.
[[nodiscard]] bool isAllowedDownloadUrl(const QString &text);

} // namespace QindaQt::QindaLutris
