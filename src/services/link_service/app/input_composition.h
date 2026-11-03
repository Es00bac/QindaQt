// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_input/input_feature.h>
#include <qindaqt/services/link_input/portal_session.h>
#include <qindaqt/services/link_macros/macro_catalog.h>
#include <qindaqt/services/link_macros/macro_feature.h>
#include <qindaqt/services/link_macros/portal_macro_output.h>
#include <qindaqt/services/link_transport/hub.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Link {

struct InputCompositionOptions {
    PortalOptions portal;             // portal bus name and restore-token file
    QString inputActionsService;      // empty = org.qindaqt.InputActions1
    InputGate *gate = nullptr;        // borrowed; null = closed (fail closed)
};

// Wires the `input` and `macro` capabilities into a hub: one portal session,
// one gate, one macro output and catalog, shared by both features
// (ADR-0436). Nothing here talks to the portal until a phone sends input.begin.
// Ownership: owns everything it creates except the borrowed gate and hub.
class InputComposition final : public QObject {
    Q_OBJECT
public:
    InputComposition(Hub &hub, QDBusConnection bus, InputCompositionOptions options, QObject *parent = nullptr);
    ~InputComposition() override;
    // Releases held input and closes the portal session; safe to call more than once.
    void shutdown();

private:
    StaticInputGate m_closedGate{false};
    InputGate *m_gate;
    PortalInputSession m_portal;
    InputFeature m_input;
    Macros::PortalMacroOutput m_output;
    Macros::MacroCatalog m_catalog;
    Macros::MacroFeature m_macros;
};

} // namespace QindaQt::Link
