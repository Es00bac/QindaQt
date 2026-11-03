// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "json_list_model.h"
#include "link_api.h"

#include <QtCore/QObject>

#include <functional>

namespace QindaQt::LinkApp {

// The conversation list: newest-first pages from `ListThreads`, a search over the loaded
// threads, unread totals, and live refresh on `ThreadsChanged`.
//
// Ownership: borrows the api. Threading: GUI thread. Error behavior: `error` holds a short
// user-facing reason ("" when fine); the list keeps what it already had.
// AGENT-GUARD: rows contain names, numbers and snippets. Search runs in memory only and nothing
// here logs a row.
class ThreadsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *threads READ threads CONSTANT FINAL)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(int unread READ unread NOTIFY stateChanged FINAL)
    Q_PROPERTY(int loadedCount READ loadedCount NOTIFY stateChanged FINAL)
public:
    using Clock = std::function<qint64()>;
    explicit ThreadsViewModel(LinkApi &api, QObject *parent = nullptr);
    void setClock(Clock clock) { m_clock = std::move(clock); }

    [[nodiscard]] JsonListModel *threads() { return &m_visible; }
    [[nodiscard]] QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    [[nodiscard]] bool loading() const { return m_loading; }
    [[nodiscard]] bool hasMore() const { return m_more; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int unread() const { return m_unread; }
    [[nodiscard]] int loadedCount() const { return int(m_all.size()); }
    [[nodiscard]] QString deviceId() const { return m_device; }

    // The phone whose conversations are shown; "" clears the list.
    Q_INVOKABLE void setDeviceId(const QString &id);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadMore();
    // A conversation was read on the phone (or marked read here): drop its badge.
    Q_INVOKABLE void clearUnread(qint64 threadId);
    // One loaded thread's row, or an empty map.
    Q_INVOKABLE QVariantMap thread(qint64 threadId) const;

Q_SIGNALS:
    void searchTextChanged();
    void stateChanged();

private:
    void applyPage(const QString &json, bool replaceFirstPage);
    void rebuildVisible();
    void fail(const QString &code);

    LinkApi &m_api;
    Clock m_clock;
    QString m_device;
    QString m_search;
    QList<QVariantMap> m_all;  // newest first
    JsonListModel m_visible{{QStringLiteral("threadId"), QStringLiteral("title"), QStringLiteral("snippet"),
                             QStringLiteral("when"), QStringLiteral("date"), QStringLiteral("unread"),
                             QStringLiteral("count"), QStringLiteral("addrs"), QStringLiteral("names"),
                             QStringLiteral("searchKey")}};
    bool m_loading = false;
    bool m_more = false;
    int m_unread = 0;
    QString m_error;
    quint64 m_generation = 0;
};

} // namespace QindaQt::LinkApp
