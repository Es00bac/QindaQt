// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/voice_applet/voice_service_client_adapter.h>

namespace QindaQt::Shell::VoiceApplet {

using Services::Voice::ClientState;
using Services::Voice::OperationKind;
using Services::Voice::Snapshot;

VoiceServiceClientAdapter::VoiceServiceClientAdapter(Services::Voice::VoiceClient *client,
                                                     QObject *parent)
    : VoiceClientInterface(parent), m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    connect(m_client, &Services::Voice::VoiceClient::stateChanged, this,
            &VoiceClientInterface::stateChanged);
    connect(m_client, &Services::Voice::VoiceClient::snapshotChanged, this,
            &VoiceClientInterface::snapshotChanged);
    connect(m_client, &Services::Voice::VoiceClient::levelChanged, this,
            &VoiceClientInterface::levelChanged);
    connect(m_client, &Services::Voice::VoiceClient::operationCompleted, this,
            &VoiceClientInterface::operationCompleted);
}

ClientState VoiceServiceClientAdapter::clientState() const noexcept
{
    return m_client->state();
}

QString VoiceServiceClientAdapter::reasonCode() const { return m_client->reasonCode(); }

bool VoiceServiceClientAdapter::hasSnapshot() const noexcept
{
    return m_client->hasSnapshot();
}

Snapshot VoiceServiceClientAdapter::snapshot() const { return m_client->snapshot(); }

quint32 VoiceServiceClientAdapter::levelPercent() const noexcept
{
    return m_client->levelPercent();
}

void VoiceServiceClientAdapter::refresh()
{
    // AGENT-GUARD: expanding the popup is not permission to activate Voice1.
    // The client already refetches on owner/invalidation signals while
    // running; only VoiceAppletComposition's confirmed Settings1 gate starts
    // a stopped client.
}

quint64 VoiceServiceClientAdapter::submit(const OperationKind kind,
                                          const QString &providerId, const bool enable)
{
    switch (kind) {
    case OperationKind::StartDictation: return m_client->startDictation();
    case OperationKind::StartCommand:   return m_client->startCommand();
    case OperationKind::Finish:         return m_client->finish();
    case OperationKind::Cancel:         return m_client->cancel();
    case OperationKind::Retry:          return m_client->retry();
    case OperationKind::Undo:           return m_client->undo();
    case OperationKind::CopyLast:       return m_client->copyLast();
    case OperationKind::SetProvider:    return m_client->setProvider(providerId);
    case OperationKind::SetEnabled:     return m_client->setEnabled(enable);
    }
    return 0;
}

} // namespace QindaQt::Shell::VoiceApplet
