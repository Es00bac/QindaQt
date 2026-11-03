// SPDX-License-Identifier: GPL-3.0-or-later
#include "devices_view_model.h"

#include "message_json.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace QindaQt::LinkApp {
namespace {

QString errorText(const QString &code)
{
    if (code == QLatin1String("unavailable") || code == QLatin1String("not-connected")) {
        return QObject::tr("The QindaLink service is not running.");
    }
    if (code == QLatin1String("timeout")) return QObject::tr("The service did not answer in time.");
    return QObject::tr("The QindaLink service reported an error (%1).").arg(code);
}

QString stepLabel(const QString &id)
{
    if (id.startsWith(QLatin1String("grant:"))) {
        static const QHash<QString, QString> names{
            {QStringLiteral("READ_SMS"), QObject::tr("Read messages")},
            {QStringLiteral("SEND_SMS"), QObject::tr("Send messages")},
            {QStringLiteral("RECEIVE_SMS"), QObject::tr("Receive messages")},
            {QStringLiteral("READ_CONTACTS"), QObject::tr("Read contact names")},
            {QStringLiteral("POST_NOTIFICATIONS"), QObject::tr("Post notifications")},
            {QStringLiteral("READ_PHONE_STATE"), QObject::tr("Read phone state")}};
        return names.value(id.mid(6), id.mid(6));
    }
    if (id == QLatin1String("restricted-settings")) return QObject::tr("Allow restricted settings");
    if (id == QLatin1String("notification-listener")) return QObject::tr("Notification access");
    if (id == QLatin1String("battery-whitelist")) return QObject::tr("Exempt from battery optimisation");
    return id;
}

} // namespace

