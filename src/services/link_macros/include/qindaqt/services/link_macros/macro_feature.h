// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/input_actions/macro_runner.h>
#include <qindaqt/services/link_input/input_feature.h>
#include <qindaqt/services/link_macros/macro_catalog.h>
#include <qindaqt/services/link_macros/portal_macro_output.h>
#include <qindaqt/services/link_transport/hub.h>

#include <QtCore/QObject>

namespace QindaQt::Link::Macros {

// The `macro` capability (protocol page, "Macros (macro.*)").
//
// The phone sees only id and name of the user's existing library; steps never
// leave the desktop. A run is executed by the shared MacroRunner over the
// portal session and only for the phone that currently controls input, so one
// input.begin approval covers typing, pointing and macros alike.
//
// Ownership: borrows the hub, catalog, input feature and output; all must
// outlive it. Threading: GUI thread. Error behavior: every request with an id
// gets exactly one reply.
// AGENT-GUARD: a controller change, disconnect, lock or portal loss must cancel
// the run so the runner releases what it holds (MacroRunner::cancel).
class MacroFeature final : public QObject, public FeatureHandler {
    Q_OBJECT
public:
    MacroFeature(Hub &hub, MacroCatalog &catalog, InputFeature &input, PortalMacroOutput &output,
                 QObject *parent = nullptr);

    [[nodiscard]] QString ns() const override { return QStringLiteral("macro"); }
    void handle(Session &session, const Envelope &envelope) override;
    void sessionClosed(const QString &deviceId) override;

private:
    void run(Session &session, const Envelope &e);
    void syncReadySessions();
    void pushList(Session &session);
    void pushState(const QString &state, const QString &reason = {});
    void settle();
    void reply(Session &session, qint64 id, const QString &errorCode, const QString &message = {});

    Hub &m_hub;
    MacroCatalog &m_catalog;
    InputFeature &m_input;
    PortalMacroOutput &m_output;
    InputActions::MacroRunner m_runner;
    QHash<QString, Session *> m_known; // sessions that received the list
    QString m_runDevice, m_runMacro, m_failure;
    qint64 m_runRequest = 0;
    bool m_cancelRequested = false;
};

} // namespace QindaQt::Link::Macros
