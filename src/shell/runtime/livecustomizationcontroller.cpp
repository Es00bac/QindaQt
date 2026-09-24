// SPDX-License-Identifier: GPL-3.0-or-later
#include "livecustomizationcontroller.h"

#include "livecustomizationmodel.h"
#include "settingsroutelauncher.h"

#include "qindaqt/profiles/profile_types.h"
#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/editor_session.h"
#include "qindaqt/shell_customization_editor/live_editor_host.h"

#include <QDebug>
#include <QMetaType>

#include <utility>

namespace QindaQt::Shell {

namespace Model = LiveCustomizationModel;
using ShellCustomizationEditor::DropTarget;
using ShellCustomizationEditor::EditorOutcome;
using ShellCustomizationEditor::LiveEditorHost;

namespace {

DropTarget targetOf(const QString &panelId, const QString &zone, const QString &before = {})
{
    DropTarget target;
    target.panelId = panelId;
    target.zone = zone;
    if (!before.isEmpty()) {
        target.beforeAppletId = before;
    }
    return target;
}

} // namespace

QString LiveCustomizationController::chordSettingsKey()
{
    return QStringLiteral("shell.customization.chord");
}

QString LiveCustomizationController::defaultChord()
{
    return QStringLiteral("meta-right");
}

int LiveCustomizationController::modifiersForChord(const QString &chord)
{
    if (chord == QLatin1String("meta-alt-right")) {
        return static_cast<int>(Qt::MetaModifier | Qt::AltModifier);
    }
    return static_cast<int>(Qt::MetaModifier);
}

LiveCustomizationController::LiveCustomizationController(
    const QVector<Applets::AppletManifest> &manifests, QString userProfileDirectory,
    OutputSource outputs, Services::SettingsClient::SettingsClient *settings,
    SettingsRouteLauncher *routes, QObject *parent)
    : QObject(parent)
    , m_manifests(manifests)
    , m_userProfileDirectory(std::move(userProfileDirectory))
    , m_outputs(std::move(outputs))
    , m_settings(settings)
    , m_routes(routes)
    , m_chord(defaultChord())
{
    if (m_settings != nullptr) {
        connect(m_settings, &Services::SettingsClient::SettingsClient::snapshotChanged, this,
                &LiveCustomizationController::applyChordSnapshot);
        applyChordSnapshot();
    }
}

LiveCustomizationController::~LiveCustomizationController() = default;

void LiveCustomizationController::applyChordSnapshot()
{
    const auto &snapshot = m_settings->snapshot();
    if (!snapshot.has_value()) {
        return;
    }
    const QVariant value = snapshot->values.value(chordSettingsKey());
    // AGENT-GUARD: only the two documented chords are honored; any other
    // string (or a wrong type) keeps the default rather than disabling
    // customization or inventing a binding.
    QString chord = defaultChord();
    if (value.metaType().id() == QMetaType::QString
        && value.toString() == QLatin1String("meta-alt-right")) {
        chord = QStringLiteral("meta-alt-right");
    }
    if (chord != m_chord) {
        m_chord = chord;
        Q_EMIT chordChanged();
    }
}

void LiveCustomizationController::adoptProfile(const Profiles::LayoutProfile &profile)
{
    m_adopted = profile;
    m_hasAdopted = true;
    if (m_host && !m_host->requiresRebuild() && !m_outputsStale) {
        const auto committed = m_host->committedProfile();
        if (committed != nullptr && committed->toJson() == profile.toJson()) {
            // Our own Apply came back through the store watcher: keep the
            // session so Undo still walks the history of this edit run.
            Q_EMIT changed();
            return;
        }
    }
    rebuildHost();
    Q_EMIT changed();
}

void LiveCustomizationController::outputGenerationChanged()
{
    m_outputsStale = true;
    if (m_host) {
        const EditorOutcome outcome = m_host->notifyOutputGenerationChanged();
        Q_UNUSED(outcome);
    }
    // AGENT-GUARD: rebuild here, do not leave the session stale for the lazy
    // ensureHost() path. available() is false while requiresRebuild() holds,
    // and available() is exactly what disables AppletEditHandle's chord
    // TapHandler and PanelLiveCustomization's menu entry points - the only
    // callers that would ever reach ensureHost(). Marking stale without
    // rebuilding therefore deadlocks: one display hotplug killed
    // Meta+right-click customization for the rest of the session, recovering
    // only if a layout-profile adoption happened to rebuild the host.
    // notifyOutputGenerationChanged() has already cancelled any open gesture,
    // so replacing the session now is the same operation adoptProfile() does.
    rebuildHost();
    Q_EMIT changed();
}

void LiveCustomizationController::rebuildHost()
{
    if (!m_hasAdopted) {
        m_host.reset();
        return;
    }
    const QVector<ShellLayout::LogicalOutput> outputs = m_outputs ? m_outputs() : QVector<ShellLayout::LogicalOutput>{};
    if (m_host) {
        m_host->rebuild(m_adopted, outputs);
    } else {
        m_host = std::make_unique<LiveEditorHost>(m_adopted, outputs, m_manifests,
                                                  m_userProfileDirectory);
    }
    m_outputsStale = false;
    m_dropAccepted = false;
    m_dropReason.clear();
    m_dropTarget.clear();
    if (!m_host->ready()) {
        m_statusText = m_host->unavailableReason();
        qWarning().noquote() << "QindaQt shell live customization is unavailable:"
                             << m_statusText;
    }
}

bool LiveCustomizationController::ensureHost()
{
    if (!m_host || m_host->requiresRebuild() || m_outputsStale) {
        rebuildHost();
    }
    if (!m_host || !m_host->ready()) {
        m_statusText = m_host ? m_host->unavailableReason()
                              : QStringLiteral("no layout profile has been adopted yet");
        Q_EMIT changed();
        return false;
    }
    return true;
}

bool LiveCustomizationController::available() const
{
    return m_host && m_host->ready() && !m_host->requiresRebuild();
}

bool LiveCustomizationController::canUndo() const
{
    return available() && m_host->canUndo();
}

bool LiveCustomizationController::canRedo() const
{
    return available() && m_host->canRedo();
}

bool LiveCustomizationController::dragActive() const
{
    return available() && m_host->visualDragActive();
}

const Profiles::PanelSpec *LiveCustomizationController::panel(const QString &panelId) const
{
    const auto *profile = m_host ? m_host->profile() : nullptr;
    return profile ? Model::findPanel(*profile, panelId) : nullptr;
}

// An applet by owner: a panel id, or "@desktop" for the profile's desktop
// applets (the engine's DesktopAppletOwnerId), so the desktop menu can edit
// the desktop-icons settings through the same typed rows.
const Profiles::AppletSpec *LiveCustomizationController::ownedApplet(
    const QString &ownerId, const QString &appletId) const
{
    const auto *profile = m_host ? m_host->profile() : nullptr;
    if (profile == nullptr) {
        return nullptr;
    }
    if (ownerId == ShellCustomization::DesktopAppletOwnerId) {
        for (const auto &applet : profile->desktopApplets) {
            if (applet.id == appletId) {
                return &applet;
            }
        }
        return nullptr;
    }
    const auto *owner = Model::findPanel(*profile, ownerId);
    return owner ? Model::findApplet(*owner, appletId) : nullptr;
}

bool LiveCustomizationController::settle(const QString &action, const EditorOutcome &outcome,
                                         bool applyAfter)
{
    bool ok = outcome.ok;
    QString message = outcome.message;
    if (ok && applyAfter) {
        const EditorOutcome applied = m_host->apply();
        ok = applied.ok;
        message = applied.message;
    }
    m_statusText = ok ? QString() : message;
    if (!ok) {
        qWarning().noquote() << "QindaQt shell live customization" << action << "refused:" << message;
    }
    Q_EMIT actionReported(ok, action, message);
    Q_EMIT changed();
    return ok;
}

QString LiveCustomizationController::appletDisplayName(const QString &panelId,
                                                       const QString &appletId) const
{
    const auto *applet = ownedApplet(panelId, appletId);
    if (applet == nullptr) {
        return {};
    }
    const auto *manifest = Model::findManifest(m_manifests, applet->plugin);
    return manifest && !manifest->name.isEmpty() ? manifest->name : applet->plugin;
}

bool LiveCustomizationController::moveAppletToZone(const QString &panelId,
                                                   const QString &appletId,
                                                   const QString &zone)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *owner = panel(panelId);
    const auto move = owner ? Model::zoneMove(*owner, appletId, zone) : std::nullopt;
    if (!move.has_value()) {
        m_statusText = QStringLiteral("the applet or zone is unknown");
        Q_EMIT changed();
        return false;
    }
    const auto *applet = Model::findApplet(*owner, appletId);
    if (Model::appletZone(*applet) == zone && move->flatOrderUnchanged) {
        return settle(QStringLiteral("move-to-zone"), EditorOutcome::success(), false);
    }
    if (move->flatOrderUnchanged) {
        // Only the zone tag changes: the whole settings map with zone replaced,
        // exactly what the Settings route's zone companion command carries.
        QVariantMap settings = applet->settings;
        settings.insert(QStringLiteral("zone"), zone);
        return settle(QStringLiteral("move-to-zone"),
                      m_host->applyGesture(
                          ShellCustomizationEditor::configureAppletSettingsIntent(
                              panelId, appletId, settings),
                          move->target),
                      true);
    }
    return settle(QStringLiteral("move-to-zone"),
                  m_host->applyGesture(
                      ShellCustomizationEditor::MoveAppletIntent{panelId, appletId},
                      move->target),
                  true);
}

