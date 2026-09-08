// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"

#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_model.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h"

#include <QtCore/QBuffer>
#include <QtCore/QScopeGuard>

namespace QindaQt::StatusNotifierApplet {

namespace {

// AGENT-GUARD: icon sizes never exceed the shared S1 512-pixel ceiling, so a
// hostile or buggy iconSize write cannot turn row enrichment into an
// unbounded render.
constexpr int kMaxAppletIconSize =
    int(QindaQt::StatusNotifier::kMaxIconPixmapDimension);

// AGENT-CONTRACT: the only icon boundary into QML. Rendered images cross as a
// bounded PNG data URL; QML never sees file paths, theme roots, or pixmap
// payloads. The S1 renderer clamps every image to the shared 512-pixel
// ceiling, so the encoded payload stays bounded too.
[[nodiscard]] QString encodePngDataUrl(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

} // namespace

StatusNotifierAppletController::StatusNotifierAppletController(
    StatusNotifierSourceInterface *source,
    bool readGranted,
    bool activateGranted,
    int iconSize,
    QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_readGranted(readGranted)
    , m_activateGranted(activateGranted)
    , m_iconSize(qBound(1, iconSize, kMaxAppletIconSize))
{
    if (m_source) {
        connect(m_source, &StatusNotifierSourceInterface::changed,
                this, &StatusNotifierAppletController::reproject);
    }
    if (m_source && m_readGranted) {
        connect(m_source, &StatusNotifierSourceInterface::menuChanged,
                this, &StatusNotifierAppletController::menuChanged);
    }
    reproject();
}

QString StatusNotifierAppletController::phaseText() const noexcept
{
    return phaseToString(m_projection.phase);
}

QString StatusNotifierAppletController::phaseReasonText() const
{
    return m_projection.phaseReason;
}

bool StatusNotifierAppletController::watcherLive() const noexcept
{
    return m_projection.phase == AppletPhase::Loading
        || m_projection.phase == AppletPhase::Ready
        || m_projection.phase == AppletPhase::Empty;
}

QVariantList StatusNotifierAppletController::itemRows() const
{
    return m_itemRows;
}

int StatusNotifierAppletController::itemCount() const noexcept
{
    return m_projection.presentedCount + m_projection.overflowCount;
}

int StatusNotifierAppletController::presentedCount() const noexcept
{
    return m_projection.presentedCount;
}

int StatusNotifierAppletController::overflowCount() const noexcept
{
    return m_projection.overflowCount;
}

QString StatusNotifierAppletController::overflowText() const
{
    return m_projection.overflowText;
}

void StatusNotifierAppletController::setIconSize(int size)
{
    const int clamped = qBound(1, size, kMaxAppletIconSize);
    if (m_iconSize == clamped) {
        return;
    }
    m_iconSize = clamped;
    reproject();
}

void StatusNotifierAppletController::clearFeedback()
{
    if (!m_feedbackPresent && m_feedback.isEmpty()) {
        return;
    }
    m_feedbackPresent = false;
    m_feedback.clear();
    m_feedbackStatus = QStringLiteral("error");
    Q_EMIT feedbackChanged();
}

void StatusNotifierAppletController::acknowledgeDegraded()
{
    // Fail-closed: with observation withheld there is no degradation truth to
    // acknowledge, and the seam must not be touched at all (same rule as the
    // read path); with no source composed there is nothing to acknowledge.
    if (!m_readGranted || m_source == nullptr) {
        return;
    }
    // The seam emits changed() when it cleared a pending degradation; the
    // direct connection reprojects synchronously, so no extra reproject here.
    m_source->acknowledgeDegraded();
}

void StatusNotifierAppletController::reproject()
{
    // AGENT-GUARD: read-denied withholding. With the read grant denied the
    // source is never even queried: no presentation read, no descriptor read,
    // no icon render. Observation withheld means withheld.
    if (!m_readGranted) {
        m_projection = StatusNotifierAppletModel::project({}, {}, false, m_texts);
        m_itemRows.clear();
    } else if (!m_source) {
        m_projection = {};
        m_projection.phase = AppletPhase::Unavailable;
        m_projection.phaseReason = QLatin1String(kReasonSourceUnavailable);
        m_itemRows.clear();
    } else {
        m_projection = StatusNotifierAppletModel::project(
            m_source->presentation(), m_source->itemDescriptors(), true, m_texts);

        // Icon enrichment for presented rows only; the kMaxPresentedItems cap
        // bounds how many seam renders one reprojection can issue.
        const QImage fallback = QindaQt::StatusNotifier::StatusNotifierIconRenderer::fallbackIcon(
            m_iconSize);
        m_itemRows.clear();
        m_itemRows.reserve(m_projection.rows.size());
        for (const auto &row : m_projection.rows) {
            StatusNotifierItemRow enriched = row;
            const QindaQt::StatusNotifier::OwnerKey key {
                row.uniqueName, row.objectPath, row.generation
            };
            QImage image = m_source->renderIcon(key, m_iconSize);
            // The seam contract promises a non-null image; a violating seam
            // still cannot push a null into presentation.
            if (image.isNull()) {
                image = fallback;
            }
            enriched.iconIsPlaceholder = (image == fallback);
            enriched.iconDataUrl = encodePngDataUrl(image);
            m_itemRows.append(QVariant::fromValue(enriched));
        }
    }
    Q_EMIT stateReprojected();
}

void StatusNotifierAppletController::setFeedback(const QString &message, const QString &status)
{
    m_feedbackPresent = true;
    m_feedback = message;
    m_feedbackStatus = status;
    Q_EMIT feedbackChanged();
}

const StatusNotifierItemRow *StatusNotifierAppletController::findRow(
    const QString &uniqueName,
    const QString &objectPath,
    quint64 generation) const
{
    for (const auto &row : m_projection.rows) {
        if (row.uniqueName == uniqueName && row.objectPath == objectPath
            && row.generation == generation) {
            return &row;
        }
    }
    return nullptr;
}

bool StatusNotifierAppletController::liveTarget(const QString &uniqueName,
                                                const QString &objectPath,
                                                quint64 generation) const
{
    return m_readGranted && m_source && generation != 0
        && findRow(uniqueName, objectPath, generation)
        && m_source->currentGeneration(uniqueName) == generation;
}

bool StatusNotifierAppletController::admitAction(const QString &uniqueName,
                                                 const QString &objectPath,
                                                 quint64 generation)
{
    if (!m_activateGranted) {
        setFeedback(m_texts.feedbackActivateNotGranted);
        return false;
    }
    // Generation fencing is local AND delegated: the row must be presented at
    // the exact generation and the seam must still consider the owner live at
    // that generation before anything dispatches. Generation 0 is never live
    // truth. The seam revalidates again internally before sending.
    const StatusNotifierItemRow *row = findRow(uniqueName, objectPath, generation);
    if (m_source == nullptr || row == nullptr || generation == 0
        || m_source->currentGeneration(uniqueName) != generation) {
        setFeedback(m_texts.feedbackStaleItem);
        return false;
    }
    if (m_dispatchInProgress) {
        setFeedback(m_texts.feedbackBusy);
        return false;
    }
    return true;
}

bool StatusNotifierAppletController::dispatchIntent(IntentKind kind,
                                                    const QString &uniqueName,
                                                    const QString &objectPath,
                                                    quint64 generation, int x, int y)
{
    if (!admitAction(uniqueName, objectPath, generation)) {
        return false;
    }
    m_dispatchInProgress = true;
    const auto guard = qScopeGuard([this] { m_dispatchInProgress = false; });

    // One seam call per gesture, with the item anchor in global logical coordinates.
    const QindaQt::StatusNotifier::OwnerKey target { uniqueName, objectPath, generation };
    QindaQt::StatusNotifier::RegistryOutcome outcome;
    switch (kind) {
    case IntentKind::Activate:
        outcome = m_source->activate(target, x, y);
        break;
    case IntentKind::SecondaryActivate:
        outcome = m_source->secondaryActivate(target, x, y);
        break;
    case IntentKind::ContextMenu:
        outcome = m_source->openMenu(target, x, y);
        break;
    }
    if (!outcome.accepted()) {
        setFeedback(m_texts.feedbackRefused.arg(outcome.reasonCode));
        return false;
    }
    return true;
}

bool StatusNotifierAppletController::activateItem(const QString &uniqueName,
                                                  const QString &objectPath,
                                                  quint64 generation, int x, int y)
{
    return dispatchIntent(IntentKind::Activate, uniqueName, objectPath, generation, x, y);
}

bool StatusNotifierAppletController::secondaryActivateItem(const QString &uniqueName,
                                                           const QString &objectPath,
                                                           quint64 generation, int x, int y)
{
    return dispatchIntent(IntentKind::SecondaryActivate, uniqueName, objectPath, generation, x, y);
}

bool StatusNotifierAppletController::openContextMenu(const QString &uniqueName,
                                                     const QString &objectPath,
                                                     quint64 generation, int x, int y)
{
    return dispatchIntent(IntentKind::ContextMenu, uniqueName, objectPath, generation, x, y);
}

QVariantList StatusNotifierAppletController::menuRowsFor(const QString &uniqueName,
                                                         const QString &objectPath,
                                                         quint64 generation)
{
    QVariantList out;
    if (!m_source || !m_readGranted) {
        return out;
    }
    const StatusNotifierItemRow *row = findRow(uniqueName, objectPath, generation);
    if (row == nullptr || generation == 0
        || m_source->currentGeneration(uniqueName) != generation) {
        return out; // Stale or vanished target: empty preview, no dispatch.
    }
    const auto descriptors = m_source->itemDescriptors();
    for (const auto &descriptor : descriptors) {
        if (descriptor.identity == row->identity) {
            const auto menuRows = StatusNotifierAppletModel::projectMenu(descriptor.menu);
            out.reserve(menuRows.size());
            for (const auto &menuRow : menuRows) {
                out.append(QVariant::fromValue(menuRow));
            }
            return out;
        }
    }
    return out;
}

} // namespace QindaQt::StatusNotifierApplet
