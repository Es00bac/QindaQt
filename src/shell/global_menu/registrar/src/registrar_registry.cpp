// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <algorithm>
#include <limits>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

namespace
{

bool isUniqueName(const QString &name)
{
    if (!name.startsWith(u':') || name.size() < 4 || name.toUtf8().size() > 255) {
        return false;
    }
    const QStringView body{name.constData() + 1, name.size() - 1};
    int elements = 0;
    qsizetype elementLength = 0;
    for (QChar character : body) {
        if (character == u'.') {
            if (elementLength == 0) {
                return false;
            }
            ++elements;
            elementLength = 0;
            continue;
        }
        const bool allowed = (character.isLetterOrNumber() && character.unicode() < 128)
            || character == u'_' || character == u'-';
        if (!allowed) {
            return false;
        }
        ++elementLength;
    }
    return elementLength > 0 && elements >= 1;
}

bool isObjectPath(const QString &path)
{
    if (!path.startsWith(u'/') || path.toUtf8().size() > kMaxMenuObjectPathUtf8Bytes) {
        return false;
    }
    if (path == QStringLiteral("/")) {
        return true;
    }
    if (path.endsWith(u'/')) {
        return false;
    }
    for (QStringView element : QStringView(path).sliced(1).split(u'/')) {
        if (element.isEmpty()) {
            return false;
        }
        for (QChar character : element) {
            const bool allowed = (character.isLetterOrNumber() && character.unicode() < 128)
                || character == u'_';
            if (!allowed) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

RegistrarRegistry::RegistrarRegistry(QObject *parent)
    : QObject(parent)
{
}

std::optional<quint64> RegistrarRegistry::nextGeneration()
{
    if (m_generationHighWater == std::numeric_limits<quint64>::max()) {
        return std::nullopt;
    }
    return ++m_generationHighWater;
}

RegistrationResult RegistrarRegistry::registerWindow(
    quint32 windowId, const QString &ownerUniqueName, const QDBusObjectPath &menuObjectPath)
{
    if (windowId == 0 || !isUniqueName(ownerUniqueName)
        || !isObjectPath(menuObjectPath.path())) {
        return {.outcome = RegistrationOutcome::Invalid, .registration = std::nullopt};
    }

    const auto existing = m_byWindow.constFind(windowId);
    if (existing != m_byWindow.cend() && existing->ownerUniqueName != ownerUniqueName) {
        return {.outcome = RegistrationOutcome::OwnedByAnotherPeer,
                .registration = std::nullopt};
    }
    if (existing == m_byWindow.cend()
        && m_byWindow.size() >= kMaxRegisteredWindows) {
        return {.outcome = RegistrationOutcome::CapacityExceeded,
                .registration = std::nullopt};
    }
    if (existing != m_byWindow.cend() && existing->menuObjectPath == menuObjectPath) {
        return {.outcome = RegistrationOutcome::Unchanged, .registration = *existing};
    }

    quint64 ownerGenerationValue = 0;
    const auto ownerGenerationIt = m_ownerGenerations.constFind(ownerUniqueName);
    if (ownerGenerationIt == m_ownerGenerations.cend()) {
        const std::optional<quint64> generation = nextGeneration();
        if (!generation) {
            return {.outcome = RegistrationOutcome::GenerationExhausted,
                    .registration = std::nullopt};
        }
        ownerGenerationValue = *generation;
    } else {
        ownerGenerationValue = *ownerGenerationIt;
    }
    const std::optional<quint64> registrationGeneration = nextGeneration();
    if (!registrationGeneration) {
        return {.outcome = RegistrationOutcome::GenerationExhausted,
                .registration = std::nullopt};
    }

    const bool firstForOwner = ownerGenerationIt == m_ownerGenerations.cend();
    const bool replacingWindow = existing != m_byWindow.cend();
    if (firstForOwner) {
        m_ownerGenerations.insert(ownerUniqueName, ownerGenerationValue);
    }
    AppMenuRegistration registration{.windowId = windowId,
                                     .ownerUniqueName = ownerUniqueName,
                                     .menuObjectPath = menuObjectPath,
                                     .ownerGeneration = ownerGenerationValue,
                                     .registrationGeneration = *registrationGeneration};
    m_byWindow.insert(windowId, registration);
    m_windowsByOwner[ownerUniqueName].insert(windowId);
    if (firstForOwner) {
        Q_EMIT ownerBecamePresent(ownerUniqueName, ownerGenerationValue);
    }
    Q_EMIT windowRegistered(registration);
    return {.outcome = replacingWindow ? RegistrationOutcome::Updated
                                        : RegistrationOutcome::Registered,
            .registration = registration};
}

void RegistrarRegistry::eraseRegistration(const AppMenuRegistration &registration)
{
    m_byWindow.remove(registration.windowId);
    auto ownerIt = m_windowsByOwner.find(registration.ownerUniqueName);
    if (ownerIt != m_windowsByOwner.end()) {
        ownerIt->remove(registration.windowId);
        if (ownerIt->isEmpty()) {
            m_windowsByOwner.erase(ownerIt);
            m_ownerGenerations.remove(registration.ownerUniqueName);
            Q_EMIT ownerBecameAbsent(registration.ownerUniqueName);
        }
    }
    Q_EMIT windowUnregistered(registration.windowId, registration.ownerUniqueName);
}

RemovalOutcome RegistrarRegistry::unregisterWindow(quint32 windowId,
                                                   const QString &ownerUniqueName)
{
    const auto existing = m_byWindow.constFind(windowId);
    if (existing == m_byWindow.cend()) {
        return RemovalOutcome::NotFound;
    }
    if (existing->ownerUniqueName != ownerUniqueName) {
        return RemovalOutcome::NotOwner;
    }
    const AppMenuRegistration registration = *existing;
    eraseRegistration(registration);
    return RemovalOutcome::Removed;
}

RemovalOutcome RegistrarRegistry::retireOwner(const QString &ownerUniqueName,
                                              quint64 observedOwnerGeneration)
{
    const auto generation = m_ownerGenerations.constFind(ownerUniqueName);
    if (generation == m_ownerGenerations.cend()) {
        return RemovalOutcome::NotFound;
    }
    // AGENT-GUARD: a delayed owner-loss callback may retire only the exact
    // owner generation it observed. This keeps future watcher replacement or
    // synthetic test delivery from deleting a newer binding.
    if (*generation != observedOwnerGeneration) {
        return RemovalOutcome::StaleOwnerGeneration;
    }
    QList<quint32> windows = m_windowsByOwner.value(ownerUniqueName).values();
    std::sort(windows.begin(), windows.end());
    for (quint32 windowId : windows) {
        const auto registration = m_byWindow.constFind(windowId);
        if (registration != m_byWindow.cend()) {
            eraseRegistration(*registration);
        }
    }
    return RemovalOutcome::Removed;
}

std::optional<AppMenuRegistration> RegistrarRegistry::registrationFor(quint32 windowId) const
{
    const auto registration = m_byWindow.constFind(windowId);
    return registration == m_byWindow.cend() ? std::nullopt
                                              : std::optional<AppMenuRegistration>(*registration);
}

RegistrarMenuList RegistrarRegistry::menus() const
{
    QList<quint32> windowIds = m_byWindow.keys();
    std::sort(windowIds.begin(), windowIds.end());
    RegistrarMenuList result;
    result.reserve(windowIds.size());
    for (quint32 windowId : windowIds) {
        const AppMenuRegistration registration = m_byWindow.value(windowId);
        result.append(RegistrarMenu{.windowId = registration.windowId,
                                    .service = registration.ownerUniqueName,
                                    .objectPath = registration.menuObjectPath});
    }
    return result;
}

std::optional<quint64> RegistrarRegistry::ownerGeneration(const QString &ownerUniqueName) const
{
    const auto generation = m_ownerGenerations.constFind(ownerUniqueName);
    return generation == m_ownerGenerations.cend() ? std::nullopt
                                                    : std::optional<quint64>(*generation);
}

qsizetype RegistrarRegistry::size() const noexcept
{
    return m_byWindow.size();
}

} // namespace QindaQt::Shell::GlobalMenu::Registrar
