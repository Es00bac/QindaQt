// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <qqmlintegration.h>

namespace QindaQt::Shell::DesktopSurface {

// Owns the bounded placement of Desktop-directory icons for the WHOLE desktop
// (ADR-0167). Positions are global layout coordinates - the same frame the
// compositor uses to lay outputs side by side - not output-local ones, so one
// icon has exactly one place on the desktop no matter how many outputs are
// connected. The filesystem identity key is supplied by
// DesktopContentsController, so a rename does not detach an icon from the
// position the user chose.
//
// AGENT-CONTRACT: exactly ONE instance serves the whole session.
// DesktopSurfaceController owns it and injects the same object into every
// per-output surface, because those surfaces must observe the same placements
// and because a mutation made while dragging on one output has to be visible
// to the output that adopts the icon on release. A second instance would
// reintroduce the per-output split this replaced, so never construct one
// inside a desktop surface.
//
// Not final: QML_ELEMENT instantiates through QQmlElement.
class DesktopIconLayoutStore : public QObject {
  Q_OBJECT
  QML_ELEMENT

public:
  explicit DesktopIconLayoutStore(QObject *parent = nullptr);
  explicit DesktopIconLayoutStore(QString storagePath,
                                  QObject *parent = nullptr);

  // Global-coordinate placement for one icon identity; empty when the icon has
  // never been placed, which means the caller flows it into its default slot.
  Q_INVOKABLE QVariantMap position(const QString &layoutKey) const;
  Q_INVOKABLE bool setPosition(const QString &layoutKey, qreal x, qreal y);
  // Forgets every placement, so the next layout pass flows all icons into
  // their default slots. This is what "arrange" means on a whole desktop.
  Q_INVOKABLE bool clearAll();

  // True when the on-disk document is the superseded v1 per-output format and
  // has not been migrated yet.
  [[nodiscard]] Q_INVOKABLE bool hasLegacyLayout() const;
  // One-time upgrade of a v1 (per-output, output-local) document to global
  // coordinates. `keptScreenName` names the output whose arrangement survives
  // and `originX`/`originY` its global origin; every other output's placements
  // are dropped, because v1 stored one independent arrangement per output and
  // there is no truthful way to merge two conflicting ones into a single
  // desktop. Callers pass the primary output. Idempotent: a document that is
  // already v2 is left alone and this returns true.
  Q_INVOKABLE bool migrateLegacyLayout(const QString &keptScreenName,
                                       qreal originX, qreal originY);

  // Live, deliberately unsaved positions for a drag in flight. Each per-output
  // surface is its own layer-shell window and cannot paint outside its own
  // output, so a drag that crosses an output boundary is only continuous if
  // the surface that will adopt the icon can draw it while the other surface
  // still holds the pointer grab. These three calls are that channel: the
  // dragging surface publishes positions, every surface reads them, and
  // nothing touches the disk until the drag ends with a real setPosition.
  //
  // AGENT-GUARD: drag positions are volatile by design. Never persist them
  // here and never let them survive endDrag(); a crash mid-drag must leave the
  // saved arrangement exactly as it was.
  Q_INVOKABLE void updateDrag(const QVariantMap &positionsByLayoutKey);
  Q_INVOKABLE void endDrag();
  [[nodiscard]] Q_INVOKABLE QVariantMap dragPosition(const QString &layoutKey) const;
  [[nodiscard]] Q_INVOKABLE bool isDragging() const;

signals:
  // Emitted after any accepted mutation. Per-output surfaces re-read their
  // owned icons from this, which is how an icon dragged across an output
  // boundary appears on the output that now owns it.
  void changed();
  // Emitted for every published drag update and once on endDrag(), so other
  // outputs redraw an icon being dragged toward them.
  void dragChanged();

private:
  void load();
  [[nodiscard]] bool save() const;
  [[nodiscard]] static bool validKey(const QString &value);
  [[nodiscard]] static bool validCoordinate(qreal value);

  QString m_storagePath;
  // layoutKey -> {x, y} in global layout coordinates.
  QVariantMap m_icons;
  // Raw v1 payload (screenName -> layoutKey -> {x, y}) retained unmigrated so
  // a downgrade or a failed migration never destroys the user's arrangement.
  QVariantMap m_legacyScreens;
  // layoutKey -> {x, y} in global layout coordinates, live drag only.
  QVariantMap m_dragPositions;
};

} // namespace QindaQt::Shell::DesktopSurface
