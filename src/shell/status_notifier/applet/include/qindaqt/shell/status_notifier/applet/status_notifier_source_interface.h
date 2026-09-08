// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtGui/QImage>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>

namespace QindaQt::StatusNotifierApplet {

// AGENT-CONTRACT: Injected least-authority public source seam for the
// StatusNotifier tray applet. The applet controller depends strictly on this
// interface. It never opens D-Bus connections, never touches the registry,
// the monitor, or icon-theme internals directly, and never mutates anything
// without the generation fencing described below.
//
// Threading: the seam is GUI-thread confined. The controller invokes every
// virtual member directly on its own thread and expects `changed()` to be
// emitted on that same thread. A seam backed by another thread must marshal
// through Qt queued connections (the values crossing the boundary are
// implicitly-shared Qt copies, safe to hand over after the emitting call
// returns); re-entrant emission of `changed()` from inside an intent
// dispatch call is supported and fenced by the controller's exactly-once
// dispatch guard.
//
// Lifetime: the controller borrows the seam and never deletes it. The
// composing shell must guarantee the source outlives the controller (or
// parents it accordingly); destroying the source first is undefined behavior
// for any in-flight controller call.
//
// Errors and results: observation members (presentation/itemDescriptors/
// renderIcon/currentGeneration) are pure reads and never mutate anything.
// renderIcon NEVER returns a null image: any failure resolves to the shared
// deterministic placeholder so presentation can never be wedged by a hostile
// icon. The intent members evaluate AND revalidate the target against live
// registry state internally; a non-accepted RegistryOutcome means nothing was
// dispatched, and `reasonCode` names the refusal. acknowledgeDegraded() is the
// sole seam mutation that is not a generation-fenced item intent: it clears a
// pending registry degradation marker (the status-tray contract's
// acknowledgement transition) and nothing else.
class StatusNotifierSourceInterface : public QObject {
    Q_OBJECT

public:
    explicit StatusNotifierSourceInterface(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~StatusNotifierSourceInterface() override = default;

    [[nodiscard]] virtual QindaQt::StatusNotifier::TrayPresentation presentation() const = 0;
    [[nodiscard]] virtual QList<QindaQt::StatusNotifier::ItemDescriptor> itemDescriptors() const = 0;
    [[nodiscard]] virtual QImage renderIcon(const QindaQt::StatusNotifier::OwnerKey &key,
                                            int size) const = 0;
    // Current registry generation of the owner, 0 when the owner is not live.
    [[nodiscard]] virtual quint64 currentGeneration(const QString &uniqueName) const = 0;

    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome activate(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y) = 0;
    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome secondaryActivate(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y) = 0;
    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome contextMenu(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y) = 0;

    // Optional exported-menu extension. Defaults preserve legacy sources.
    // Reads are GUI-thread snapshots. menuState returns status none/loading/
    // ready/error, revision as an exact decimal string, and recursive entries:
    // id(int), kind(action/submenu/separator), label, enabled, separator,
    // checkable, checked, radio, iconName, shortcut, children. Invisible nodes
    // are omitted and disabled ancestors disable descendants. Consumers retain
    // the captured owner key and revision; only ready state permits invocation.
    // openMenu prepares root 0 or falls back to legacy ContextMenu only when
    // no export is advertised. AboutToShow accepts existing enabled submenus;
    // invokeMenu accepts only existing enabled leaf actions, sends clicked once
    // without retry, and consumes the captured revision before notification.
    [[nodiscard]] virtual bool itemIsMenu(const QindaQt::StatusNotifier::OwnerKey &) const
    { return false; }
    [[nodiscard]] virtual bool hasExportedMenu(const QindaQt::StatusNotifier::OwnerKey &) const
    { return false; }
    [[nodiscard]] virtual QVariantMap menuState(const QindaQt::StatusNotifier::OwnerKey &) const
    {
        return {{QStringLiteral("status"), QStringLiteral("none")},
                {QStringLiteral("revision"), QStringLiteral("0")},
                {QStringLiteral("entries"), QVariantList{}}};
    }
    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome openMenu(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y)
    { return contextMenu(target, x, y); }
    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome aboutToShowMenu(
        const QindaQt::StatusNotifier::OwnerKey &, quint64, int)
    { return {QindaQt::StatusNotifier::RegistryStatus::InvalidRequest, QStringLiteral("menu-unavailable")}; }
    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome invokeMenu(
        const QindaQt::StatusNotifier::OwnerKey &, quint64, int)
    { return {QindaQt::StatusNotifier::RegistryStatus::InvalidRequest, QStringLiteral("menu-unavailable")}; }
    [[nodiscard]] virtual QindaQt::StatusNotifier::RegistryOutcome scroll(
        const QindaQt::StatusNotifier::OwnerKey &, int, const QString &)
    { return {QindaQt::StatusNotifier::RegistryStatus::InvalidRequest, QStringLiteral("scroll-unavailable")}; }

    // AGENT-CONTRACT: the only degradation-recovery transition at this
    // boundary (docs/wiki/shell/status-tray.md). Implementations clear the
    // registry's degradation marker when one is pending and emit changed()
    // when presentation could have moved; with no degradation pending this
    // is a no-op and must NOT notify. It performs no D-Bus work and never
    // touches item membership, so a retained last-known-good row survives it.
    virtual void acknowledgeDegraded() = 0;

Q_SIGNALS:
    // Menu changes do not recreate item delegates or discard menu focus.
    void menuChanged();
    // Emitted after every registry-affecting event, on the GUI thread —
    // including REJECTED outcomes that still moved registry presentation
    // (e.g. a malformed live replacement or a capacity rejection that set
    // the registry's degradation marker). A refused event that left
    // presentation untouched never notifies. The S1 monitor deliberately
    // exposes no registry-change signal of its own; the seam owns change
    // notification for its consumers.
    void changed();
};

} // namespace QindaQt::StatusNotifierApplet
