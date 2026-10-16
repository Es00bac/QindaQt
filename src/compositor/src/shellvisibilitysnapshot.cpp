// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/shellvisibilitysnapshot.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringView>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace QindaQt::Compositor {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

bool hasWellFormedUtf16(QStringView text)
{
    for (qsizetype index = 0; index < text.size(); ++index) {
        const auto current = text[index];
        if (current.isHighSurrogate()) {
            if (index + 1 >= text.size() || !text[index + 1].isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (current.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

// Identifiers reach here straight off the wire, so a rejected one may be
// empty, untrimmed, or long. Quote it and bound it: these strings go to the
// session log verbatim.
QString describe(const QString &identifier)
{
    if (identifier.isEmpty()) {
        return QStringLiteral("'<empty>'");
    }
    constexpr qsizetype MaxLoggedCharacters = 64;
    if (identifier.size() > MaxLoggedCharacters) {
        return QStringLiteral("'%1...'").arg(identifier.left(MaxLoggedCharacters));
    }
    return QStringLiteral("'%1'").arg(identifier);
}

QString describeRect(const QRect &rect)
{
    return QStringLiteral("%1x%2%3%4")
        .arg(rect.width())
        .arg(rect.height())
        .arg(rect.x() < 0 ? QString::number(rect.x()) : QStringLiteral("+%1").arg(rect.x()),
             rect.y() < 0 ? QString::number(rect.y()) : QStringLiteral("+%1").arg(rect.y()));
}

bool validIdentifier(const QString &value)
{
    const QStringView view(value);
    return !view.isEmpty()
        && view.size() <= ShellVisibilityWireLimits::MaxIdentifierCharacters
        && view == view.trimmed() && hasWellFormedUtf16(view);
}

bool hasSafeExtent(const QRect &geometry)
{
    if (!geometry.isValid()) {
        return false;
    }
    const qint64 right = qint64(geometry.x()) + geometry.width() - 1;
    const qint64 bottom = qint64(geometry.y()) + geometry.height() - 1;
    return right <= std::numeric_limits<int>::max()
        && bottom <= std::numeric_limits<int>::max();
}

bool intersects(const QRect &left, const QRect &right)
{
    return qint64(left.left()) <= right.right()
        && qint64(right.left()) <= left.right()
        && qint64(left.top()) <= right.bottom()
        && qint64(right.top()) <= left.bottom();
}

QJsonObject geometryJson(const QRect &geometry)
{
    return {{QStringLiteral("x"), geometry.x()},
            {QStringLiteral("y"), geometry.y()},
            {QStringLiteral("width"), geometry.width()},
            {QStringLiteral("height"), geometry.height()}};
}

std::optional<QStringList> canonicalIdentifiers(const QStringList &values)
{
    if (values.size() > ShellVisibilityWireLimits::MaxScopeMemberships) {
        return std::nullopt;
    }
    QSet<QString> unique;
    for (const auto &value : values) {
        if (!validIdentifier(value) || unique.contains(value)) {
            return std::nullopt;
        }
        unique.insert(value);
    }
    auto result = unique.values();
    result.sort();
    return result;
}

QJsonArray stringsJson(const QStringList &values)
{
    QJsonArray result;
    for (const auto &value : values) {
        result.append(value);
    }
    return result;
}

// The output a frame overlaps most, ties broken by the lowest id so the
// choice is deterministic; nullopt when it overlaps none.
std::optional<QString> bestOverlappingOutput(
    const QRect &frame, const QVector<ShellVisibilityOutputSnapshot> &sortedOutputs)
{
    std::optional<QString> best;
    qint64 bestArea = 0;
    for (const auto &output : sortedOutputs) {
        if (!intersects(output.geometry, frame)) {
            continue;
        }
        const QRect overlap = output.geometry.intersected(frame);
        const qint64 area = qint64(overlap.width()) * overlap.height();
        if (!best || area > bestArea) {
            best = output.id;
            bestArea = area;
        }
    }
    return best;
}

void degrade(QVector<ShellVisibilityDegradation> *degradations, const QString &windowId,
             ShellVisibilityDegradationKind kind, QString message)
{
    if (degradations) {
        degradations->append({windowId, kind, std::move(message)});
    }
}

// AGENT-GUARD: every rejection names its member and its reason (ADR-0237);
// a rejection voids the whole batch, so its message is all a reader of the
// log gets. These are producer bugs and must stay batch-fatal. Returns true
// when the window is structurally sound.
bool structurallySound(const ShellVisibilityWindowSnapshot &window,
                       const QSet<QString> &seenIds, QString *error)
{
    const QString windowLabel = describe(window.id);
    if (!validIdentifier(window.id)) {
        return fail(error, QStringLiteral("managed window %1 has a non-canonical identifier")
                               .arg(windowLabel));
    }
    if (seenIds.contains(window.id)) {
        return fail(error, QStringLiteral("managed window %1 is listed more than once")
                               .arg(windowLabel));
    }
    if (!hasSafeExtent(window.frameGeometry)) {
        return fail(error, QStringLiteral("managed window %1 has an unusable frame geometry %2")
                               .arg(windowLabel, describeRect(window.frameGeometry)));
    }
    const auto workspaces = canonicalIdentifiers(window.workspaceIds);
    const bool canonicalWorkspaceScope =
        (window.onAllWorkspaces && workspaces && workspaces->isEmpty())
        || (!window.onAllWorkspaces && workspaces && !workspaces->isEmpty());
    if (!canonicalWorkspaceScope) {
        return fail(error,
                    QStringLiteral("managed window %1 claims onAllWorkspaces=%2 with %3 "
                                   "workspace IDs")
                        .arg(windowLabel,
                             window.onAllWorkspaces ? QStringLiteral("true")
                                                    : QStringLiteral("false"),
                             QString::number(window.workspaceIds.size())));
    }
    if (!canonicalIdentifiers(window.activityIds)) {
        return fail(error, QStringLiteral("managed window %1 has non-canonical activity IDs")
                               .arg(windowLabel));
    }
    return true;
}

// AGENT-NOTE (ADR-0384): the transient races ADR-0237 measured -- a window
// between outputs, stranded by an unplugged output, or active for a moment
// while still minimized or hidden -- degrade only that window. Voiding the
// batch for them disabled auto-hide and the dock for the whole session.
// Returns false when the window must be left out of this generation.
bool repairTransient(ShellVisibilityWindowSnapshot *window,
                     const QHash<QString, QRect> &outputGeometries,
                     const QVector<ShellVisibilityOutputSnapshot> &sortedOutputs,
                     QVector<ShellVisibilityDegradation> *degradations)
{
    const QString windowLabel = describe(window->id);
    const auto outputGeometry = outputGeometries.constFind(window->outputId);
    const bool knownOutput = outputGeometry != outputGeometries.cend();
    if (!knownOutput || !intersects(outputGeometry.value(), window->frameGeometry)) {
        const auto best = bestOverlappingOutput(window->frameGeometry, sortedOutputs);
        if (!best) {
            degrade(degradations, window->id, ShellVisibilityDegradationKind::StrandedExcluded,
                    QStringLiteral("managed window %1 has frame geometry %2, which overlaps "
                                   "no output; left out of this generation")
                        .arg(windowLabel, describeRect(window->frameGeometry)));
            return false;
        }
        if (knownOutput) {
            degrade(degradations, window->id,
                    ShellVisibilityDegradationKind::OutsideOutputReassigned,
                    QStringLiteral("managed window %1 has frame geometry %2, which lies "
                                   "outside its output %3 at %4; published on %5")
                        .arg(windowLabel, describeRect(window->frameGeometry),
                             describe(window->outputId),
                             describeRect(outputGeometry.value()), describe(*best)));
        } else {
            degrade(degradations, window->id,
                    ShellVisibilityDegradationKind::UnknownOutputReassigned,
                    QStringLiteral("managed window %1 names output %2, which this "
                                   "generation does not have; published on %3")
                        .arg(windowLabel, describe(window->outputId), describe(*best)));
        }
        window->outputId = *best;
    }
    if (window->active && (window->minimized || window->hidden)) {
        degrade(degradations, window->id,
                ShellVisibilityDegradationKind::ActiveWhileHiddenCleared,
                QStringLiteral("managed window %1 is active while marked %2; "
                               "published as inactive")
                    .arg(windowLabel, window->minimized ? QStringLiteral("minimized")
                                                        : QStringLiteral("hidden")));
        window->active = false;
    }
    return true;
}

// Only called for a structurally sound window, so the identifier lists are
// known to canonicalize.
QJsonObject publishedWindowJson(const ShellVisibilityWindowSnapshot &window)
{
    return {{QStringLiteral("id"), window.id},
            {QStringLiteral("outputId"), window.outputId},
            {QStringLiteral("frameGeometry"), geometryJson(window.frameGeometry)},
            {QStringLiteral("workspaceIds"),
             stringsJson(canonicalIdentifiers(window.workspaceIds).value_or(QStringList{}))},
            {QStringLiteral("onAllWorkspaces"), window.onAllWorkspaces},
            {QStringLiteral("activityIds"),
             stringsJson(canonicalIdentifiers(window.activityIds).value_or(QStringList{}))},
            {QStringLiteral("active"), window.active},
            {QStringLiteral("maximized"), window.maximized},
            {QStringLiteral("minimized"), window.minimized},
            {QStringLiteral("hidden"), window.hidden},
            {QStringLiteral("fullscreen"), window.fullscreen}};
}

std::optional<QJsonObject> canonicalState(
    const ShellVisibilitySnapshotCandidate &candidate,
    QString *error,
    QVector<ShellVisibilityDegradation> *degradations)
{
    if (candidate.outputGeneration == 0) {
        fail(error, QStringLiteral("output generation must be non-zero"));
        return std::nullopt;
    }
    if (!validIdentifier(candidate.scope.workspaceId)
        || !validIdentifier(candidate.scope.activityId)) {
        fail(error, QStringLiteral("current workspace/activity IDs are not canonical"));
        return std::nullopt;
    }

    if (candidate.outputs.isEmpty()
        || candidate.outputs.size() > ShellVisibilityWireLimits::MaxOutputs) {
        fail(error, QStringLiteral("enabled output count is outside the wire limit"));
        return std::nullopt;
    }
    auto outputs = candidate.outputs;
    std::sort(outputs.begin(), outputs.end(), [](const auto &left, const auto &right) {
        return left.id < right.id;
    });
    QHash<QString, QRect> outputGeometries;
    QJsonArray outputJson;
    for (const auto &output : outputs) {
        // AGENT-GUARD: name the output and the reason it was refused. A
        // rejection voids the whole snapshot, so this message is all a reader
        // gets; a shared one for every cause leaves them nothing to act on.
        const QString outputLabel = describe(output.id);
        if (!validIdentifier(output.id)) {
            fail(error, QStringLiteral("enabled output %1 has a non-canonical identifier")
                            .arg(outputLabel));
            return std::nullopt;
        }
        if (outputGeometries.contains(output.id)) {
            fail(error, QStringLiteral("enabled output %1 is listed more than once")
                            .arg(outputLabel));
            return std::nullopt;
        }
        if (!hasSafeExtent(output.geometry)) {
            fail(error, QStringLiteral("enabled output %1 has an unusable geometry %2")
                            .arg(outputLabel, describeRect(output.geometry)));
            return std::nullopt;
        }
        if (!std::isfinite(output.scale) || output.scale <= 0.0
            || output.scale > ShellVisibilityWireLimits::MaxOutputScale) {
            fail(error, QStringLiteral("enabled output %1 has an out-of-range scale %2")
                            .arg(outputLabel, QString::number(output.scale)));
            return std::nullopt;
        }
        outputGeometries.insert(output.id, output.geometry);
        outputJson.append(QJsonObject{{QStringLiteral("id"), output.id},
                                      {QStringLiteral("geometry"),
                                       geometryJson(output.geometry)},
                                      {QStringLiteral("scale"), output.scale}});
    }
    if (candidate.windows.size() > ShellVisibilityWireLimits::MaxWindows) {
        fail(error, QStringLiteral("managed window count exceeds the wire limit"));
        return std::nullopt;
    }
    auto windows = candidate.windows;
    std::sort(windows.begin(), windows.end(), [](const auto &left, const auto &right) {
        return left.id < right.id;
    });
    QJsonArray windowJson;
    QSet<QString> windowIds;
    QString activeWindowId;
    for (auto window : windows) {
        if (!structurallySound(window, windowIds, error)) {
            return std::nullopt;
        }
        windowIds.insert(window.id);
        if (!repairTransient(&window, outputGeometries, outputs, degradations)) {
            continue;
        }
        if (window.active && !activeWindowId.isEmpty()) {
            fail(error,
                 QStringLiteral("managed windows %1 and %2 are both active, which cannot "
                                "form one coherent snapshot")
                     .arg(describe(activeWindowId), describe(window.id)));
            return std::nullopt;
        }
        if (window.active) {
            activeWindowId = window.id;
        }
        windowJson.append(publishedWindowJson(window));
    }

    return QJsonObject{
        {QStringLiteral("outputGeneration"),
         QString::number(candidate.outputGeneration)},
        {QStringLiteral("scope"),
         QJsonObject{{QStringLiteral("workspaceId"), candidate.scope.workspaceId},
                     {QStringLiteral("activityId"), candidate.scope.activityId}}},
        {QStringLiteral("outputs"), outputJson},
        {QStringLiteral("windows"), windowJson},
    };
}

QByteArray unavailableSnapshot(const QString &epoch,
                               quint64 revision,
                               const QString &code,
                               const QString &message)
{
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("status"), QStringLiteral("unavailable")},
                             {QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("epoch"), epoch},
                             {QStringLiteral("revision"), QString::number(revision)},
                             {QStringLiteral("failure"),
                              QJsonObject{{QStringLiteral("code"),
                                           code},
                                          {QStringLiteral("message"),
                                           message}}},
                         })
        .toJson(QJsonDocument::Compact);
}

} // namespace

ShellVisibilitySnapshotStore::ShellVisibilitySnapshotStore(
    QString epoch,
    ShellVisibilityRevisionSeed revisionSeed)
    : m_snapshotJson(unavailableSnapshot(
          epoch, revisionSeed.value, QStringLiteral("snapshot-unavailable"),
          QStringLiteral("no valid compositor inventory has been published")))
    , m_epoch(std::move(epoch))
    , m_revision(revisionSeed.value)
{
}

ShellVisibilityPublishResult ShellVisibilitySnapshotStore::publish(
    const ShellVisibilitySnapshotCandidate &candidate,
    QString *error,
    QVector<ShellVisibilityDegradation> *degradations)
{
    if (degradations) {
        degradations->clear();
    }
    if (!validIdentifier(m_epoch)) {
        fail(error, QStringLiteral("snapshot epoch is not canonical"));
        return ShellVisibilityPublishResult::Rejected;
    }
    const auto state = canonicalState(candidate, error, degradations);
    if (!state) {
        // A rejected batch publishes nothing, so nothing was degraded either.
        if (degradations) {
            degradations->clear();
        }
        return ShellVisibilityPublishResult::Rejected;
    }
    const auto canonical = QJsonDocument(*state).toJson(QJsonDocument::Compact);
    if (canonical == m_canonicalState && m_available) {
        return ShellVisibilityPublishResult::Unchanged;
    }
    if (m_revision == std::numeric_limits<quint64>::max()) {
        fail(error, QStringLiteral("shell visibility revision is exhausted"));
        return ShellVisibilityPublishResult::RevisionExhausted;
    }

    const quint64 nextRevision = m_revision + 1;
    QJsonObject published = *state;
    published.insert(QStringLiteral("status"), QStringLiteral("ok"));
    published.insert(QStringLiteral("schemaVersion"), 1);
    published.insert(QStringLiteral("epoch"), m_epoch);
    // AGENT-CONTRACT: Compositor1 carries every quint64 as a decimal string;
    // JSON numbers cannot exactly represent the complete revision domain.
    published.insert(QStringLiteral("revision"), QString::number(nextRevision));
    const auto payload = QJsonDocument(published).toJson(QJsonDocument::Compact);
    if (payload.size() > ShellVisibilityWireLimits::MaxPayloadBytes) {
        fail(error, QStringLiteral("shell visibility snapshot exceeds the wire byte limit"));
        return ShellVisibilityPublishResult::Rejected;
    }
    m_revision = nextRevision;
    m_canonicalState = canonical;
    m_snapshotJson = payload;
    m_available = true;
    if (error) {
        error->clear();
    }
    return ShellVisibilityPublishResult::Published;
}

const QByteArray &ShellVisibilitySnapshotStore::snapshotJson() const noexcept
{
    return m_snapshotJson;
}

quint64 ShellVisibilitySnapshotStore::revision() const noexcept
{
    return m_revision;
}

const QString &ShellVisibilitySnapshotStore::epoch() const noexcept
{
    return m_epoch;
}

bool ShellVisibilitySnapshotStore::available() const noexcept
{
    return m_available;
}

bool ShellVisibilitySnapshotStore::markUnavailable(
    const QString &code,
    const QString &message)
{
    if (!m_available) {
        return false;
    }
    m_available = false;
    m_snapshotJson = unavailableSnapshot(m_epoch, m_revision, code, message);
    return true;
}

} // namespace QindaQt::Compositor
