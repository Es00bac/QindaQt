// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_input/tablet_surface.h>
#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>
#include <qindaqt/services/tablet_devices/tablet_placement.h>

#include <QObject>
#include <QSizeF>
#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::SettingsInput {

// How the selected tablet is turned and which part of it reaches which part
// of the screen (ADR-0285), presented in the frames the user sees: the tablet
// as it lies on the desk and the screen as it appears. It knows whether the
// tablet is a pen display (which turns with its screen and has no rotation of
// its own) or a desk tablet (whose rotation and areas are the user's).
//
// AGENT-CONTRACT: every change is planned by the same pure planner the
// session policy runs (Services::TabletDevices::planDeskTabletPlacement) and
// written through the port, so Settings and the session can never compute two
// different values for one device. KWin is written first and the intent
// recorded second; success alone moves what is presented. A refused write is
// reported through statusReported() and the model keeps presenting what the
// device holds. An intent that cannot be recorded still changes the device,
// and the status says it will not be remembered.
//
// Lifetime and threading: `port`, `outputs` and `store` are borrowed, live on
// this object's thread and outlive it; `store` may be null (degraded).
class TabletPlacementModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool penDisplay READ penDisplay NOTIFY placementChanged)
    Q_PROPERTY(bool rotationAvailable READ rotationAvailable NOTIFY
                   placementChanged)
    // Degrees clockwise the tablet is turned on the desk, relative to the
    // screen's up.
    Q_PROPERTY(int rotation READ rotation NOTIFY placementChanged)
    Q_PROPERTY(QString rotationNote READ rotationNote NOTIFY placementChanged)
    Q_PROPERTY(QString followNote READ followNote NOTIFY placementChanged)
    Q_PROPERTY(bool inputAreaAvailable READ inputAreaAvailable NOTIFY
                   placementChanged)
    Q_PROPERTY(bool outputAreaAvailable READ outputAreaAvailable NOTIFY
                   placementChanged)
    // [x, y, width, height] normalized; empty when unavailable.
    Q_PROPERTY(QVariantList inputArea READ inputArea NOTIFY placementChanged)
    Q_PROPERTY(QVariantList outputArea READ outputArea NOTIFY placementChanged)
    // The tablet as the user sees it, in millimetres; 0 when unknown.
    Q_PROPERTY(double tabletWidth READ tabletWidth NOTIFY placementChanged)
    Q_PROPERTY(double tabletHeight READ tabletHeight NOTIFY placementChanged)
    // The mapped surface in logical pixels; 0 when unknown.
    Q_PROPERTY(double surfaceWidth READ surfaceWidth NOTIFY placementChanged)
    Q_PROPERTY(double surfaceHeight READ surfaceHeight NOTIFY placementChanged)
    // [{x, y, width, height, label}] normalized to the surface.
    Q_PROPERTY(QVariantList surfaceScreens READ surfaceScreens NOTIFY
                   placementChanged)
    Q_PROPERTY(QString surfaceLabel READ surfaceLabel NOTIFY placementChanged)
    Q_PROPERTY(bool proportionsAvailable READ proportionsAvailable NOTIFY
                   placementChanged)
    Q_PROPERTY(bool keepProportions READ keepProportions WRITE
                   setKeepProportions NOTIFY placementChanged)
    // The normalized width/height the screen rectangle is locked to while
    // proportions are kept; 0 leaves it free.
    Q_PROPERTY(double outputAspectLock READ outputAspectLock NOTIFY
                   placementChanged)