bool LiveCustomizationController::moveAppletStep(const QString &panelId,
                                                 const QString &appletId, int delta)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *owner = panel(panelId);
    const auto target = owner ? Model::stepMove(*owner, appletId, delta) : std::nullopt;
    if (!target.has_value()) {
        m_statusText = QStringLiteral("the applet is already at the edge of its zone");
        Q_EMIT changed();
        return false;
    }
    return settle(delta < 0 ? QStringLiteral("move-left") : QStringLiteral("move-right"),
                  m_host->applyGesture(
                      ShellCustomizationEditor::MoveAppletIntent{panelId, appletId}, *target),
                  true);
}

bool LiveCustomizationController::moveAppletToPanel(const QString &panelId,
                                                    const QString &appletId,
                                                    const QString &targetPanelId)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *profile = m_host->profile();
    const auto target = Model::panelMove(*profile, panelId, appletId, targetPanelId);
    if (!target.has_value()) {
        m_statusText = QStringLiteral("the target panel is unknown");
        Q_EMIT changed();
        return false;
    }
    return settle(QStringLiteral("move-to-panel"),
                  m_host->applyGesture(
                      ShellCustomizationEditor::MoveAppletIntent{panelId, appletId}, *target),
                  true);
}

bool LiveCustomizationController::removeApplet(const QString &panelId, const QString &appletId)
{
    if (!ensureHost()) {
        return false;
    }
    return settle(QStringLiteral("remove-applet"),
                  m_host->applyGesture(ShellCustomizationEditor::removeIntent(panelId, appletId),
                                       targetOf(panelId, QStringLiteral("start"))),
                  true);
}

