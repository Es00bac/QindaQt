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

std::optional<QJsonObject> canonicalState(
    const ShellVisibilitySnapshotCandidate &candidate,
    QString *error)
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
        if (!validIdentifier(output.id) || outputGeometries.contains(output.id)
            || !hasSafeExtent(output.geometry) || !std::isfinite(output.scale)
            || output.scale <= 0.0
            || output.scale > ShellVisibilityWireLimits::MaxOutputScale) {
            fail(error, QStringLiteral("an enabled output is invalid or ambiguous"));
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
    for (const auto &window : windows) {
        const auto outputGeometry = outputGeometries.constFind(window.outputId);
        const auto workspaces = canonicalIdentifiers(window.workspaceIds);
        const auto activities = canonicalIdentifiers(window.activityIds);
        const bool canonicalWorkspaceScope =
            (window.onAllWorkspaces && workspaces && workspaces->isEmpty())
            || (!window.onAllWorkspaces && workspaces && !workspaces->isEmpty());
        if (!validIdentifier(window.id) || windowIds.contains(window.id)
            || outputGeometry == outputGeometries.cend()
            || !hasSafeExtent(window.frameGeometry)
            || !intersects(outputGeometry.value(), window.frameGeometry)
            || !canonicalWorkspaceScope || !activities
            || (window.active && (window.minimized || window.hidden))) {
            fail(error, QStringLiteral("a managed window is invalid or ambiguous"));
            return std::nullopt;
        }
        if (window.active && !activeWindowId.isEmpty()) {
            fail(error,
                 QStringLiteral("multiple active windows cannot form one coherent snapshot"));
            return std::nullopt;
        }
        if (window.active) {
            activeWindowId = window.id;
        }
        windowIds.insert(window.id);
        windowJson.append(
            QJsonObject{{QStringLiteral("id"), window.id},
                        {QStringLiteral("outputId"), window.outputId},
                        {QStringLiteral("frameGeometry"),
                         geometryJson(window.frameGeometry)},
                        {QStringLiteral("workspaceIds"), stringsJson(*workspaces)},
                        {QStringLiteral("onAllWorkspaces"), window.onAllWorkspaces},
                        {QStringLiteral("activityIds"), stringsJson(*activities)},
                        {QStringLiteral("active"), window.active},
                        {QStringLiteral("maximized"), window.maximized},
                        {QStringLiteral("minimized"), window.minimized},
                        {QStringLiteral("hidden"), window.hidden}});
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
    QString *error)
{
    if (!validIdentifier(m_epoch)) {
        fail(error, QStringLiteral("snapshot epoch is not canonical"));
        return ShellVisibilityPublishResult::Rejected;
    }
    const auto state = canonicalState(candidate, error);
    if (!state) {
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
