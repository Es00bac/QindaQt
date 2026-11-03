// SPDX-License-Identifier: GPL-3.0-or-later
#include "threads_view_model.h"

#include "message_json.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::LinkApp {
namespace {

constexpr int kPage = 50;

bool newer(const QVariantMap &a, const QVariantMap &b)
{
    const qint64 da = a.value(QStringLiteral("date")).toLongLong();
    const qint64 db = b.value(QStringLiteral("date")).toLongLong();
    if (da != db) return da > db;
    return a.value(QStringLiteral("threadId")).toLongLong() > b.value(QStringLiteral("threadId")).toLongLong();
}

} // namespace

ThreadsViewModel::ThreadsViewModel(LinkApi &api, QObject *parent)
    : QObject(parent), m_api(api), m_clock([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    connect(&m_api, &LinkApi::threadsChanged, this, [this](const QString &device, const QString &json) {
        if (device != m_device) return;
        const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
        if (o.contains(QStringLiteral("unread"))) {
            m_unread = o.value(QStringLiteral("unread")).toInt();
            Q_EMIT stateChanged();
        }
        reload();
    });
    connect(&m_api, &LinkApi::availableChanged, this, [this] { if (m_api.available()) reload(); });
}

void ThreadsViewModel::setSearchText(const QString &text)
{
    if (text == m_search) return;
    m_search = text;
    rebuildVisible();
    Q_EMIT searchTextChanged();
}

void ThreadsViewModel::setDeviceId(const QString &id)
{
    if (id == m_device) return;
    m_device = id;
    m_all.clear();
    m_more = false;
    m_unread = 0;
    m_error.clear();
    ++m_generation;
    rebuildVisible();
    Q_EMIT stateChanged();
    if (!m_device.isEmpty()) reload();
}

void ThreadsViewModel::fail(const QString &code)
{
    m_loading = false;
    m_error = messagesErrorText(code);
    Q_EMIT stateChanged();
}

void ThreadsViewModel::reload()
{
    if (m_device.isEmpty()) return;
    const quint64 generation = ++m_generation;
    m_loading = true;
    Q_EMIT stateChanged();
    m_api.listThreads(m_device, kPage, -1, -1, [this, generation](const QString &json, const QString &error) {
        if (generation != m_generation) return;
        if (!error.isEmpty()) return fail(error);
        applyPage(json, true);
    });
}

void ThreadsViewModel::loadMore()
{
    if (m_device.isEmpty() || m_loading || !m_more || m_all.isEmpty()) return;
    const quint64 generation = m_generation;
    const QVariantMap last = m_all.constLast();
    m_loading = true;
    Q_EMIT stateChanged();
    m_api.listThreads(m_device, kPage, last.value(QStringLiteral("date")).toLongLong(),
                      last.value(QStringLiteral("threadId")).toLongLong(),
                      [this, generation](const QString &json, const QString &error) {
                          if (generation != m_generation) return;
                          if (!error.isEmpty()) return fail(error);
                          applyPage(json, false);
                      });
}

void ThreadsViewModel::applyPage(const QString &json, bool firstPage)
{
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    const qint64 now = m_clock();
    QHash<qint64, QVariantMap> byId;
    QSet<qint64> inPage;
    for (const QJsonValue &t : o.value(QStringLiteral("threads")).toArray()) {
        inPage.insert(qint64(t.toObject().value(QStringLiteral("id")).toDouble()));
    }
    for (const QVariantMap &row : std::as_const(m_all)) byId.insert(row.value(QStringLiteral("threadId")).toLongLong(), row);
    for (const QJsonValue &t : o.value(QStringLiteral("threads")).toArray()) {
        const QVariantMap row = threadRow(t.toObject(), now);
        byId.insert(row.value(QStringLiteral("threadId")).toLongLong(), row);
    }
    if (firstPage) {
        // A refreshed newest page is authoritative for everything at least as new as its oldest row
        // (or for everything, when the phone has no more): a thread in that range which is not in
        // the page is gone (deleted on the phone).
        const QJsonArray page = o.value(QStringLiteral("threads")).toArray();
        const bool phoneHasMore = o.value(QStringLiteral("more")).toBool();
        qint64 oldestDate = 0;
        qint64 oldestId = 0;
        if (!page.isEmpty()) {
            const QJsonObject last = page.last().toObject();
            oldestDate = qint64(last.value(QStringLiteral("date")).toDouble());
            oldestId = qint64(last.value(QStringLiteral("id")).toDouble());
        }
        for (auto it = byId.begin(); it != byId.end();) {
            const qint64 id = it.key();
            const qint64 date = it->value(QStringLiteral("date")).toLongLong();
            const bool inRange = !phoneHasMore || page.isEmpty() || date > oldestDate || (date == oldestDate && id >= oldestId);
            if (!inPage.contains(id) && inRange) it = byId.erase(it); else ++it;
        }
    }
    m_all = byId.values();
    std::sort(m_all.begin(), m_all.end(), newer);
    // A refresh of the newest page says nothing about whether older pages exist beyond what we hold.
    if (!firstPage || m_all.size() <= kPage) m_more = o.value(QStringLiteral("more")).toBool();
    m_unread = o.value(QStringLiteral("unread")).toInt(m_unread);
    m_loading = false;
    m_error.clear();
    rebuildVisible();
    Q_EMIT stateChanged();
}

void ThreadsViewModel::rebuildVisible()
{
    const QString needle = m_search.trimmed().toLower();
    QList<QVariantMap> rows;
    for (const QVariantMap &row : std::as_const(m_all)) {
        if (needle.isEmpty() || row.value(QStringLiteral("searchKey")).toString().contains(needle)) rows.append(row);
    }
    m_visible.setRows(rows);
}

void ThreadsViewModel::clearUnread(qint64 threadId)
{
    for (QVariantMap &row : m_all) {
        if (row.value(QStringLiteral("threadId")).toLongLong() == threadId && row.value(QStringLiteral("unread")).toInt() > 0) {
            m_unread = std::max(0, m_unread - row.value(QStringLiteral("unread")).toInt());
            row.insert(QStringLiteral("unread"), 0);
        }
    }
    rebuildVisible();
    Q_EMIT stateChanged();
}

QVariantMap ThreadsViewModel::thread(qint64 threadId) const
{
    for (const QVariantMap &row : m_all) {
        if (row.value(QStringLiteral("threadId")).toLongLong() == threadId) return row;
    }
    return {};
}

} // namespace QindaQt::LinkApp
