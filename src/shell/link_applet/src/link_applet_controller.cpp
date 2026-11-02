// SPDX-License-Identifier: GPL-3.0-or-later
#include "link_applet_controller.h"

#include <QtCore/QDateTime>

#include <utility>

namespace QindaQt::Shell::LinkApplet
{

LinkAppletController::LinkAppletController(Link::LinkClient *client, const bool phoneReadGranted,
                                           const bool phoneControlGranted, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_read(phoneReadGranted)
    , m_control(phoneReadGranted && phoneControlGranted)
    , m_clock([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    if (m_read) {
        Q_ASSERT(m_client != nullptr);
        Q_ASSERT(m_client->thread() == thread());
        connect(m_client, &Link::LinkClient::availableChanged, this,
                &LinkAppletController::onDevicesChanged);
        connect(m_client, &Link::LinkClient::devicesChanged, this,
                &LinkAppletController::onDevicesChanged);
        connect(m_client, &Link::LinkClient::pairingRequested, this,
                &LinkAppletController::onPairingRequested);
        m_refresh.setInterval(60 * 1000);
        connect(&m_refresh, &QTimer::timeout, this, &LinkAppletController::reproject);
        m_refresh.start();
    }
    reproject();
}

LinkAppletController::~LinkAppletController() = default;

void LinkAppletController::setAppLaunch(Availability available, Launch launch)
{
    m_appAvailable = std::move(available);
    m_appLaunch = std::move(launch);
    refreshAppAvailability();
}

void LinkAppletController::setClock(Clock clock)
{
    m_clock = std::move(clock);
    reproject();
}

void LinkAppletController::refreshAppAvailability()
{
    const bool next = m_read && m_appAvailable && m_appAvailable();
    if (next != m_canOpenApp) {
        m_canOpenApp = next;
        Q_EMIT canOpenAppChanged();
    }
}

void LinkAppletController::onDevicesChanged()
{
    if (m_pairing.has_value()) {
        // The request ends when the service goes away or the phone is no
        // longer a live, still-unpaired connection (accepted elsewhere,
        // timed out, or disconnected).
        bool live = m_client->available();
        if (live) {
            live = false;
            for (const auto &d : m_client->devices()) {
                if (d.id == m_pairing->deviceId) {
                    live = d.connected && !(d.paired && d.ready);
                    break;
                }
            }
        }
        if (!live) {
            m_pairing.reset();
        }
    }
    reproject();
}

void LinkAppletController::onPairingRequested(const QString &id, const QString &name,
                                              const QString &model, const QString &code)
{
    if (formatPairingCode(code).isEmpty() || id.isEmpty()) {
        return; // not a six-digit code: never show something the user cannot compare
    }
    m_pairing = PendingPairing{id, name, model, code};
    reproject();
}

void LinkAppletController::reproject()
{
    ProjectionContext context;
    if (m_read && m_client != nullptr) {
        context.serviceAvailable = m_client->available();
        context.devices = m_client->devices();
        context.pairing = m_pairing;
    }
    context.nowMs = m_clock();
    // AGENT-NOTE: a denied read grant leaves the context at its defaults
    // (service absent), which projects to Unavailable without reading
    // `m_client`.
    LinkAppletModel next = projectLinkApplet(context);
    refreshAppAvailability();
    if (next == m_model) {
        return;
    }
    m_model = std::move(next);
    Q_EMIT modelChanged();
}

void LinkAppletController::acceptPairing()
{
    if (!m_control || m_client == nullptr || !m_model.hasPairing) {
        return;
    }
    const QString id = m_model.pairing.deviceId;
    m_pairing.reset();
    m_client->confirmPairing(id, true);
    reproject();
}

void LinkAppletController::rejectPairing()
{
    if (!m_control || m_client == nullptr || !m_model.hasPairing) {
        return;
    }
    const QString id = m_model.pairing.deviceId;
    m_pairing.reset();
    m_client->confirmPairing(id, false);
    reproject();
}

void LinkAppletController::ringPhone()
{
    // AGENT-GUARD: ring only a READY phone that advertised `ring`.
    if (!m_control || m_client == nullptr || !m_model.canRing) {
        return;
    }
    m_client->ring(m_model.deviceId);
}

void LinkAppletController::unpairPhone()
{
    if (!m_control || m_client == nullptr || m_model.deviceId.isEmpty()) {
        return;
    }
    m_client->unpair(m_model.deviceId);
}

void LinkAppletController::openApp()
{
    if (!m_canOpenApp || !m_appLaunch) {
        return;
    }
    m_appLaunch();
}

} // namespace QindaQt::Shell::LinkApplet
