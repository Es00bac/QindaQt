// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app_preferences.h"
#include "json_list_model.h"
#include "link_api.h"
#include "process_runner.h"

#include <QtCore/QObject>

namespace QindaQt::LinkApp {

// Phone apps page: the phone's launchable apps from `qindaqt-link-mirror --serial S --list-json`,
// a search box, pinned favourites (persisted in AppPreferences) and "open on desktop".
//
// States (`state`): "idle" (not asked yet), "loading", "ready", "needs-adb" (no phone reachable over
// USB debugging, or no mirror program), "error".
// AGENT-CONTRACT: the serial comes from Link1.Setup.AdbDevices; this class never runs adb. The
// mirror is a separate process per app window (`--serial S --package P`), started detached.
// AGENT-GUARD: package names and labels are not logged; evidence quotes counts only.
class PhoneAppsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *apps READ apps CONSTANT FINAL)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged FINAL)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY stateChanged FINAL)
public:
    explicit PhoneAppsViewModel(LinkApi &api, ProcessRunner &runner, AppPreferences &preferences,
                                QObject *parent = nullptr);
    // Program that lists and mirrors apps; tests pass a fake path.
    void setMirrorProgram(QString program) { m_program = std::move(program); }
    // Pure parse of `--list-json` output into rows {pkg,label,iconUrl,favourite}; empty on malformed input.
    [[nodiscard]] static QList<QVariantMap> parseInventory(const QByteArray &json);

    [[nodiscard]] JsonListModel *apps() { return &m_visible; }
    [[nodiscard]] QString state() const { return m_state; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    [[nodiscard]] int totalCount() const { return int(m_all.size()); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggleFavourite(const QString &pkg);
    // Starts the mirror window for `pkg`. Returns false (and sets `message`) when it cannot.
    Q_INVOKABLE bool open(const QString &pkg);

Q_SIGNALS:
    void stateChanged();
    void searchTextChanged();

private:
    void setState(const QString &state, const QString &message);
    void rebuild();

    LinkApi &m_api;
    ProcessRunner &m_runner;
    AppPreferences &m_prefs;
    QString m_program = QStringLiteral("qindaqt-link-mirror");
    QString m_serial;
    QString m_state = QStringLiteral("idle");
    QString m_message;
    QString m_search;
    QList<QVariantMap> m_all;
    JsonListModel m_visible{{QStringLiteral("pkg"), QStringLiteral("label"), QStringLiteral("iconUrl"), QStringLiteral("favourite")}};
    quint64 m_generation = 0;
};

} // namespace QindaQt::LinkApp
