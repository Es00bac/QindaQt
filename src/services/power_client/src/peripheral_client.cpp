// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/power_client/peripheral_client.h>
#include <QtCore/QPointer>
namespace QindaQt::Power {
PeripheralClient::PeripheralClient(PowerClient *authority,PeripheralTransport *transport,QObject *parent)
    : QObject(parent),m_authority(authority),m_transport(transport)
{
    Q_ASSERT(authority && transport);
    m_timeout.setSingleShot(true); m_timeout.setInterval(2000);
    connect(authority,&PowerClient::stateChanged,this,[this] { synchronize(); });
    connect(authority,&PowerClient::snapshotChanged,this,[this] { synchronize(); });
    connect(transport,&PeripheralTransport::receipt,this,&PeripheralClient::accept);
    connect(transport,&PeripheralTransport::invalidated,this,[this](const QString &owner,quint64 epoch,quint64 revision) {
        if (!m_running || owner!=m_owner || epoch!=m_epoch || revision==0) return;
        if (m_valid && revision<=m_snapshot.revision) return;
        m_minRevision=qMax(m_minRevision,revision); refresh();
    });
    connect(&m_timeout,&QTimer::timeout,this,[this] {
        m_pending=0; m_dirty=false; m_transport->cancel();
        clear(QStringLiteral("peripherals-unavailable"));
    });
}
PeripheralClient::~PeripheralClient() { m_transport->bind({}); }
void PeripheralClient::start() { if (!m_running) { m_running=true; synchronize(); } }
void PeripheralClient::stop()
{
    m_running=false; m_pending=0; m_dirty=false; m_epoch=0; m_minRevision=0;
    m_owner.clear(); m_timeout.stop(); m_transport->bind({});
    clear(QStringLiteral("stopped"));
}
void PeripheralClient::clear(const QString &reason)
{
    m_snapshot={}; m_valid=false; m_reason=reason;
    Q_EMIT changed();
}
void PeripheralClient::synchronize()
{
    if (!m_running) return;
    const auto state=m_authority->state();
    const bool usable=m_authority->hasSnapshot() && (state==PowerClientState::Ready || state==PowerClientState::Degraded);
    const QString owner=usable ? m_authority->owner() : QString{};
    const quint64 epoch=usable ? m_authority->snapshot().epoch : 0;
    if (owner==m_owner && epoch==m_epoch) return;
    m_pending=0; m_dirty=false; m_timeout.stop();
    m_owner=owner; m_epoch=epoch; m_minRevision=0; m_transport->bind(owner);
    QPointer<PeripheralClient> guard(this);
    clear(owner.isEmpty() ? QStringLiteral("peripherals-unavailable") : QStringLiteral("starting"));
    if (!guard || !m_running || m_owner!=owner || m_epoch!=epoch) return;
    if (!owner.isEmpty()) refresh();
}
void PeripheralClient::refresh()
{
    if (!m_running || m_owner.isEmpty() || m_epoch==0) return;
    if (m_pending) { m_dirty=true; return; }
    if (++m_nextToken==0) ++m_nextToken;
    m_pending=m_nextToken; m_timeout.start();
    m_transport->request(m_pending);
}
void PeripheralClient::accept(const QString &owner,quint64 token,const QByteArray &payload)
{
    if (!m_running || !m_pending || token!=m_pending || owner!=m_owner
        || owner!=m_authority->owner() || !m_authority->hasSnapshot()
        || m_epoch!=m_authority->snapshot().epoch) return;
    m_pending=0; m_timeout.stop();
    PeripheralSnapshot value;
    if (!decodePeripheralSnapshot(payload,value) || value.epoch!=m_epoch
        || (m_valid && (value.revision<m_snapshot.revision
            || (value.revision==m_snapshot.revision && value!=m_snapshot)))) {
        m_dirty=false; clear(QStringLiteral("peripherals-malformed")); return;
    }
    if (value.revision<m_minRevision) { m_dirty=false; refresh(); return; }
    m_snapshot=std::move(value); m_valid=true; m_reason=m_snapshot.reasonCode;
    const bool again=m_dirty; m_dirty=false;
    QPointer<PeripheralClient> guard(this);
    Q_EMIT changed();
    if (!guard || !m_running || owner!=m_owner) return;
    if (again) refresh();
}
}
