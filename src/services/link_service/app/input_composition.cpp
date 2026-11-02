// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_composition.h"

namespace QindaQt::Link {

InputComposition::InputComposition(Hub &hub, QDBusConnection bus, InputCompositionOptions options, QObject *parent)
    : QObject(parent),
      m_gate(options.gate != nullptr ? options.gate : &m_closedGate),
      m_portal(bus, std::move(options.portal)),
      m_input(m_portal, *m_gate),
      m_output(m_portal, *m_gate),
      m_catalog(bus, options.inputActionsService.isEmpty() ? QString::fromLatin1(InputActions::Service)
                                                           : options.inputActionsService),
      m_macros(hub, m_catalog, m_input, m_output)
{
    hub.addFeature(&m_input);
    hub.addFeature(&m_macros);
    m_catalog.start();
}

InputComposition::~InputComposition()
{
    m_output.releaseAll();
}

} // namespace QindaQt::Link
