// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Registrar
{

inline constexpr qsizetype kMaxRegisteredWindows = 1024;
inline constexpr qsizetype kMaxMenuObjectPathUtf8Bytes = 4096;

struct AppMenuRegistration final {
    quint32 windowId = 0;
    QString ownerUniqueName;
    QDBusObjectPath menuObjectPath;
    quint64 ownerGeneration = 0;
    quint64 registrationGeneration = 0;

    bool operator==(const AppMenuRegistration &) const = default;
};

enum class RegistrationOutcome {
    Registered,
    Updated,
    Unchanged,
    Invalid,
    CapacityExceeded,
    OwnedByAnotherPeer,
    GenerationExhausted,
};

struct RegistrationResult final {
    RegistrationOutcome outcome = RegistrationOutcome::Invalid;
    std::optional<AppMenuRegistration> registration;
};

enum class RemovalOutcome {
    Removed,
    NotFound,
    NotOwner,
    StaleOwnerGeneration,
};

// Bounded process-local registry behind the legacy D-Bus surface. The caller
// unique name is the ownership key; a well-known service can never be stored.
// The registry owns copied registrations until removal/destruction. Invalid,
// over-limit, stale-generation, and cross-owner mutations fail closed without
// changing state. All calls and signals stay on the constructing Qt thread.
class RegistrarRegistry final : public QObject
{
    Q_OBJECT

public:
    explicit RegistrarRegistry(QObject *parent = nullptr);

    [[nodiscard]] RegistrationResult registerWindow(quint32 windowId,
                                                    const QString &ownerUniqueName,
                                                    const QDBusObjectPath &menuObjectPath);
    [[nodiscard]] RemovalOutcome unregisterWindow(quint32 windowId,
                                                  const QString &ownerUniqueName);
    [[nodiscard]] RemovalOutcome retireOwner(const QString &ownerUniqueName,
                                             quint64 observedOwnerGeneration);

    [[nodiscard]] std::optional<AppMenuRegistration> registrationFor(
        quint32 windowId) const;
    [[nodiscard]] RegistrarMenuList menus() const;
    [[nodiscard]] std::optional<quint64> ownerGeneration(
        const QString &ownerUniqueName) const;
    [[nodiscard]] qsizetype size() const noexcept;

Q_SIGNALS:
    void windowRegistered(QindaQt::Shell::GlobalMenu::Registrar::AppMenuRegistration registration);
    void windowUnregistered(quint32 windowId, QString formerOwnerUniqueName);
    void ownerBecamePresent(QString ownerUniqueName, quint64 ownerGeneration);
    void ownerBecameAbsent(QString ownerUniqueName);

private:
    [[nodiscard]] std::optional<quint64> nextGeneration();
    void eraseRegistration(const AppMenuRegistration &registration);

    QHash<quint32, AppMenuRegistration> m_byWindow;
    QHash<QString, QSet<quint32>> m_windowsByOwner;
    QHash<QString, quint64> m_ownerGenerations;
    quint64 m_generationHighWater = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Registrar

Q_DECLARE_METATYPE(QindaQt::Shell::GlobalMenu::Registrar::AppMenuRegistration)
