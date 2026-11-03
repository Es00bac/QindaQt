// SPDX-License-Identifier: GPL-3.0-or-later
#include "conversation_view_model.h"

#include "message_json.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace QindaQt::LinkApp {
namespace {

constexpr int kPage = 50;
constexpr int kMaxText = 1600;

QString sendFailureReason(const QString &code)
{
    // Provider codes from sms.sent (docs/wiki/reference/qindalink-protocol-v1.md).
    if (code == QLatin1String("no-service")) return QObject::tr("no mobile service");
    if (code == QLatin1String("radio-off")) return QObject::tr("the phone's radio is off");
    if (code == QLatin1String("limit")) return QObject::tr("the phone's sending limit was reached");
    if (code == QLatin1String("denied")) return QObject::tr("the phone refused");
    return code.isEmpty() ? QObject::tr("failed") : code;
}

} // namespace

ConversationViewModel::ConversationViewModel(LinkApi &api, QObject *parent)
    : QObject(parent), m_api(api), m_clock([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    connect(&m_api, &LinkApi::threadsChanged, this, [this](const QString &device, const QString &json) {
        if (device != m_device || m_thread < 0) return;
        const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
        bool affected = o.value(QStringLiteral("all")).toBool();
        for (const QJsonValue &id : o.value(QStringLiteral("threads")).toArray()) {
            affected = affected || qint64(id.toDouble()) == m_thread;
        }
        if (affected) refresh();
    });
    connect(&m_api, &LinkApi::sendStatus, this, &ConversationViewModel::onSendStatus);
}

QString ConversationViewModel::recipients() const
{
    QStringList parts;
    for (qsizetype i = 0; i < m_addrs.size(); ++i) {
        const QString name = m_names.value(i);
        parts << (name.isEmpty() ? m_addrs.at(i) : QStringLiteral("%1 (%2)").arg(name, m_addrs.at(i)));
    }
    return parts.join(QStringLiteral(", "));
}

void ConversationViewModel::setDeviceId(const QString &id)
{
    if (id == m_device) return;
    m_device = id;
    close();
}

void ConversationViewModel::close()
{
    ++m_generation;
    m_thread = -1;
    m_title.clear();
    m_addrs.clear();
    m_names.clear();
    m_model.setRows({});
    m_older = m_loading = m_sending = false;
    m_error.clear();
    m_sendError.clear();
    m_pendingText.clear();
    m_pendingAddrs.clear();
    m_retryText.clear();
    m_retryAddrs.clear();
    m_sent.clear();
    m_refKeys.clear();
    Q_EMIT threadChanged();
    Q_EMIT stateChanged();
}

void ConversationViewModel::fail(const QString &code)
{
    m_loading = false;
    m_error = messagesErrorText(code);
    Q_EMIT stateChanged();
}

void ConversationViewModel::openThread(const QVariantMap &thread)
{
    close();
    m_thread = thread.value(QStringLiteral("threadId")).toLongLong();
    m_title = thread.value(QStringLiteral("title")).toString();
    m_addrs = thread.value(QStringLiteral("addrs")).toStringList();
    m_names = thread.value(QStringLiteral("names")).toStringList();
    Q_EMIT threadChanged();
    refresh();
    if (thread.value(QStringLiteral("unread")).toInt() > 0) {
        const qlonglong id = m_thread;
        // Android only lets the default SMS app write the provider, so this is often "unsupported";
        // that is expected and silent.
        m_api.markRead(m_device, id, [this, id](const QString &, const QString &error) {
            if (error.isEmpty()) Q_EMIT threadRead(id);
        });
    }
}

void ConversationViewModel::refresh()
{
    if (m_thread < 0 || m_device.isEmpty()) return;
    const quint64 generation = m_generation;
    m_loading = true;
    Q_EMIT stateChanged();
    m_api.listMessages(m_device, m_thread, kPage, -1, QString(), -1,
                       [this, generation](const QString &json, const QString &error) {
                           if (generation != m_generation) return;
                           if (!error.isEmpty()) return fail(error);
                           applyIncoming(json, false);
                       });
}

void ConversationViewModel::loadOlder()
{
    if (m_thread < 0 || m_loading || !m_older) return;
    const QVariantMap *oldest = nullptr;
    for (const QVariantMap &row : m_model.rows()) {
        if (!row.value(QStringLiteral("local")).toBool()) { oldest = &row; break; }
    }
    if (oldest == nullptr) return;
    const quint64 generation = m_generation;
    m_loading = true;
    Q_EMIT stateChanged();
    m_api.listMessages(m_device, m_thread, kPage, oldest->value(QStringLiteral("date")).toLongLong(),
                       oldest->value(QStringLiteral("kind")).toString(), oldest->value(QStringLiteral("id")).toLongLong(),
                       [this, generation](const QString &json, const QString &error) {
                           if (generation != m_generation) return;
                           if (!error.isEmpty()) return fail(error);
                           applyIncoming(json, true);
                       });
}

void ConversationViewModel::applyIncoming(const QString &json, bool older)
{
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    const qint64 now = m_clock();
    QList<QVariantMap> incoming;
    for (const QJsonValue &m : o.value(QStringLiteral("messages")).toArray()) incoming.append(messageRow(m.toObject(), now));
    // A refresh of the newest page says nothing about older pages; only a first load or a page
    // fetched with a cursor decides whether more exist.
    if (older || m_model.count() == 0) m_older = o.value(QStringLiteral("more")).toBool();
    QList<QVariantMap> merged = mergeMessages(m_model.rows(), incoming);
    // A message this client sent is shown from a local row until the provider lists its own copy
    // (many phones never do); drop the local row when the same text appears as a provider row.
    for (int i = int(merged.size()) - 1; i >= 0; --i) {
        if (!merged.at(i).value(QStringLiteral("local")).toBool()) continue;
        for (const QVariantMap &row : std::as_const(incoming)) {
            if (row.value(QStringLiteral("outgoing")).toBool()
                && row.value(QStringLiteral("body")) == merged.at(i).value(QStringLiteral("body"))
                && row.value(QStringLiteral("date")).toLongLong() >= merged.at(i).value(QStringLiteral("date")).toLongLong() - 120000) {
                merged.removeAt(i);
                break;
            }
        }
    }
    m_loading = false;
    m_error.clear();
    setMerged(merged);
    Q_EMIT stateChanged();
}

void ConversationViewModel::setMerged(QList<QVariantMap> merged)
{
    const QList<QVariantMap> &old = m_model.rows();
    auto sameKeys = [&](qsizetype oldFrom, qsizetype newFrom, qsizetype count) {
        for (qsizetype i = 0; i < count; ++i) {
            if (old.at(oldFrom + i).value(QStringLiteral("key")) != merged.at(newFrom + i).value(QStringLiteral("key"))) return false;
        }
        return true;
    };
    const qsizetype n = old.size();
    if (n == 0) {
        m_model.setRows(merged);
    } else if (merged.size() == n && sameKeys(0, 0, n)) {
        for (int i = 0; i < n; ++i) {
            if (old.at(i) != merged.at(i)) m_model.replaceAt(i, merged.at(i));
        }
    } else if (merged.size() > n && sameKeys(0, 0, n)) {
        m_model.append(merged.mid(n));
    } else if (merged.size() > n && sameKeys(0, merged.size() - n, n)) {
        m_model.prepend(merged.mid(0, merged.size() - n));
    } else {
        m_model.setRows(merged);
    }
}

void ConversationViewModel::loadImage(const QString &key)
{
    const int row = m_model.indexOf(QStringLiteral("key"), key);
    if (row < 0) return;
    const QVariantMap data = m_model.get(row);
    if (data.value(QStringLiteral("imageState")).toString() != QLatin1String("none")) return;
    m_model.updateAt(row, {{QStringLiteral("imageState"), QStringLiteral("loading")}});
    const quint64 generation = m_generation;
    m_api.getPart(m_device, data.value(QStringLiteral("imagePart")).toLongLong(), 640,
                  [this, generation, key](const QString &json, const QString &error) {
                      if (generation != m_generation) return;
                      const int at = m_model.indexOf(QStringLiteral("key"), key);
                      if (at < 0) return;
                      const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
                      if (!error.isEmpty() || o.value(QStringLiteral("data")).toString().isEmpty()) {
                          m_model.updateAt(at, {{QStringLiteral("imageState"), QStringLiteral("failed")}});
                          return;
                      }
                      m_model.updateAt(at, {{QStringLiteral("imageState"), QStringLiteral("ready")},
                                            {QStringLiteral("image"), QStringLiteral("data:%1;base64,%2")
                                                                          .arg(o.value(QStringLiteral("mime")).toString(),
                                                                               o.value(QStringLiteral("data")).toString())}});
                  });
}

QString ConversationViewModel::recipientKey(const QStringList &addrs)
{
    // Spaces, dashes, dots and parentheses do not change who a number is; the phone strips them too.
    QStringList normal;
    for (const QString &a : addrs) {
        QString n;
        for (const QChar c : a) {
            if (c.isDigit() || (c == QLatin1Char('+') && n.isEmpty())) n += c;
        }
        normal << (n.isEmpty() ? a : n);
    }
    normal.sort();
    normal.removeDuplicates();
    return normal.join(QLatin1Char(','));
}

QString ConversationViewModel::displayRecipients(const QStringList &addrs) const
{
    QStringList parts;
    for (const QString &addr : addrs) {
        const qsizetype at = m_addrs.indexOf(addr);
        const QString name = at >= 0 ? m_names.value(at) : QString();
        parts << (name.isEmpty() ? addr : QStringLiteral("%1 (%2)").arg(name, addr));
    }
    return parts.join(QStringLiteral(", "));
}

void ConversationViewModel::setSendError(const QString &text)
{
    m_sendError = text;
    Q_EMIT stateChanged();
}

void ConversationViewModel::requestSend(const QString &text) { sendTo(text, m_addrs); }

void ConversationViewModel::sendTo(QString text, QStringList addrs)
{
    if (m_thread < 0 || m_sending) return;
    if (text.trimmed().isEmpty()) return;
    m_sendError.clear();
    if (text.size() > kMaxText) {
        setSendError(tr("A message can be at most %1 characters.").arg(kMaxText));
        return;
    }
    if (addrs.isEmpty()) {
        setSendError(tr("This conversation has no recipient."));
        return;
    }
    if (!m_confirmedSets.contains(recipientKey(addrs))) {
        m_pendingText = text;
        m_pendingAddrs = addrs;
        Q_EMIT confirmSendRequested(displayRecipients(addrs), int(text.size()));
        return;
    }
    sendNow(text, addrs);
}

void ConversationViewModel::confirmSend()
{
    if (m_pendingText.isEmpty()) return;
    m_confirmedSets.insert(recipientKey(m_pendingAddrs));
    const QString text = std::exchange(m_pendingText, QString());
    const QStringList addrs = std::exchange(m_pendingAddrs, QStringList());
    sendNow(text, addrs);
}

void ConversationViewModel::cancelSend()
{
    m_pendingText.clear();
    m_pendingAddrs.clear();
}

void ConversationViewModel::retryToUnsent()
{
    if (m_retryAddrs.isEmpty()) return;
    sendTo(QString(m_retryText), QStringList(m_retryAddrs));
}

void ConversationViewModel::dismissRetry()
{
    m_retryText.clear();
    m_retryAddrs.clear();
    m_sendError.clear();
    Q_EMIT stateChanged();
}

void ConversationViewModel::sendNow(QString text, QStringList addrs)
{
    m_sending = true;
    m_sendError.clear();
    m_retryText.clear();
    m_retryAddrs.clear();
    Q_EMIT stateChanged();
    const quint64 generation = m_generation;
    m_api.send(m_device, addrs, text, [this, generation, text, addrs](const QString &json, const QString &error) {
        if (generation != m_generation) return;
        m_sending = false;
        if (!error.isEmpty()) {
            m_sendError = messagesErrorText(error, true);
            m_retryText = text;     // nothing left the desktop: keep the draft for an explicit retry
            m_retryAddrs = addrs;
            Q_EMIT stateChanged();
            return;
        }
        const qlonglong ref = qint64(QJsonDocument::fromJson(json.toUtf8()).object().value(QStringLiteral("ref")).toDouble());
        const qint64 now = m_clock();
        QVariantMap row = messageRow(QJsonObject{{QStringLiteral("id"), ref}, {QStringLiteral("kind"), QStringLiteral("sms")},
                                                 {QStringLiteral("box"), QStringLiteral("outbox")}, {QStringLiteral("body"), text},
                                                 {QStringLiteral("date"), now}},
                                     now);
        const QString key = QStringLiteral("local:%1").arg(ref);
        row.insert(QStringLiteral("key"), key);
        row.insert(QStringLiteral("local"), true);
        row.insert(QStringLiteral("status"), QStringLiteral("sending"));
        row.insert(QStringLiteral("statusText"), tr("Sending..."));
        m_refKeys.insert(ref, key);
        m_sent.insert(ref, {text, addrs});
        m_model.append({row});
        Q_EMIT sendAccepted();
        Q_EMIT stateChanged();
    });
}

void ConversationViewModel::setLocalStatus(const QString &key, const QString &status, const QString &text)
{
    const int row = m_model.indexOf(QStringLiteral("key"), key);
    if (row >= 0) m_model.updateAt(row, {{QStringLiteral("status"), status}, {QStringLiteral("statusText"), text}});
}

void ConversationViewModel::onSendStatus(const QString &device, qlonglong ref, const QString &state, const QString &code,
                                         const QString &detail)
{
    if (device != m_device) return;
    const QString key = m_refKeys.value(ref);
    if (key.isEmpty()) return;
    if (state == QLatin1String("sent")) setLocalStatus(key, state, tr("Sent"));
    else if (state == QLatin1String("delivered")) { setLocalStatus(key, state, tr("Delivered")); m_refKeys.remove(ref); m_sent.remove(ref); }
    else if (state == QLatin1String("failed")) {
        const SentInfo info = m_sent.take(ref);
        m_refKeys.remove(ref);
        // `unsent` lists indexes into the recipients of that request; without it nothing is known
        // to have left, so every recipient is offered again.
        QStringList unsent;
        const QJsonValue listed = QJsonDocument::fromJson(detail.toUtf8()).object().value(QStringLiteral("unsent"));
        if (listed.isArray()) {
            for (const QJsonValue &i : listed.toArray()) {
                const int at = i.toInt(-1);
                if (at >= 0 && at < info.addrs.size()) unsent << info.addrs.at(at);
            }
        } else {
            unsent = info.addrs;
        }
        QString text = tr("Not sent: %1").arg(sendFailureReason(code));
        if (!unsent.isEmpty()) text += tr(". Not sent to: %1").arg(displayRecipients(unsent));
        setLocalStatus(key, state, text);
        if (!unsent.isEmpty() && !info.text.isEmpty()) {
            m_retryText = info.text;
            m_retryAddrs = unsent;
            Q_EMIT stateChanged();
        }
    }
}

} // namespace QindaQt::LinkApp
