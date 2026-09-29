// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Apps::PolkitAgent {

// What the dialog shows for "which program is asking." Empty fields are
// honest absence, never a placeholder guess; the dialog falls back to a
// generic phrase when displayName is empty.
struct RequesterInfo final {
    QString displayName;
    QString iconName;
    QString programPath;

    friend bool operator==(const RequesterInfo &, const RequesterInfo &) = default;
};

// Resolves the requesting program from its pid alone, the only fact polkit's
// details reliably carry (see the AGENT-NOTE in polkit_listener.cpp). Roots
// are injected so tests never touch the real /proc or installed .desktop
// files.
class PolkitRequesterResolver final {
public:
    explicit PolkitRequesterResolver(QString procRoot = QStringLiteral("/proc"),
                                     QStringList applicationDirectories = {});

    // Reads <procRoot>/<pid>/exe (the program path) and <procRoot>/<pid>/comm
    // (a fallback display name), then looks for a *.desktop file in
    // applicationDirectories whose Exec= basename matches the resolved
    // program's basename, cheaply (first match, no PATH search, no
    // field-code expansion -- this is a display hint, not a launch plan). A
    // pid this agent cannot read (already exited, or a namespace it cannot
    // see into) resolves to an all-empty RequesterInfo rather than a guess.
    [[nodiscard]] RequesterInfo resolve(qint64 pid) const;

private:
    QString m_procRoot;
    QStringList m_applicationDirectories;
};

} // namespace QindaQt::Apps::PolkitAgent
