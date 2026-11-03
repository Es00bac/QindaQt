// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "json_list_model.h"
#include "link_api.h"

#include <QtCore/QObject>

namespace QindaQt::LinkApp {

// Notifications page: the per-app mute list (`ListApps` / `SetAppMuted`).
// AGENT-NOTE: only app labels and packages are handled here; notification titles and texts never
// reach this class.
class NotificationsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *apps READ apps CONSTANT FINAL)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged FINAL)
    Q_PROPERTY(int mutedCount READ mutedCount NOTIFY stateChanged FINAL)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY stateChanged FINAL)
public:
    explicit NotificationsViewModel(LinkApi &api, QObject *parent = nullptr);
    [[nodiscard]] JsonListModel *apps() { return &m_visible; }
    [[nodiscard]] QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] bool loaded() const { return m_loaded; }
    [[nodiscard]] int mutedCount() const;
    [[nodiscard]] int totalCount() const { return int(m_all.size()); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setMuted(const QString &pkg, bool muted);

Q_SIGNALS:
    void stateChanged();
    void searchTextChanged();

private:
    void rebuild();

    LinkApi &m_api;
    QList<QVariantMap> m_all;
    JsonListModel m_visible{{QStringLiteral("pkg"), QStringLiteral("label"), QStringLiteral("muted")}};
    QString m_search;
    QString m_error;
    bool m_loaded = false;
};

} // namespace QindaQt::LinkApp
