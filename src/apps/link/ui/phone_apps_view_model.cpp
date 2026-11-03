// SPDX-License-Identifier: GPL-3.0-or-later
#include "phone_apps_view_model.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

#include <algorithm>

namespace QindaQt::LinkApp {

PhoneAppsViewModel::PhoneAppsViewModel(LinkApi &api, ProcessRunner &runner, AppPreferences &preferences, QObject *parent)
    : QObject(parent), m_api(api), m_runner(runner), m_prefs(preferences)
{
}

QList<QVariantMap> PhoneAppsViewModel::parseInventory(const QByteArray &json)
{
    // The mirror may print diagnostics before the document; the document is the last line that parses.
    QJsonObject root;
    for (const QByteArray &line : json.split('\n')) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.trimmed());
        if (doc.isObject() && doc.object().contains(QStringLiteral("apps"))) root = doc.object();
    }
    static const QRegularExpression packageShape(QRegularExpression::anchoredPattern(QStringLiteral("[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z0-9_]+)+")));
    QList<QVariantMap> rows;
    for (const QJsonValue &v : root.value(QStringLiteral("apps")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString pkg = o.value(QStringLiteral("package")).toString();
        if (!packageShape.match(pkg).hasMatch()) continue;
        const QString icon = o.value(QStringLiteral("icon")).toString();
        rows.append({{QStringLiteral("pkg"), pkg},
                     {QStringLiteral("label"), o.value(QStringLiteral("label")).toString(pkg)},
                     {QStringLiteral("iconUrl"), icon.isEmpty() ? QString() : QStringLiteral("data:image/png;base64,") + icon},
                     {QStringLiteral("favourite"), false}});
    }
    return rows;
}

void PhoneAppsViewModel::setState(const QString &state, const QString &message)
{
    m_state = state;
    m_message = message;
    Q_EMIT stateChanged();
}

void PhoneAppsViewModel::setSearchText(const QString &text)
{
    if (text == m_search) return;
    m_search = text;
    rebuild();
    Q_EMIT searchTextChanged();
}

void PhoneAppsViewModel::refresh()
{
    if (m_state == QLatin1String("loading")) return;
    const quint64 generation = ++m_generation;
    setState(QStringLiteral("loading"), tr("Asking the phone for its apps..."));
    m_api.adbDevices([this, generation](const QString &json, const QString &error) {
        if (generation != m_generation) return;
        m_serial.clear();
        for (const QJsonValue &v : QJsonDocument::fromJson(json.toUtf8()).array()) {
            const QJsonObject o = v.toObject();
            if (m_serial.isEmpty() || o.value(QStringLiteral("appInstalled")).toBool()) m_serial = o.value(QStringLiteral("serial")).toString();
        }
        if (!error.isEmpty() || m_serial.isEmpty()) {
            m_all.clear();
            rebuild();
            setState(QStringLiteral("needs-adb"),
                     tr("Phone apps need a USB cable with USB debugging allowed. Connect the phone and allow debugging, then refresh."));
            return;
        }
        m_runner.capture(m_program, {QStringLiteral("--serial"), m_serial, QStringLiteral("--list-json")},
                         [this, generation](int code, const QByteArray &out) {
                             if (generation != m_generation) return;
                             const QList<QVariantMap> rows = parseInventory(out);
                             if (code != 0 || rows.isEmpty()) {
                                 m_all.clear();
                                 rebuild();
                                 setState(code == -1 ? QStringLiteral("needs-adb") : QStringLiteral("error"),
                                          code == -1 ? tr("The phone did not answer over USB debugging. Unlock it and try again.")
                                                     : tr("The phone returned no apps. Open the QindaLink app on it and try again."));
                                 return;
                             }
                             m_all = rows;
                             rebuild();
                             setState(QStringLiteral("ready"), QString());
                         });
    });
}

void PhoneAppsViewModel::rebuild()
{
    const QString needle = m_search.trimmed().toLower();
    QList<QVariantMap> rows;
    for (QVariantMap row : std::as_const(m_all)) {
        const QString pkg = row.value(QStringLiteral("pkg")).toString();
        row.insert(QStringLiteral("favourite"), m_prefs.isFavourite(pkg));
        if (needle.isEmpty() || row.value(QStringLiteral("label")).toString().toLower().contains(needle)
            || pkg.toLower().contains(needle)) {
            rows.append(row);
        }
    }
    std::stable_sort(rows.begin(), rows.end(), [](const QVariantMap &a, const QVariantMap &b) {
        const bool fa = a.value(QStringLiteral("favourite")).toBool();
        const bool fb = b.value(QStringLiteral("favourite")).toBool();
        if (fa != fb) return fa;
        return QString::localeAwareCompare(a.value(QStringLiteral("label")).toString(), b.value(QStringLiteral("label")).toString()) < 0;
    });
    m_visible.setRows(rows);
}

void PhoneAppsViewModel::toggleFavourite(const QString &pkg)
{
    if (!m_prefs.setFavourite(pkg, !m_prefs.isFavourite(pkg))) {
        setState(m_state, tr("Could not save your favourites."));
    }
    rebuild();
}

bool PhoneAppsViewModel::open(const QString &pkg)
{
    static const QRegularExpression packageShape(QRegularExpression::anchoredPattern(QStringLiteral("[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z0-9_]+)+")));
    if (m_serial.isEmpty() || !packageShape.match(pkg).hasMatch()) return false;
    if (!m_runner.startDetached(m_program, {QStringLiteral("--serial"), m_serial, QStringLiteral("--package"), pkg})) {
        setState(m_state, tr("Could not start qindaqt-link-mirror."));
        return false;
    }
    return true;
}

} // namespace QindaQt::LinkApp
