// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/input_actions/input_actions.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

#include <optional>

namespace QindaQt::Link::Macros {

// Read-only mirror of the user's macro library, obtained from the public
// org.qindaqt.InputActions1 GetSnapshot() and refreshed on Changed(revision).
// QindaLink never reads qindaqt/input-macros.json (ADR-0436) and never edits
// the library.
//
// Ownership: owns its bus subscriptions. Threading: GUI thread. Error
// behavior: an unavailable service, an unready library or a malformed
// snapshot empties the catalog; it never keeps stale macros.
// AGENT-CONTRACT: the other side is MacroEndpoint (src/controllers/kwin).
// Version-1 JSON: {schemaVersion:1, revision, ready, macros:[{config:{...}}]}.
class MacroCatalog final : public QObject {
    Q_OBJECT
public:
    explicit MacroCatalog(QDBusConnection bus, QString service = QString::fromLatin1(InputActions::Service),
                          QObject *parent = nullptr);
    ~MacroCatalog() override;

    void start();
    [[nodiscard]] bool ready() const { return m_ready; }
    [[nodiscard]] const QList<InputActions::Macro> &macros() const { return m_macros; }
    [[nodiscard]] std::optional<InputActions::Macro> macro(const QString &id) const;
    [[nodiscard]] QString revision() const { return m_revision; }

Q_SIGNALS:
    // Emitted only when the set of macros (or any step) really changed; a
    // revision bump caused by a macro starting or stopping is silent.
    void changed();

private Q_SLOTS:
    void onChanged(qulonglong revision);

private:
    void refresh();
    void apply(bool ready, const QString &revision, QList<InputActions::Macro> macros);

    QDBusConnection m_bus;
    QString m_service;
    QDBusServiceWatcher m_watcher;
    QList<InputActions::Macro> m_macros;
    QString m_revision;
    bool m_ready = false;
    bool m_started = false;
    quint64 m_generation = 0;
};

} // namespace QindaQt::Link::Macros
