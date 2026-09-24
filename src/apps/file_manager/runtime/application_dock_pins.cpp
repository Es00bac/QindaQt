// SPDX-License-Identifier: GPL-3.0-or-later
#include "application_dock_pins.h"

#include <qindaqt/services/dock_items/settings1_dock_pins.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>

#include <utility>

namespace QindaQt::Apps::FileManager {

using Services::DockItems::Settings1DockPins;
using Services::SettingsClient::SettingsClient;
using Services::SettingsClient::SettingsTransport;

ApplicationDockPins::ApplicationDockPins(std::unique_ptr<SettingsTransport> transport,
                                         QObject *parent)
    : QObject(parent), m_transport(std::move(transport)),
      m_client(std::make_unique<SettingsClient>(*m_transport, Settings1DockPins::scopedKeys())),
      m_pins(std::make_unique<Settings1DockPins>(*m_client)) {
  const auto changed = [this] { emit stateChanged(); };
  connect(m_pins.get(), &Settings1DockPins::pinsChanged, this, changed);
  connect(m_pins.get(), &Settings1DockPins::statusChanged, this, changed);
  connect(m_pins.get(), &Settings1DockPins::requestFinished, this, changed);
  connect(m_client.get(), &SettingsClient::stateChanged, this, changed);
  // Without a session bus or Settings1 the client stays unavailable, and so
  // does Keep in Dock; the window works as before.
  const bool started = m_client->start();
  Q_UNUSED(started);
}

ApplicationDockPins::~ApplicationDockPins() {
  // Stop before the pins helper and the transport it reads go away.
  m_client->stop();
}

void ApplicationDockPins::setApplicationId(const QString &applicationId) {
  if (m_applicationId == applicationId) {
    return;
  }
  m_applicationId = applicationId;
  emit stateChanged();
}

bool ApplicationDockPins::available() const {
  return !m_applicationId.isEmpty() && m_pins->isLoaded() && !m_pins->writePending();
}

bool ApplicationDockPins::pinned() const {
  return !m_applicationId.isEmpty() && m_pins->isPinned(m_applicationId);
}

bool ApplicationDockPins::toggle() {
  if (!available()) {
    return false;
  }
  const bool admitted = pinned() ? m_pins->unpinApplication(m_applicationId)
                                 : m_pins->pinApplication(m_applicationId);
  // Admission makes a write pending, which withdraws `available`.
  emit stateChanged();
  return admitted;
}

} // namespace QindaQt::Apps::FileManager
