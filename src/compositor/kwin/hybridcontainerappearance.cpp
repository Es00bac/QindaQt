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

QString HybridContainerAppearanceStore::displayName(const QString &containerId)
{
    const auto overrideName = m_byContainer.constFind(containerId);
    if (overrideName != m_byContainer.cend() && !overrideName->name.isEmpty()) {
        return overrideName->name;
    }
    const auto generated = m_generatedNames.constFind(containerId);
    if (generated != m_generatedNames.cend()) {
        return *generated;
    }
    const auto name = QStringLiteral("Container %1").arg(++m_nameCounter);
    m_generatedNames.insert(containerId, name);
    return name;
}

void HybridContainerAppearanceStore::forgetContainer(
    const QString &containerId) noexcept
{
    m_byContainer.remove(containerId);
    m_generatedNames.remove(containerId);
}

void HybridContainerAppearanceStore::clear() noexcept
{
    m_byContainer.clear();
    m_generatedNames.clear();
}

} // namespace QindaQt::Compositor::KWinIntegration
