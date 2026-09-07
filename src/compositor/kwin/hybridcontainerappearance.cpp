// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerappearance.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

} // namespace

bool HybridContainerAppearanceStore::setName(
    const QString &containerId, const QString &name, QString *error)
{
    if (containerId.isEmpty()) {
        return fail(error, QStringLiteral("container ID must not be empty"));
    }
    if (name.trimmed().isEmpty()) {
        m_byContainer[containerId].name.clear();
        return true;
    }
    const QString normalized = Compositor::normalizedContainerName(name);
    if (normalized.isEmpty()) {
        return fail(error,
                    QStringLiteral("container name has unsupported characters "
                                   "or exceeds the length limit"));
    }
    m_byContainer[containerId].name = normalized;
    return true;
}

bool HybridContainerAppearanceStore::setColor(
    const QString &containerId, const QString &colorHex, QString *error)
{
    if (containerId.isEmpty()) {
        return fail(error, QStringLiteral("container ID must not be empty"));
    }
    if (colorHex.trimmed().isEmpty()) {
        m_byContainer[containerId].colorHex.clear();
        return true;
    }
    const QString normalized = Compositor::normalizedContainerColor(colorHex);
    if (normalized.isEmpty()) {
        return fail(error, QStringLiteral("container color must be a '#RRGGBB' value"));
    }
    m_byContainer[containerId].colorHex = normalized;
    return true;
}

Compositor::ContainerAppearance HybridContainerAppearanceStore::appearance(
    const QString &containerId) const
{
    return m_byContainer.value(containerId);
}

void HybridContainerAppearanceStore::forgetContainer(
    const QString &containerId) noexcept
{
    m_byContainer.remove(containerId);
}

void HybridContainerAppearanceStore::clear() noexcept
{
    m_byContainer.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
