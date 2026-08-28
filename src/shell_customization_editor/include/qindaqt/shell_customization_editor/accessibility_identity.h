// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::ShellCustomizationEditor {

// Accessibility identities for the Customize editor. The canvas is a
// rendering and is ignored by assistive technology; the outline is the
// accessible representation. These pure helpers produce the deterministic
// names, position-in-set values, and announcement texts so the presentation
// never invents its own wording.

enum class AnnouncementKind {
    Polite,
    Assertive,
};

struct Announcement final {
    AnnouncementKind kind = AnnouncementKind::Polite;
    QString message;

    bool operator==(const Announcement &) const = default;
};

// Coalesces announcements per event turn: repeated announces of one kind
// replace the pending one, and drain() publishes exactly the latest tuple of
// each kind, mirroring the StateCard pattern.
class AnnouncementCenter final {
public:
    void announce(Announcement announcement);
    [[nodiscard]] QVector<Announcement> drain();
    [[nodiscard]] bool hasPending() const noexcept;

private:
    std::optional<Announcement> m_polite;
    std::optional<Announcement> m_assertive;
};

// "Top panel" / "Bottom panel on primary": edge name plus the output selector
// unless it is the wildcard. Deterministic; no theme or applet branding.
[[nodiscard]] QString panelDisplayName(const Profiles::PanelSpec &panel);

// The plugin identity is the honest fallback name; the manifest catalog is
// not a dependency of this module and may be incomplete by design.
[[nodiscard]] QString appletDisplayName(const Profiles::AppletSpec &applet);

[[nodiscard]] QString zoneDisplayName(const QString &zone);

// Position of the drop target inside the panel (1-based insert position of
// applets.size() + 1 slots).
[[nodiscard]] int dropPositionInSet(const Profiles::PanelSpec &targetPanel,
                                    const DropTarget &target);

// "Move clock to Top panel, end zone, position 3 of 4 — accepted" or
// "… — rejected: <reason>", exactly the shape the architecture specifies.
[[nodiscard]] QString describeMove(const Profiles::LayoutProfile &profile,
                                   const QString &appletName,
                                   const DropTarget &target,
                                   bool accepted,
                                   const QString &rejectionReason = {});

[[nodiscard]] Announcement moveAnnouncement(const Profiles::LayoutProfile &profile,
                                            const QString &appletName,
                                            const DropTarget &target,
                                            bool accepted,
                                            const QString &rejectionReason = {});

} // namespace QindaQt::ShellCustomizationEditor
