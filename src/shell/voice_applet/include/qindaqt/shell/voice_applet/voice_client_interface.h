// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>

#include <QtCore/QObject>

namespace QindaQt::Shell::VoiceApplet {

// Injected least-authority seam between the applet controller and the Voice1
// consumer stack.
//
// AGENT-CONTRACT: the controller depends only on this interface. It never
// opens a session bus, never speaks to a provider, and never learns a provider
// process identity; that is what lets the controller and its projection be
// tested from literals with no bus and no installed provider.
//
// Threading: GUI-thread confined. Every virtual is invoked on the controller's
// thread and every signal must be emitted there.
//
// Lifetime: the controller borrows the seam and never deletes it. The composing
// shell guarantees the seam outlives the controller.
//
// Results: submit() reports nothing synchronously. Exactly one
// operationCompleted carries the returned request id, and a returned id of 0
// means no request was made and no completion will arrive.
class VoiceClientInterface : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~VoiceClientInterface() override = default;

    [[nodiscard]] virtual Services::Voice::ClientState clientState() const noexcept = 0;
    [[nodiscard]] virtual QString reasonCode() const = 0;
    [[nodiscard]] virtual bool hasSnapshot() const noexcept = 0;
    [[nodiscard]] virtual Services::Voice::Snapshot snapshot() const = 0;
    [[nodiscard]] virtual quint32 levelPercent() const noexcept = 0;

    // Asks the consumer stack to refresh its projection. Advisory: a seam that
    // is already current may do nothing.
    virtual void refresh() = 0;

    [[nodiscard]] virtual quint64 submit(Services::Voice::OperationKind kind,
                                         const QString &providerId, bool enable) = 0;

Q_SIGNALS:
    void stateChanged(QindaQt::Services::Voice::ClientState state,
                      const QString &reasonCode);
    void snapshotChanged(const QindaQt::Services::Voice::Snapshot &snapshot);
    void levelChanged(quint32 levelPercent);
    void operationCompleted(quint64 requestId,
                            const QindaQt::Services::Voice::OperationResult &result);
};

} // namespace QindaQt::Shell::VoiceApplet