DevicesViewModel::DevicesViewModel(LinkApi &api, QObject *parent)
    : QObject(parent), m_api(api), m_clock([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    connect(&m_api, &LinkApi::availableChanged, this, [this] {
        if (m_api.available()) refresh(); else { applyDevices(QStringLiteral("[]")); Q_EMIT changed(); }
    });
    connect(&m_api, &LinkApi::devicesChanged, this, &DevicesViewModel::applyDevices);
    connect(&m_api, &LinkApi::pairingRequested, this, &DevicesViewModel::addPairing);
}

void DevicesViewModel::setError(const QString &error)
{
    if (error == m_error) return;
    m_error = error;
    Q_EMIT changed();
}

void DevicesViewModel::refresh()
{
    const quint64 generation = ++m_generation;
    m_api.devices([this, generation](const QString &json, const QString &error) {
        if (generation != m_generation) return;
        if (!error.isEmpty()) {
            applyDevices(QStringLiteral("[]"));
            setError(errorText(error));
            return;
        }
        setError({});
        applyDevices(json);
    });
    m_api.pendingPairings([this](const QString &json, const QString &error) {
        // "unsupported": the service predates PendingPairings, so only live requests are shown.
        if (error.isEmpty()) applyPending(json);
    });
}

QVariantMap DevicesViewModel::describe(const Link::LinkDevice &d) const
{
    QString battery;
    if (d.batteryLevel >= 0) {
        battery = QStringLiteral("%1%").arg(d.batteryLevel);
        if (d.charging) battery += tr(" charging");
    }
    QString network;
    if (!d.networkType.isEmpty()) {
        network = d.networkType == QLatin1String("wifi") ? tr("Wi-Fi")
                : d.networkType == QLatin1String("mobile") ? tr("Mobile data") : d.networkType;
        if (d.signal >= 0) network += tr(" (signal %1/4)").arg(d.signal);
    }
    const QString transport = d.transport == QLatin1String("usb") ? tr("USB")
                            : d.transport == QLatin1String("lan") ? tr("Network") : tr("Not connected");
    const qint64 now = m_clock();
    QString lastSeen = d.connected ? tr("Now") : d.lastSeen > 0 ? formatWhen(d.lastSeen, now) : tr("Never");
    QVariantMap out;
    out.insert(QStringLiteral("id"), d.id);
    out.insert(QStringLiteral("name"), d.name.isEmpty() ? tr("Phone") : d.name);
    out.insert(QStringLiteral("model"), d.model);
    out.insert(QStringLiteral("paired"), d.paired);
    out.insert(QStringLiteral("connected"), d.connected);
    out.insert(QStringLiteral("ready"), d.ready);
    out.insert(QStringLiteral("transport"), transport);
    out.insert(QStringLiteral("battery"), battery);
    out.insert(QStringLiteral("batteryLevel"), d.batteryLevel);
    out.insert(QStringLiteral("charging"), d.charging);
    out.insert(QStringLiteral("network"), network);
    out.insert(QStringLiteral("lastSeen"), lastSeen);
    out.insert(QStringLiteral("caps"), d.capabilities);
    return out;
}

void DevicesViewModel::applyDevices(const QString &json)
{
    m_devices = Link::parseDevices(json.toUtf8());
    // One phone is the product; show the most useful of the listed ones.
    const Link::LinkDevice *best = nullptr;
    for (const Link::LinkDevice &d : std::as_const(m_devices)) {
        auto rank = [](const Link::LinkDevice &x) { return (x.ready ? 4 : 0) + (x.connected ? 2 : 0) + (x.paired ? 1 : 0); };
        if (best == nullptr || rank(d) > rank(*best)) best = &d;
    }
    const QString before = deviceId();
    m_hasDevice = best != nullptr;
    m_device = best != nullptr ? describe(*best) : QVariantMap();
    dropStalePairings();
    Q_EMIT changed();
    if (before != deviceId()) Q_EMIT deviceIdChanged(deviceId());
}

void DevicesViewModel::dropStalePairings()
{
    for (int i = m_pairings.count() - 1; i >= 0; --i) {
        const QString id = m_pairings.get(i).value(QStringLiteral("id")).toString();
        bool pending = false;
        for (const Link::LinkDevice &d : std::as_const(m_devices)) {
            pending = pending || (d.id == id && d.connected && !d.ready);
        }
        if (!pending) m_pairings.removeAt(i);
    }
}

void DevicesViewModel::applyPending(const QString &json)
{
    for (const QJsonValue &v : QJsonDocument::fromJson(json.toUtf8()).array()) {
        const QJsonObject o = v.toObject();
        addPairing(o.value(QStringLiteral("id")).toString(), o.value(QStringLiteral("name")).toString(),
                   o.value(QStringLiteral("model")).toString(), o.value(QStringLiteral("code")).toString());
    }
}

void DevicesViewModel::addPairing(const QString &id, const QString &name, const QString &model, const QString &code)
{
    if (id.isEmpty()) return;
    const QVariantMap row{{QStringLiteral("id"), id},
                          {QStringLiteral("name"), name.isEmpty() ? tr("Phone") : name},
                          {QStringLiteral("model"), model},
                          {QStringLiteral("code"), formatPairingCode(code)}};
    const int existing = m_pairings.indexOf(QStringLiteral("id"), id);
    if (existing >= 0) m_pairings.replaceAt(existing, row); else m_pairings.append({row});
    Q_EMIT changed();
}

void DevicesViewModel::decide(const QString &id, bool accept)
{
    m_api.confirmPairing(id, accept);
    const int row = m_pairings.indexOf(QStringLiteral("id"), id);
    if (row >= 0) m_pairings.removeAt(row);
    Q_EMIT changed();
}

void DevicesViewModel::unpair(const QString &id)
{
    m_api.unpair(id);
}

void DevicesViewModel::withSerial(std::function<void(const QString &)> use)
{
    m_api.adbDevices([this, use = std::move(use)](const QString &json, const QString &error) {
        QString serial;
        if (error.isEmpty()) {
            for (const QJsonValue &v : QJsonDocument::fromJson(json.toUtf8()).array()) {
                const QJsonObject o = v.toObject();
                if (serial.isEmpty() || o.value(QStringLiteral("appInstalled")).toBool()) {
                    serial = o.value(QStringLiteral("serial")).toString();
                }
            }
        }
        m_adbSerial = serial;
        if (serial.isEmpty()) {
            m_setupBusy = false;
            m_setupMessage = error.isEmpty() ? tr("No phone is connected over USB debugging. Connect it with a cable and allow USB debugging.")
                                             : errorText(error);
            m_steps.setRows({});
            Q_EMIT setupChanged();
            return;
        }
        use(serial);
    });
}

void DevicesViewModel::setupPhone()
{
    if (m_setupBusy) return;
    m_setupBusy = true;
    m_setupMessage = tr("Setting up the phone...");
    Q_EMIT setupChanged();
    withSerial([this](const QString &serial) {
        m_api.setupPhone(serial, [this](const QString &json, const QString &error) {
            m_setupBusy = false;
            if (!error.isEmpty()) {
                m_setupMessage = errorText(error);
                Q_EMIT setupChanged();
                return;
            }
            showReport(json, false);
        });
    });
}

void DevicesViewModel::checkSetup()
{
    if (m_setupBusy) return;
    withSerial([this](const QString &serial) {
        m_api.setupStatus(serial, [this](const QString &json, const QString &error) {
            if (!error.isEmpty()) {
                m_setupMessage = errorText(error);
                Q_EMIT setupChanged();
                return;
            }
            showReport(json, true);
        });
    });
}

void DevicesViewModel::showReport(const QString &json, bool isStatus)
{
    const QJsonObject report = QJsonDocument::fromJson(json.toUtf8()).object();
    if (report.value(QStringLiteral("error")).toString() == QLatin1String("busy")) {
        m_setupMessage = tr("Setup is already running for this phone. Wait for it to finish.");
        Q_EMIT setupChanged();
        return;
    }
    QList<QVariantMap> rows;
    int bad = 0;
    for (const QJsonValue &v : report.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject o = v.toObject();
        const bool ok = o.value(QStringLiteral("ok")).toBool();
        bad += ok ? 0 : 1;
        const QString id = o.value(QStringLiteral("id")).toString();
        rows.append({{QStringLiteral("id"), id},
                     {QStringLiteral("label"), stepLabel(id)},
                     {QStringLiteral("ok"), ok},
                     {QStringLiteral("outcome"), ok ? (isStatus ? tr("Granted") : tr("Done")) : (isStatus ? tr("Not granted") : tr("Failed"))}});
    }
    m_steps.setRows(rows);
    if (rows.isEmpty()) m_setupMessage = tr("The service returned no setup steps.");
    else if (bad == 0) m_setupMessage = isStatus ? tr("The phone is set up.") : tr("The phone is set up. Open QindaLink on it to finish.");
    else m_setupMessage = isStatus ? tr("%n step(s) still need a grant. Run Set up phone.", nullptr, bad)
                                   : tr("%n step(s) failed. Check that the QindaLink app is installed and the phone is unlocked.", nullptr, bad);
    Q_EMIT setupChanged();
}

} // namespace QindaQt::LinkApp
