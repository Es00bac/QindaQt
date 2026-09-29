// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_placement_model.h>

#include <QVariantMap>

// What the Pen & tablet page SAYS about a placement: the notes under the
// rotation, the screen a pen display turns with, and the surface the area
// editor draws. Kept apart from the model's state and writes so neither file
// grows past a readable size (ADR-0285).

namespace QindaQt::Apps::SettingsInput {

namespace TD = Services::TabletDevices;
using TD::MappedRotation;
using TD::Rotation;
using TD::TabletMapChoice;
using TD::TabletOutputCandidate;

bool TabletPlacementModel::namedOutputPresent() const {
    if (m_choice != TabletMapChoice::NamedOutput || m_outputName.isEmpty()) {
        return false;
    }
    for (const TabletOutputCandidate &output : m_outputList) {
        if (output.enabled && output.connectorName == m_outputName) {
            return true;
        }
    }
    return false;
}

QString TabletPlacementModel::rotationNote() const {
    if (!rotationAvailable()) {
        return {};
    }
    switch (m_mapped.state) {
    case MappedRotation::State::Known:
        if (m_mapped.rotation == Rotation::None) {
            return {};
        }
        return tr("%1 is rotated %2°, so the pen is turned to match: up on "
                  "the tablet stays up on the screen.")
            .arg(surfaceLabel())
            .arg(TD::rotationDegrees(m_mapped.rotation));
    case MappedRotation::State::Mixed:
        return tr("Your screens are rotated differently, so the pen cannot "
                  "stay upright on every one of them. Map the tablet to one "
                  "screen to keep it upright there.");
    case MappedRotation::State::Unknown:
        return tr("The screen's rotation is not known right now, so the pen "
                  "is not turned to match it.");
    }
    return {};
}

QString TabletPlacementModel::followNote() const {
    if (!m_hasPen || !m_penDisplay) {
        return {};
    }
    if (m_choice == TabletMapChoice::EntireWorkspace) {
        return tr("Spans every screen, so it cannot turn with one. Map it to "
                  "its own screen to follow that screen's rotation.");
    }
    if (namedOutputPresent()) {
        return tr("Turns with %1. Rotate that screen in Displays and the pen "
                  "follows.")
            .arg(surfaceLabel());
    }
    return tr("Turns with whichever screen is active. Map it to its own "
              "screen so it always follows that screen.");
}

QVariantList TabletPlacementModel::surfaceScreens() const {
    QVariantList screens;
    for (const TabletSurfaceScreen &screen : m_surface.screens) {
        screens.append(QVariantMap{
            {QStringLiteral("x"), screen.area.x()},
            {QStringLiteral("y"), screen.area.y()},
            {QStringLiteral("width"), screen.area.width()},
            {QStringLiteral("height"), screen.area.height()},
            {QStringLiteral("label"), screen.label},
        });
    }
    return screens;
}

QString TabletPlacementModel::surfaceLabel() const {
    if (m_choice == TabletMapChoice::EntireWorkspace) {
        return tr("Every screen");
    }
    if (namedOutputPresent()) {
        for (const TabletOutputCandidate &output : m_outputList) {
            if (output.enabled && output.connectorName == m_outputName) {
                return tabletOutputLabel(output);
            }
        }
    }
    return tr("The active screen");
}

} // namespace QindaQt::Apps::SettingsInput