bool LiveCustomizationController::duplicateApplet(const QString &panelId,
                                                  const QString &appletId)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *applet = ownedApplet(panelId, appletId);
    if (applet == nullptr) {
        m_statusText = QStringLiteral("the applet is unknown");
        Q_EMIT changed();
        return false;
    }
    // The same target the Settings route's Duplicate used: the applet's own
    // zone with no anchor, so the copy lands at the end of that zone.
    const QString copyId = Model::nextInstanceId(*m_host->profile(), applet->plugin);
    const QString zone = panelId == ShellCustomization::DesktopAppletOwnerId
        ? QStringLiteral("desktop") : Model::appletZone(*applet);
    return settle(QStringLiteral("duplicate-applet"),
                  m_host->applyGesture(
                      ShellCustomizationEditor::duplicateIntent(panelId, appletId, copyId),
                      targetOf(panelId, zone), copyId),
                  true);
}

QVariantList LiveCustomizationController::appletSettingRows(const QString &panelId,
                                                            const QString &appletId) const
{
    const auto *applet = ownedApplet(panelId, appletId);
    const auto *manifest = applet ? Model::findManifest(m_manifests, applet->plugin) : nullptr;
    if (manifest == nullptr) {
        return {};
    }
    return Model::appletSettingRows(*manifest, applet->settings);
}

bool LiveCustomizationController::setAppletSetting(const QString &panelId,
                                                   const QString &appletId,
                                                   const QString &key, const QVariant &value)
{
    if (!ensureHost()) {
        return false;
    }
    const auto *applet = ownedApplet(panelId, appletId);
    const auto *manifest = applet ? Model::findManifest(m_manifests, applet->plugin) : nullptr;
    if (manifest == nullptr) {
        m_statusText = QStringLiteral("this applet's manifest is unavailable");
        Q_EMIT changed();
        return false;
    }
    const auto validation = Model::validateAppletSetting(manifest->settingsSchema, key, value);
    if (!validation.ok()) {
        m_statusText = validation.error;
        Q_EMIT changed();
        return false;
    }
    QVariantMap settings = applet->settings;
    settings.insert(key, validation.value);
    return settle(QStringLiteral("applet-setting"),
                  m_host->applyGesture(
                      ShellCustomizationEditor::configureAppletSettingsIntent(panelId, appletId,
                                                                               settings),
                      targetOf(panelId, panelId == ShellCustomization::DesktopAppletOwnerId
                                            ? QStringLiteral("desktop")
                                            : Model::appletZone(*applet))),
                  true);
}

bool LiveCustomizationController::undo()
{
    if (!ensureHost()) {
        return false;
    }
    return settle(QStringLiteral("undo"), m_host->undo(), true);
}

bool LiveCustomizationController::redo()
{
    if (!ensureHost()) {
        return false;
    }
    return settle(QStringLiteral("redo"), m_host->redo(), true);
}

void LiveCustomizationController::enterEditMode()
{
    if (m_editMode) {
        return;
    }
    m_editMode = true;
    Q_EMIT editModeChanged();
}

void LiveCustomizationController::exitEditMode()
{
    if (!m_editMode) {
        return;
    }
    if (dragActive()) {
        const bool cancelled = cancelDrag();
        Q_UNUSED(cancelled);
    }
    m_editMode = false;
    Q_EMIT editModeChanged();
}

void LiveCustomizationController::toggleEditMode()
{
    if (m_editMode) {
        exitEditMode();
    } else {
        enterEditMode();
    }
}

bool LiveCustomizationController::openCustomize()
{
    return m_routes != nullptr && m_routes->openCustomize();
}

bool LiveCustomizationController::openWallpaperSettings()
{
    return m_routes != nullptr && m_routes->openRoute(QStringLiteral("appearance"));
}

} // namespace QindaQt::Shell
