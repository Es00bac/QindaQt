// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_composition.h"

namespace QindaQt::Link {

InputComposition::InputComposition(Hub &hub, QDBusConnection bus, InputCompositionOptions options, QObject *parent)
    : QObject(parent),
      m_gate(options.gate != nullptr ? options.gate : &m_closedGate),
      m_focus(options.focus != nullptr ? options.focus : &m_noFocus),
      m_portal(bus, std::move(options.portal)),
      m_input(m_portal, *m_gate),
      m_output(m_portal, *m_gate),
      m_catalog(bus, options.inputActionsService.isEmpty() ? QString::fromLatin1(InputActions::Service)
                                                           : options.inputActionsService),
      m_macros(hub, m_catalog, m_input, m_output),
      m_vault(m_portal, *m_gate, *m_focus)
{
    hub.addFeature(&m_input);
    hub.addFeature(&m_macros);
    m_vault.setInputInUse([this] { return !m_input.controller().isEmpty(); });
    connect(&m_vault, &VaultTypingFeature::portalBorrowedChanged, &m_input, &InputFeature::setPortalBorrowed);
    hub.addFeature(&m_vault);
    m_catalog.start();
}

void InputComposition::shutdown()
{
    m_vault.shutdown();
    m_output.releaseAll();
    m_input.shutdown();
}

InputComposition::~InputComposition() { shutdown(); }

} // namespace QindaQt::Link
