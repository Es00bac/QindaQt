// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "json_list_model.h"
#include "link_api.h"

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <functional>

namespace QindaQt::LinkApp {

// One open conversation: chronological bubbles, paging of older messages, MMS images, compose and
// the send flow with its per-session confirmation.
//
// Ownership: borrows the api. Threading: GUI thread. Error behavior: `error` holds a short reason.
// AGENT-GUARD: nothing is sent except from sendNow(), which is reached only through
// requestSend() (the Send click) and, the first time in a session, confirmSend() (the dialog's
// OK). A draft is never sent by a timer, a signal or a restore.
// AGENT-GUARD: bodies and addresses are personal content; never log them.
class ConversationViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *messages READ messages CONSTANT FINAL)
    Q_PROPERTY(qlonglong threadId READ threadId NOTIFY threadChanged FINAL)
    Q_PROPERTY(QString title READ title NOTIFY threadChanged FINAL)
    Q_PROPERTY(QString recipients READ recipients NOTIFY threadChanged FINAL)
    Q_PROPERTY(bool open READ isOpen NOTIFY threadChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool sending READ sending NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool sendConfirmedThisSession READ confirmedThisSession NOTIFY stateChanged FINAL)
public:
    using Clock = std::function<qint64()>;
    explicit ConversationViewModel(LinkApi &api, QObject *parent = nullptr);
    void setClock(Clock clock) { m_clock = std::move(clock); }

    [[nodiscard]] JsonListModel *messages() { return &m_model; }
    [[nodiscard]] qlonglong threadId() const { return m_thread; }
    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] QString recipients() const;
    [[nodiscard]] bool isOpen() const { return m_thread >= 0; }
    [[nodiscard]] bool loading() const { return m_loading; }
    [[nodiscard]] bool hasOlder() const { return m_older; }
    [[nodiscard]] bool sending() const { return m_sending; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] bool confirmedThisSession() const { return m_confirmed; }

    Q_INVOKABLE void setDeviceId(const QString &id);
    // `thread` is a ThreadsViewModel row (threadId, title, addrs, names, unread).
    Q_INVOKABLE void openThread(const QVariantMap &thread);
    Q_INVOKABLE void close();
    Q_INVOKABLE void loadOlder();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadImage(const QString &key);
    // The Send click. Sends at once when this session already confirmed recipients, else asks.
    Q_INVOKABLE void requestSend(const QString &text);
    Q_INVOKABLE void confirmSend();
    Q_INVOKABLE void cancelSend();

Q_SIGNALS:
    void threadChanged();
    void stateChanged();
    // The first Send of a session: show `recipients` and `characters` in a confirmation dialog.
    void confirmSendRequested(const QString &recipients, int characters);
    void sendAccepted();
    void threadRead(qlonglong threadId);

private:
    void sendNow(const QString &text);
    void applyIncoming(const QString &json, bool older);
    void setMerged(QList<QVariantMap> merged);
    void onSendStatus(const QString &device, qlonglong ref, const QString &state, const QString &code, const QString &detail);
    void setLocalStatus(const QString &key, const QString &status, const QString &text);
    void fail(const QString &code);

    LinkApi &m_api;
    Clock m_clock;
    JsonListModel m_model{{QStringLiteral("key"), QStringLiteral("kind"), QStringLiteral("id"), QStringLiteral("date"),
                           QStringLiteral("when"), QStringLiteral("outgoing"), QStringLiteral("box"),
                           QStringLiteral("body"), QStringLiteral("trunc"), QStringLiteral("parts"),
                           QStringLiteral("imagePart"), QStringLiteral("image"), QStringLiteral("imageState"),
                           QStringLiteral("status"), QStringLiteral("statusText"), QStringLiteral("local")}};
    QString m_device;
    qlonglong m_thread = -1;
    QString m_title;
    QStringList m_addrs;
    QStringList m_names;
    bool m_loading = false;
    bool m_older = false;
    bool m_sending = false;
    bool m_confirmed = false;
    QString m_pendingText;
    QString m_error;
    QHash<qlonglong, QString> m_refKeys;  // send ref -> local row key
    quint64 m_generation = 0;
};

} // namespace QindaQt::LinkApp
