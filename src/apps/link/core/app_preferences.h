// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>

namespace QindaQt::LinkApp {

// The app's own small settings, `$XDG_CONFIG_HOME/qindalink/app.json`:
// {"favourites":["pkg",...],"rcsNoticeDismissed":bool}.
//
// Ownership: the path is injected (tests pass a QTemporaryDir). Error behavior: a missing or
// unreadable file reads as defaults; a failed write is reported by save() returning false and
// the in-memory value stays. The service's `qindalink/` files are never touched from here.
class AppPreferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool rcsNoticeDismissed READ rcsNoticeDismissed WRITE dismissRcsNotice NOTIFY changed FINAL)
public:
    explicit AppPreferences(QString path, QObject *parent = nullptr);
    [[nodiscard]] static QString defaultPath();

    [[nodiscard]] QStringList favourites() const { return m_favourites; }
    [[nodiscard]] bool isFavourite(const QString &pkg) const { return m_favourites.contains(pkg); }
    bool setFavourite(const QString &pkg, bool favourite);
    [[nodiscard]] bool rcsNoticeDismissed() const { return m_rcsDismissed; }
    bool setRcsNoticeDismissed(bool dismissed);
    // QML-facing setter for the property above (a failed write keeps the value for this session).
    void dismissRcsNotice(bool dismissed) { setRcsNoticeDismissed(dismissed); }

Q_SIGNALS:
    void changed();

private:
    void load();
    bool save();

    QString m_path;
    QStringList m_favourites;
    bool m_rcsDismissed = false;
};

} // namespace QindaQt::LinkApp
