// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_macros/macro_catalog.h>

#include <QtCore/QJsonDocument>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Link::Macros {
namespace {
// A snapshot is bounded by the library budget (1 MiB); anything larger is not ours.
constexpr qsizetype kMaxSnapshotBytes = 2 * 1024 * 1024;
}

MacroCatalog::MacroCatalog(QDBusConnection bus, QString service, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_service(std::move(service)),
      m_watcher(m_service, m_bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                if (newOwner.isEmpty()) apply(false, {}, {});
                else refresh();
            });
}

MacroCatalog::~MacroCatalog() = default;

void MacroCatalog::start()
{
    if (m_started) return;
    m_started = true;
    m_bus.connect(m_service, QString::fromLatin1(InputActions::Object), QString::fromLatin1(InputActions::Interface),
                  QStringLiteral("Changed"), this, SLOT(onChanged(qulonglong)));
    refresh();
}

std::optional<InputActions::Macro> MacroCatalog::macro(const QString &id) const
{
    for (const auto &m : m_macros) {
        if (m.id == id) return m;
    }
    return std::nullopt;
}

void MacroCatalog::onChanged(qulonglong) { refresh(); }

void MacroCatalog::refresh()
{
    // Never activates the service: a library nobody provides stays empty.
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, QString::fromLatin1(InputActions::Object),
                                                      QString::fromLatin1(InputActions::Interface),
                                                      QStringLiteral("GetSnapshot"));
    msg.setAutoStartService(false);
    const quint64 generation = ++m_generation;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_generation) return; // a newer refresh supersedes this reply
        const QDBusPendingReply<QString> reply = *watcher;
        if (reply.isError() || reply.value().toUtf8().size() > kMaxSnapshotBytes) {
            apply(false, {}, {});
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply.value().toUtf8()).object();
        if (o.value(QStringLiteral("schemaVersion")).toInt() != 1 || !o.value(QStringLiteral("ready")).toBool()
            || !o.value(QStringLiteral("macros")).isArray()) {
            apply(false, {}, {});
            return;
        }
        QList<InputActions::Macro> macros;
        for (const QJsonValue &row : o.value(QStringLiteral("macros")).toArray()) {
            InputActions::Macro m;
            QString reason;
            if (row.isObject() && InputActions::readMacro(row.toObject().value(QStringLiteral("config")).toObject(), m, reason)
                && macros.size() < InputActions::MaxMacros) {
                macros.append(m);
            }
        }
        apply(true, o.value(QStringLiteral("revision")).toString(), std::move(macros));
    });
}

void MacroCatalog::apply(bool ready, const QString &revision, QList<InputActions::Macro> macros)
{
    const bool differs = ready != m_ready || macros != m_macros;
    m_ready = ready;
    m_revision = revision;
    m_macros = std::move(macros);
    if (differs) Q_EMIT changed();
}

} // namespace QindaQt::Link::Macros
