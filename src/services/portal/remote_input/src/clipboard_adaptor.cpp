// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>
// SPDX-FileCopyrightText: 2026 QindaQt contributors
#include "clipboard_adaptor_p.h"
#include <QPointer>
#include <algorithm>
#include <limits>
#include <optional>

namespace QindaQt::Services::Portal::RemoteInput {
namespace {
constexpr auto kPath = "/org/kde/KWin/EIS/RemoteDesktop";
constexpr auto kInterface = "org.kde.KWin.EIS.RemoteDesktop";
constexpr auto kPortal = "org.freedesktop.impl.portal.Clipboard";
constexpr auto kNotAllowed = "org.freedesktop.portal.Error.NotAllowed";
constexpr auto kFailed = "org.freedesktop.portal.Error.Failed";
constexpr int kMaxTransfers = 16;
std::optional<QStringList> mimeTypes(const QVariantMap &options) {
    const auto value = options.value(QStringLiteral("mime_types"));
    if (value.metaType() != QMetaType::fromType<QStringList>()) return std::nullopt;
    const auto types = value.toStringList();
    if (types.size() > 64) return std::nullopt;
    for (const auto &type : types)
        if (type.isEmpty() || type.size() > 255) return std::nullopt;
    return types;
}
}
ClipboardAdaptor::ClipboardAdaptor(QObject &host, RemoteSessions &sessions, AccessConsent &consent,
                                   CompositorEis &eis, QDBusConnection bus)
    : QDBusAbstractAdaptor(&host), m_sessions(sessions), m_consent(consent), m_eis(eis),
      m_bus(std::move(bus)) {
    m_sink.handler = [this](const QDBusMessage &message) { compositorSignal(message); };
}
ClipboardAdaptor::~ClipboardAdaptor() = default;
void ClipboardAdaptor::refuse(const QDBusMessage &call, const char *error) {
    call.setDelayedReply(true);
    m_bus.send(call.createErrorReply(QLatin1String(error), QStringLiteral("Clipboard refused")));
}
RemoteSessions::Entry *ClipboardAdaptor::granted(const QDBusMessage &call, const QString &path) {
    auto *entry = m_sessions.entry(path);
    if (!entry || !entry->clipboard || entry->phase != RemotePhase::Started || !m_sessions.owned(call, path)
        || !m_consent.admitted()) return nullptr;
    return entry;
}
QString ClipboardAdaptor::sessionForHandle(int handle) const {
    for (const auto &path : m_sessions.paths())
        if (const auto *entry = m_sessions.find(path); entry && handle && entry->clipboard == handle) return path;
    return {};
}
void ClipboardAdaptor::enable(const QString &path, std::function<void(bool)> done) {
    const QPointer<ClipboardAdaptor> guard(this);
    const bool sent = m_eis.call(QLatin1String(kPath), QLatin1String(kInterface), QStringLiteral("connectClipboard"), {},
        [this, guard, path, done](const QDBusMessage &reply, const QString &owner) {
            if (!guard) return;
            const int handle = reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                ? reply.arguments().at(0).toInt() : 0;
            auto *entry = m_sessions.entry(path);
            if (!entry || handle <= 0 || owner != m_eis.compositor()) {
                // A late handle for a retired session is released, never used.
                if (handle > 0)
                    m_eis.call(QLatin1String(kPath), QLatin1String(kInterface), QStringLiteral("disconnectClipboard"), {handle},
                               [](const QDBusMessage &, const QString &) {});
                done(false);
                return;
            }
            entry->clipboard = handle;
            if (m_signalsOwner != owner) {
                for (const auto *member : {"selectionChanged", "selectionTransfer"}) {
                    if (!m_signalsOwner.isEmpty())
                        m_bus.disconnect(m_signalsOwner, QLatin1String(kPath), QLatin1String(kInterface), QLatin1String(member),
                                         &m_sink, SLOT(received(QDBusMessage)));
                    m_bus.connect(owner, QLatin1String(kPath), QLatin1String(kInterface), QLatin1String(member),
                                  &m_sink, SLOT(received(QDBusMessage)));
                }
                m_signalsOwner = owner;
            }
            done(true);
        });
    if (!sent) done(false);
}
void ClipboardAdaptor::retired(const RemoteSessions::Entry &entry) {
    if (entry.clipboard)
        m_eis.call(QLatin1String(kPath), QLatin1String(kInterface), QStringLiteral("disconnectClipboard"), {entry.clipboard},
                   [](const QDBusMessage &, const QString &) {});
}
void ClipboardAdaptor::compositorSignal(const QDBusMessage &message) {
    // AGENT-GUARD: only the selected compositor's targeted signals for a live
    // granted handle produce frontend clipboard signals or paste FDs.
    if (message.service() != m_eis.compositor() || message.path() != QLatin1String(kPath)) return;
    const auto args = message.arguments();
    if (args.isEmpty()) return;
    const QString path = sessionForHandle(args.at(0).toInt());
    auto *entry = m_sessions.entry(path);
    if (!entry || !m_sessions.live(path) || !m_consent.admitted()) return;
    if (message.member() == QLatin1String("selectionChanged") && args.size() == 3) {
        auto signal = QDBusMessage::createTargetedSignal(entry->frontend, QStringLiteral("/org/freedesktop/portal/desktop"),
                                                         QLatin1String(kPortal), QStringLiteral("SelectionOwnerChanged"));
        signal << QVariant::fromValue(QDBusObjectPath(path))
               << QVariantMap{{QStringLiteral("mime_types"), args.at(1).toStringList()},
                              {QStringLiteral("session_is_owner"), args.at(2).toBool()}};
        m_bus.send(signal);
    } else if (message.member() == QLatin1String("selectionTransfer") && args.size() == 3) {
        const auto fd = qdbus_cast<QDBusUnixFileDescriptor>(args.at(2));
        if (!fd.isValid()) return;
        // Bounded: the oldest unanswered paste is closed (its reader sees EOF).
        while (entry->transfers.size() >= kMaxTransfers) {
            uint oldest = std::numeric_limits<uint>::max();
            for (auto it = entry->transfers.cbegin(); it != entry->transfers.cend(); ++it) oldest = std::min(oldest, it.key());
            entry->transfers.remove(oldest);
        }
        const uint serial = ++entry->nextSerial;
        entry->transfers.insert(serial, fd);
        auto signal = QDBusMessage::createTargetedSignal(entry->frontend, QStringLiteral("/org/freedesktop/portal/desktop"),
                                                         QLatin1String(kPortal), QStringLiteral("SelectionTransfer"));
        signal << QVariant::fromValue(QDBusObjectPath(path)) << args.at(1).toString() << serial;
        m_bus.send(signal);
    }
}
void ClipboardAdaptor::RequestClipboard(const QDBusObjectPath &session, const QVariantMap &, const QDBusMessage &call) {
    auto *entry = m_sessions.entry(session.path());
    // Upstream rule: only before Start, so consent can include the clipboard.
    if (!entry || !m_sessions.owned(call, session.path()) || !m_consent.admitted()
        || (entry->phase != RemotePhase::Created && entry->phase != RemotePhase::Selected)) {
        refuse(call, kNotAllowed);
        return;
    }
    entry->clipboardRequested = true;
}
void ClipboardAdaptor::SetSelection(const QDBusObjectPath &session, const QVariantMap &options, const QDBusMessage &call) {
    auto *entry = granted(call, session.path());
    const auto types = mimeTypes(options);
    if (!entry || !types) { refuse(call, kNotAllowed); return; }
    call.setDelayedReply(true);
    const QPointer<ClipboardAdaptor> guard(this);
    if (!m_eis.call(QLatin1String(kPath), QLatin1String(kInterface), QStringLiteral("setSelection"), {entry->clipboard, *types},
            [this, guard, call](const QDBusMessage &reply, const QString &) {
                if (!guard) return;
                if (reply.type() == QDBusMessage::ReplyMessage) m_bus.send(call.createReply());
                else refuse(call, kFailed);
            }))
        refuse(call, kFailed);
}
QDBusUnixFileDescriptor ClipboardAdaptor::SelectionWrite(const QDBusObjectPath &session, uint serial, const QDBusMessage &call) {
    auto *entry = granted(call, session.path());
    const auto fd = entry ? entry->transfers.take(serial) : QDBusUnixFileDescriptor{};
    if (!fd.isValid()) { refuse(call, kNotAllowed); return {}; }
    return fd;
}
void ClipboardAdaptor::SelectionWriteDone(const QDBusObjectPath &session, uint serial, bool, const QDBusMessage &call) {
    // The paste FD already belongs to the writer; closing any unclaimed one
    // ends the requester's read with EOF.
    auto *entry = m_sessions.entry(session.path());
    if (!entry || !m_sessions.owned(call, session.path())) { refuse(call, kNotAllowed); return; }
    entry->transfers.remove(serial);
}
QDBusUnixFileDescriptor ClipboardAdaptor::SelectionRead(const QDBusObjectPath &session, const QString &mimeType,
                                                        const QDBusMessage &call) {
    auto *entry = granted(call, session.path());
    if (!entry || mimeType.isEmpty() || mimeType.size() > 255) { refuse(call, kNotAllowed); return {}; }
    call.setDelayedReply(true);
    const QPointer<ClipboardAdaptor> guard(this);
    const QString path = session.path();
    if (!m_eis.call(QLatin1String(kPath), QLatin1String(kInterface), QStringLiteral("readSelection"), {entry->clipboard, mimeType},
            [this, guard, call, path](const QDBusMessage &reply, const QString &owner) {
                if (!guard) return;
                const auto fd = reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                    ? qdbus_cast<QDBusUnixFileDescriptor>(reply.arguments().at(0)) : QDBusUnixFileDescriptor{};
                // Publication rechecks the grant: a retired session gets no data.
                if (!fd.isValid() || owner != m_eis.compositor() || !granted(call, path)) { refuse(call, kFailed); return; }
                m_bus.send(call.createReply(QVariant::fromValue(fd)));
            }))
        refuse(call, kFailed);
    return {};
}
} // namespace QindaQt::Services::Portal::RemoteInput
