// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/tablet_placement_model.h>

#include <qindaqt/services/tablet_devices/tablet_classification.h>
#include <qindaqt/services/tablet_devices/tablet_geometry.h>

#include <QVariantMap>

#include <cmath>
#include <optional>

namespace QindaQt::Apps::SettingsInput {

namespace TD = Services::TabletDevices;
using TD::MappedRotation;
using TD::Rotation;
using TD::TabletArea;
using TD::TabletDeviceSnapshot;
using TD::TabletMapChoice;
using TD::TabletOutputCandidate;
using TD::TabletPlacementIntent;
using TD::TabletPlacementPlan;
using TD::TabletPropertyWrite;

namespace {

QVariantList areaList(const std::optional<TabletArea> &area) {
    return area.has_value() ? area->toVariantList() : QVariantList{};
}

bool flag(const TabletDeviceSnapshot &tool, const QString &name) {
    const QVariant value = tool.properties.value(name);
    return value.typeId() == QMetaType::Bool && value.toBool();
}

} // namespace

TabletPlacementModel::TabletPlacementModel(
    const TD::TabletDevicePort &port, const TD::TabletOutputInventory &outputs,
    TD::TabletMappingStore *store, QObject *parent)
    : QObject(parent), m_port(port), m_outputs(outputs), m_store(store) {
    if (m_store != nullptr) {
        // The session records what it adopted, and another Settings window
        // may record an edit; either way the presented intent follows.
        connect(m_store, &TD::TabletMappingStore::ledgerChanged, this,
                &TabletPlacementModel::refresh);
    }
}

void TabletPlacementModel::setDevice(const TabletDeviceSnapshot &pen,
                                     bool hasPen, const QString &identity,
                                     const QString &name,
                                     TabletMapChoice choice,
                                     const QString &outputName) {
    m_pen = hasPen ? pen : TabletDeviceSnapshot{};
    m_hasPen = hasPen;
    m_identity = identity;
    m_name = name;
    m_choice = choice;
    m_outputName = outputName;
    m_intent = TabletPlacementIntent{};
    mergeRecordedIntent();
    recompute();
    // The switch shows what the device already does: areas that already have
    // matching proportions keep them through later edits.
    m_keepProportions = proportionsMatch();
    Q_EMIT placementChanged();
}

void TabletPlacementModel::clear() {
    setDevice(TabletDeviceSnapshot{}, false, QString(), QString(),
              TabletMapChoice::FollowActiveScreen, QString());
}

void TabletPlacementModel::applyMapping(TabletMapChoice choice,
                                        const QString &outputName) {
    m_choice = choice;
    m_outputName = outputName;
    rereadDevice();
    recompute();
    if (m_hasPen) {
        if (m_penDisplay) {
            (void)execute(TD::planPenDisplayPlacement(m_pen).writes,
                          tr("Rotation"));
        } else {
            // AGENT-GUARD: only a KNOWN screen rotation is written from here.
            // The user asked for a screen, not a rotation, so an unknown one
            // must not be guessed into KWin or into the ledger; the session
            // re-plans once Display1 answers.
            const TabletPlacementPlan plan =
                TD::planDeskTabletPlacement(m_pen, m_intent, m_mapped);
            if (plan.actionable && execute(plan.writes, tr("Rotation"))) {
                const bool adopted = plan.intent != m_intent;
                m_intent = plan.intent;
                if (adopted) {
                    remember(plan.intent);
                }
            }
        }
        recompute();
    }
    Q_EMIT placementChanged();
}

void TabletPlacementModel::refresh() {
    mergeRecordedIntent();
    recompute();
    Q_EMIT placementChanged();
}

void TabletPlacementModel::mergeRecordedIntent() {
    if (!m_hasPen || m_identity.isEmpty() || m_store == nullptr ||
        !m_store->isLoaded()) {
        return;
    }
    const TD::TabletMappingLedger ledger = m_store->ledger();
    if (!ledger.contains(m_identity)) {
        return;
    }
    // Members the ledger holds are authoritative; members it lacks keep what
    // this model knows (an edit the ledger has not confirmed yet).
    const TabletPlacementIntent recorded = ledger.record(m_identity).placement;
    if (recorded.rotation.has_value()) {
        m_intent.rotation = recorded.rotation;
    }
    if (recorded.inputArea.has_value()) {
        m_intent.inputArea = recorded.inputArea;
    }
    if (recorded.outputArea.has_value()) {
        m_intent.outputArea = recorded.outputArea;
    }
}

void TabletPlacementModel::recompute() {
    m_outputList = m_outputs.outputs();
    m_penDisplay = m_hasPen && TD::classifyTablet(m_pen, m_outputList).kind ==
                                   TD::TabletKind::PenDisplay;
    m_mapped = TD::mappedRotation(m_choice, m_outputName, m_outputList);
    m_surface = tabletSurfaceFor(m_choice, m_outputName, m_outputList);
    m_presented = TabletPlacementIntent{};
    if (!m_hasPen) {
        return;
    }
    if (!m_penDisplay) {
        m_presented =
            TD::planDeskTabletPlacement(m_pen, m_intent, presentationRotation())
                .intent;
        return;
    }
    // A pen display's output area is native to its panel; the user sees it
    // turned by the panel's rotation.
    const std::optional<TabletArea> native =
        TD::deviceArea(m_pen, QStringLiteral("outputArea"));
    if (native.has_value()) {
        m_presented.outputArea = TD::rotateArea(*native, seenTurn());
    }
}

MappedRotation TabletPlacementModel::presentationRotation() const {
    // An unknown rotation is presented, and edited, as an unrotated screen.
    // The note says so, and the session re-plans the recorded intent once
    // the rotation is known.
    if (m_mapped.state == MappedRotation::State::Unknown) {
        return MappedRotation{MappedRotation::State::Known, Rotation::None};
    }
    return m_mapped;
}

Rotation TabletPlacementModel::seenTurn() const {
    if (m_penDisplay) {
        return m_mapped.state == MappedRotation::State::Known ? m_mapped.rotation
                                                              : Rotation::None;
    }
    return m_presented.rotation.value_or(Rotation::None);
}

QSizeF TabletPlacementModel::seenTabletSize(Rotation turn) const {
    if (!m_hasPen || !m_pen.hasPhysicalSize()) {
        return QSizeF(0.0, 0.0);
    }
    const double across = m_pen.widthMillimeters();
    const double down = m_pen.heightMillimeters();
    return TD::swapsAxes(turn) ? QSizeF(down, across) : QSizeF(across, down);
}

bool TabletPlacementModel::rotationAvailable() const {
    return m_hasPen && !m_penDisplay &&
           TD::rotationMechanism(m_pen) != TD::RotationMechanism::None;
}

int TabletPlacementModel::rotation() const {
    return TD::rotationDegrees(m_presented.rotation.value_or(Rotation::None));
}

bool TabletPlacementModel::inputAreaAvailable() const {
    return m_hasPen && !m_penDisplay &&
           flag(m_pen, QStringLiteral("supportsInputArea")) &&
           m_presented.inputArea.has_value();
}

bool TabletPlacementModel::outputAreaAvailable() const {
    return m_hasPen && m_presented.outputArea.has_value();
}

QVariantList TabletPlacementModel::inputArea() const {
    return inputAreaAvailable() ? areaList(m_presented.inputArea)
                                : QVariantList{};
}

QVariantList TabletPlacementModel::outputArea() const {
    return areaList(m_presented.outputArea);
}

double TabletPlacementModel::tabletWidth() const {
    return seenTabletSize(seenTurn()).width();
}

double TabletPlacementModel::tabletHeight() const {
    return seenTabletSize(seenTurn()).height();
}

bool TabletPlacementModel::proportionsAvailable() const {
    const QSizeF tablet = seenTabletSize(seenTurn());
    return m_hasPen && !m_penDisplay && m_presented.outputArea.has_value() &&
           tablet.width() > 0.0 && tablet.height() > 0.0 &&
           m_surface.width > 0.0 && m_surface.height > 0.0;
}

double TabletPlacementModel::outputAspectLock() const {
    if (!m_keepProportions || !proportionsAvailable()) {
        return 0.0;
    }
    const QSizeF tablet = seenTabletSize(seenTurn());
    return TD::proportionalOutputAspect(
        m_presented.inputArea.value_or(TabletArea{}), tablet.width(),
        tablet.height(), m_surface.width, m_surface.height);
}

bool TabletPlacementModel::proportionsMatch() const {
    const double aspect = [this] {
        if (!proportionsAvailable()) {
            return 0.0;
        }
        const QSizeF tablet = seenTabletSize(seenTurn());
        return TD::proportionalOutputAspect(
            m_presented.inputArea.value_or(TabletArea{}), tablet.width(),
            tablet.height(), m_surface.width, m_surface.height);
    }();
    if (!(aspect > 0.0) || !m_presented.outputArea.has_value() ||
        !(m_presented.outputArea->height > 0.0)) {
        return false;
    }
    const TabletArea shown = *m_presented.outputArea;
    return std::abs(shown.width / shown.height - aspect) <= aspect * 0.01;
}

TabletPlacementIntent
TabletPlacementModel::keptProportional(TabletPlacementIntent intent) const {
    if (!m_keepProportions || !intent.outputArea.has_value() ||
        !(m_surface.width > 0.0) || !(m_surface.height > 0.0)) {
        return intent;
    }
    // The tablet's shape as seen AFTER this change: a quarter turn swaps it.
    const QSizeF tablet =
        seenTabletSize(intent.rotation.value_or(Rotation::None));
    if (!(tablet.width() > 0.0) || !(tablet.height() > 0.0)) {
        return intent;
    }
    intent.outputArea = TD::proportionalOutputArea(
        intent.inputArea.value_or(TabletArea{}), tablet.width(),
        tablet.height(), *intent.outputArea, m_surface.width,
        m_surface.height);
    return intent;
}

void TabletPlacementModel::setKeepProportions(bool keep) {
    if (keep == m_keepProportions) {
        return;
    }
    m_keepProportions = keep;
    if (keep && proportionsAvailable()) {
        const TabletPlacementIntent next = keptProportional(m_presented);
        if (next != m_presented) {
            (void)commitDesk(next, tr("Screen area"));
            return;
        }
    }
    Q_EMIT placementChanged();
}

bool TabletPlacementModel::setRotation(int degrees) {
    const std::optional<Rotation> turn =
        TD::rotationFromDegrees(((degrees % 360) + 360) % 360);
    if (!rotationAvailable() || !turn.has_value()) {
        Q_EMIT statusReported(tr("This tablet cannot be turned."));
        return false;
    }
    if (m_presented.rotation == turn) {
        return true;
    }
    TabletPlacementIntent next = m_presented;
    next.rotation = turn;
    return commitDesk(keptProportional(next), tr("Rotation"));
}

bool TabletPlacementModel::applyInputArea(double x, double y, double width,
                                          double height) {
    if (!inputAreaAvailable()) {
        Q_EMIT statusReported(tr("This tablet cannot limit the area it uses."));
        return false;
    }
    const std::optional<TabletArea> area = TD::normalizedArea(
        TabletArea{x, y, width, height}, TD::MinimumEditableAreaExtent);
    if (!area.has_value()) {
        Q_EMIT statusReported(
            tr("That area is too small or outside the tablet."));
        return false;
    }
    TabletPlacementIntent next = m_presented;
    next.inputArea = area;
    return commitDesk(keptProportional(next), tr("Tablet area"));
}

bool TabletPlacementModel::applyOutputArea(double x, double y, double width,
                                           double height) {
    if (!outputAreaAvailable()) {
        Q_EMIT statusReported(tr("This tablet reports no screen area."));
        return false;
    }
    const std::optional<TabletArea> area = TD::normalizedArea(
        TabletArea{x, y, width, height}, TD::MinimumEditableAreaExtent);
    if (!area.has_value()) {
        Q_EMIT statusReported(
            tr("That area is too small or outside the screen."));
        return false;
    }
    if (m_penDisplay) {
        return writeNativeOutputArea(*area);
    }
    TabletPlacementIntent next = m_presented;
    next.outputArea = area;
    return commitDesk(keptProportional(next), tr("Screen area"));
}

bool TabletPlacementModel::fitWholeScreen() {
    return applyOutputArea(0.0, 0.0, 1.0, 1.0);
}

bool TabletPlacementModel::keepTabletProportions(double fallbackWidth,
                                                 double fallbackHeight) {
    const QSizeF tablet = seenTabletSize(seenTurn());
    if (!(tablet.width() > 0.0) || !(tablet.height() > 0.0)) {
        Q_EMIT statusReported(tr("This tablet does not report its size, so "
                                 "its proportions cannot be matched."));
        return false;
    }
    const bool surfaceKnown = m_surface.width > 0.0 && m_surface.height > 0.0;
    const TabletArea area = TD::letterboxArea(
        tablet.width() / tablet.height(),
        surfaceKnown ? m_surface.width : fallbackWidth,
        surfaceKnown ? m_surface.height : fallbackHeight);
    return applyOutputArea(area.x, area.y, area.width, area.height);
}

bool TabletPlacementModel::resetAreas() {
    if (m_penDisplay) {
        return fitWholeScreen();
    }
    TabletPlacementIntent next = m_presented;
    if (next.inputArea.has_value()) {
        next.inputArea = TabletArea{};
    }
    if (next.outputArea.has_value()) {
        next.outputArea = TabletArea{};
    }
    return commitDesk(keptProportional(next), tr("Area"));
}

bool TabletPlacementModel::reset() {
    if (!m_hasPen) {
        return true;
    }
    if (m_penDisplay) {
        rereadDevice();
        const bool cleared =
            execute(TD::planPenDisplayPlacement(m_pen).writes, tr("Rotation"));
        const bool fitted = !outputAreaAvailable() || fitWholeScreen();
        recompute();
        Q_EMIT placementChanged();
        return cleared && fitted;
    }
    TabletPlacementIntent next = m_presented;
    next.rotation = Rotation::None;
    if (next.inputArea.has_value()) {
        next.inputArea = TabletArea{};
    }
    if (next.outputArea.has_value()) {
        next.outputArea = TabletArea{};
    }
    return commitDesk(keptProportional(next), tr("Rotation"));
}

bool TabletPlacementModel::commitDesk(const TabletPlacementIntent &next,
                                      const QString &label) {
    if (!m_hasPen) {
        Q_EMIT statusReported(tr("No tablet is selected."));
        return false;
    }
    rereadDevice();
    const TabletPlacementPlan plan =
        TD::planDeskTabletPlacement(m_pen, next, presentationRotation());
    if (!plan.actionable) {
        Q_EMIT statusReported(tr("%1 could not be changed right now.").arg(label));
        return false;
    }
    const bool written = execute(plan.writes, label);
    if (written) {
        m_intent = plan.intent;
        Q_EMIT statusReported(QString());
        remember(plan.intent);
    }
    recompute();
    Q_EMIT placementChanged();
    return written;
}

bool TabletPlacementModel::writeNativeOutputArea(const TabletArea &area) {
    const std::optional<TabletArea> native = TD::normalizedArea(
        TD::rotateArea(area, TD::inverseRotation(seenTurn())));
    if (!native.has_value()) {
        Q_EMIT statusReported(tr("That area is outside the screen."));
        return false;
    }
    const bool written = execute(
        {TabletPropertyWrite{QStringLiteral("outputArea"),
                             native->toVariantList()}},
        tr("Area"));
    if (written) {
        Q_EMIT statusReported(QString());
    }
    recompute();
    Q_EMIT placementChanged();
    return written;
}

bool TabletPlacementModel::execute(const QList<TabletPropertyWrite> &writes,
                                   const QString &label) {
    for (const TabletPropertyWrite &write : writes) {
        QString error;
        if (!m_port.writeProperty(m_pen.deviceId, write.property, write.value,
                                  &error)) {
            Q_EMIT statusReported(
                tr("%1 could not be changed: %2").arg(label, error));
            return false;
        }
        m_pen.properties.insert(write.property, write.value);
    }
    return true;
}

void TabletPlacementModel::rereadDevice() {
    // AGENT-GUARD: plan against the device as it is NOW. The session may have
    // re-planned it since this page opened (a screen turned), and a plan made
    // against the stale snapshot would skip a write it believes is done.
    if (!m_hasPen || m_pen.deviceId.isEmpty()) {
        return;
    }
    TabletDeviceSnapshot fresh;
    QString error;
    if (m_port.device(m_pen.deviceId, &fresh, &error)) {
        m_pen = fresh;
    }
}

void TabletPlacementModel::remember(const TabletPlacementIntent &intent) {
    // AGENT-GUARD: KWin obeying is not the desktop remembering. Without the
    // record the session re-plans from the OLD intent on the next screen
    // change and silently undoes this edit.
    if (m_store == nullptr ||
        !m_store->recordPlacement(m_identity, intent, m_choice, m_outputName,
                                  m_name)) {
        Q_EMIT statusReported(tr("The tablet changed, but the change could "
                                 "not be remembered for next time."));
    }
}

} // namespace QindaQt::Apps::SettingsInput
