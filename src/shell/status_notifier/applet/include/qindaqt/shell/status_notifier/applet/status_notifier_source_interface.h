// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
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
// dispatched, and `reasonCode` names the refusal.
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

Q_SIGNALS:
    // Emitted after every registry-affecting event, on the GUI thread. The S1
    // monitor deliberately exposes no registry-change signal of its own; the
    // seam owns change notification for its consumers.
    void changed();
};

} // namespace QindaQt::StatusNotifierApplet