public:
    TabletPlacementModel(
        const Services::TabletDevices::TabletDevicePort &port,
        const Services::TabletDevices::TabletOutputInventory &outputs,
        Services::TabletDevices::TabletMappingStore *store,
        QObject *parent = nullptr);

    // The selection hands over the device and its current mapping. Nothing
    // is written: opening Settings must never move a pen.
    void setDevice(const Services::TabletDevices::TabletDeviceSnapshot &pen,
                   bool hasPen, const QString &identity, const QString &name,
                   Services::TabletDevices::TabletMapChoice choice,
                   const QString &outputName);
    void clear();
    // The mapping just changed: the compensation moves with the screen, so
    // the plan is written again (a known screen rotation only).
    void applyMapping(Services::TabletDevices::TabletMapChoice choice,
                      const QString &outputName);
    // Outputs or the ledger changed: recompute what is presented.
    void refresh();

    [[nodiscard]] bool penDisplay() const { return m_penDisplay; }
    [[nodiscard]] bool rotationAvailable() const;
    [[nodiscard]] int rotation() const;
    [[nodiscard]] QString rotationNote() const;
    [[nodiscard]] QString followNote() const;
    [[nodiscard]] bool inputAreaAvailable() const;
    [[nodiscard]] bool outputAreaAvailable() const;
    [[nodiscard]] QVariantList inputArea() const;
    [[nodiscard]] QVariantList outputArea() const;
    [[nodiscard]] double tabletWidth() const;
    [[nodiscard]] double tabletHeight() const;
    [[nodiscard]] double surfaceWidth() const { return m_surface.width; }
    [[nodiscard]] double surfaceHeight() const { return m_surface.height; }
    [[nodiscard]] QVariantList surfaceScreens() const;
    [[nodiscard]] QString surfaceLabel() const;
    [[nodiscard]] bool proportionsAvailable() const;
    [[nodiscard]] bool keepProportions() const { return m_keepProportions; }
    [[nodiscard]] double outputAspectLock() const;

    void setKeepProportions(bool keep);
    Q_INVOKABLE bool setRotation(int degrees);
    // Rectangles are normalized to the tablet / the surface as SEEN.
    Q_INVOKABLE bool applyInputArea(double x, double y, double width,
                                    double height);
    Q_INVOKABLE bool applyOutputArea(double x, double y, double width,
                                     double height);
    Q_INVOKABLE bool fitWholeScreen();
    // Letterboxes the whole tablet onto the surface. The fallback extents
    // stand in for the surface when no screen geometry is known.
    Q_INVOKABLE bool keepTabletProportions(double fallbackWidth,
                                           double fallbackHeight);
    Q_INVOKABLE bool resetAreas();
    // Upright, the whole tablet onto the whole surface; a pen display also
    // loses any stale rotation.
    Q_INVOKABLE bool reset();

Q_SIGNALS:
    void placementChanged();
    // Empty text clears the status.
    void statusReported(const QString &text);

private:
    void mergeRecordedIntent();
    void recompute();
    [[nodiscard]] Services::TabletDevices::MappedRotation
    presentationRotation() const;
    [[nodiscard]] Services::TabletDevices::Rotation seenTurn() const;
    [[nodiscard]] QSizeF seenTabletSize(
        Services::TabletDevices::Rotation turn) const;
    [[nodiscard]] bool namedOutputPresent() const;
    // The screen rectangle already has the tablet rectangle's proportions
    // (within 1%): the initial state of the "Keep proportions" switch.
    [[nodiscard]] bool proportionsMatch() const;
    [[nodiscard]] Services::TabletDevices::TabletPlacementIntent
    keptProportional(Services::TabletDevices::TabletPlacementIntent intent)
        const;
    void rereadDevice();
    bool commitDesk(const Services::TabletDevices::TabletPlacementIntent &next,
                    const QString &label);
    bool writeNativeOutputArea(const Services::TabletDevices::TabletArea &area);
    bool execute(const QList<Services::TabletDevices::TabletPropertyWrite>
                     &writes,
                 const QString &label);
    void remember(const Services::TabletDevices::TabletPlacementIntent &intent);

    const Services::TabletDevices::TabletDevicePort &m_port;
    const Services::TabletDevices::TabletOutputInventory &m_outputs;
    Services::TabletDevices::TabletMappingStore *m_store = nullptr;

    Services::TabletDevices::TabletDeviceSnapshot m_pen;
    bool m_hasPen = false;
    QString m_identity;
    QString m_name;
    Services::TabletDevices::TabletMapChoice m_choice =
        Services::TabletDevices::TabletMapChoice::FollowActiveScreen;
    QString m_outputName;
    // What the user asked for, as far as this model knows: the ledger's
    // members, overlaid by edits made here that the ledger may not hold yet.
    Services::TabletDevices::TabletPlacementIntent m_intent;
    bool m_keepProportions = false;

    // Derived by recompute(); never written directly.
    QList<Services::TabletDevices::TabletOutputCandidate> m_outputList;
    bool m_penDisplay = false;
    Services::TabletDevices::MappedRotation m_mapped;
    TabletSurface m_surface;
    Services::TabletDevices::TabletPlacementIntent m_presented;
};

} // namespace QindaQt::Apps::SettingsInput
