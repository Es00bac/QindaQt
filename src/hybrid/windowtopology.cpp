// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/hybrid/windowtopology.h>

#include <algorithm>
#include <utility>

namespace QindaQt::Hybrid {
namespace {

void assignError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

void appendNodeWindows(const Core::LayoutNode &node, QStringList &windowIds)
{
    if (node.isLeaf()) {
        windowIds.append(node.windowId());
        return;
    }
    appendNodeWindows(*node.firstChild(), windowIds);
    appendNodeWindows(*node.secondChild(), windowIds);
}

QStringList containerWindows(const Core::WindowContainer &container)
{
    QStringList result;
    for (const auto &page : container.pages()) {
        appendNodeWindows(page.root(), result);
    }
    return result;
}

} // namespace

std::optional<WindowTopology> WindowTopology::create(
    QStringList independentWindowIds,
    QVector<Core::WindowContainer> containers,
    quint64 revision,
    QString *error)
{
    WindowTopology candidate;
    candidate.m_revision = revision;
    for (QString &windowId : independentWindowIds) {
        if (candidate.m_independentWindows.contains(windowId)) {
            assignError(error,
                        QStringLiteral("duplicate independent window ID '%1'").arg(windowId));
            return std::nullopt;
        }
        candidate.m_independentWindows.insert(std::move(windowId));
    }
    for (Core::WindowContainer &container : containers) {
        if (candidate.m_containers.contains(container.id())) {
            assignError(error,
                        QStringLiteral("duplicate container ID '%1'").arg(container.id()));
            return std::nullopt;
        }
        candidate.m_containers.insert(container.id(), std::move(container));
    }

    const auto validation = candidate.validate();
    if (!validation.valid) {
        assignError(error, validation.message);
        return std::nullopt;
    }
    return candidate;
}

QStringList WindowTopology::independentWindowIds() const
{
    QStringList result = m_independentWindows.values();
    std::sort(result.begin(), result.end());
    return result;
}

const Core::WindowContainer *WindowTopology::container(const QString &containerId) const noexcept
{
    const auto match = m_containers.constFind(containerId);
    return match == m_containers.cend() ? nullptr : &match.value();
}

bool WindowTopology::isIndependent(const QString &windowId) const noexcept
{
    return m_independentWindows.contains(windowId);
}

std::optional<QString> WindowTopology::ownerOf(const QString &windowId) const
{
    for (auto match = m_containers.cbegin(); match != m_containers.cend(); ++match) {
        if (match.value().findWindow(windowId)) {
            return match.key();
        }
    }
    return std::nullopt;
}

QStringList WindowTopology::windowIds(const QString &containerId) const
{
    const auto *match = container(containerId);
    return match ? containerWindows(*match) : QStringList{};
}

Core::ValidationResult WindowTopology::validate() const
{
    QSet<QString> ownedWindows;
    for (const QString &windowId : m_independentWindows) {
        if (windowId.isEmpty()) {
            return Core::ValidationResult::failure(
                QStringLiteral("independent window ID is empty"));
        }
        ownedWindows.insert(windowId);
    }

    for (auto match = m_containers.cbegin(); match != m_containers.cend(); ++match) {
        const auto &container = match.value();
        if (match.key() != container.id()) {
            return Core::ValidationResult::failure(
                QStringLiteral("container map key does not match ID '%1'").arg(container.id()));
        }
        const auto containerValidation = container.validate();
        if (!containerValidation.valid) {
            return Core::ValidationResult::failure(
                QStringLiteral("container '%1': %2").arg(container.id(),
                                                       containerValidation.message));
        }
        if (container.pages().isEmpty() || container.singleWindowId().has_value()) {
            return Core::ValidationResult::failure(
                QStringLiteral("container '%1' must have at least two members")
                    .arg(container.id()));
        }

        for (const QString &windowId : containerWindows(container)) {
            if (ownedWindows.contains(windowId)) {
                return Core::ValidationResult::failure(
                    QStringLiteral("window '%1' has duplicate topology ownership")
                        .arg(windowId));
            }
            ownedWindows.insert(windowId);
        }
    }
    return Core::ValidationResult::success();
}

void WindowTopology::swap(WindowTopology &other) noexcept
{
    std::swap(m_revision, other.m_revision);
    m_containers.swap(other.m_containers);
    m_independentWindows.swap(other.m_independentWindows);
}

TopologyRepository::TopologyRepository(WindowTopology initial)
    : m_topology(std::move(initial))
{
    Q_ASSERT(m_topology.validate().valid);
}

void TopologyRepository::publish(WindowTopology &&candidate) noexcept
{
    // AGENT-GUARD: Coordinator validation and scene commit happen before this
    // no-fail swap. Adding fallible work here could leave scene and model at
    // different revisions. See architecture/hybrid-topology.md.
    m_topology.swap(candidate);
}

} // namespace QindaQt::Hybrid
