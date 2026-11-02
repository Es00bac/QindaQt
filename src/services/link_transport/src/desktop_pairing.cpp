// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/desktop_pairing.h>

#include <qindaqt/services/link_protocol/pairing_math.h>

#include <QtCore/QRandomGenerator>

namespace QindaQt::Link {
namespace {

QByteArray randomBytes(int n)
{
    QByteArray out(n, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(out.data()), n / 4);
    return out;
}

} // namespace

DesktopPairing::DesktopPairing(QByteArray spkiD, QByteArray spkiP, int timeoutMs, QObject *parent)
    : QObject(parent), m_spkiD(std::move(spkiD)), m_spkiP(std::move(spkiP))
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(timeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finishAbort(QStringLiteral("pair-timeout")); });
}

void DesktopPairing::begin(qint64 requestId)
{
    m_nd = randomBytes(32);
    m_state = State::AwaitNonce;
    m_timeout.start();
    Envelope commit;
    commit.t = QStringLiteral("pair.commit");
    commit.re = requestId;
    commit.b = QJsonObject{{QStringLiteral("c"),
                            QString::fromLatin1(pairingCommitment(m_nd, m_spkiD, m_spkiP).toHex())}};
    Q_EMIT frame(commit);
}

bool DesktopPairing::handle(const Envelope &f)
{
    if (f.t == QLatin1String("pair.nonce") && m_state == State::AwaitNonce) {
        const QJsonValue v = f.b.value(QStringLiteral("np"));
        const QByteArray np = QByteArray::fromHex(v.toString().toLatin1());
        if (!v.isString() || np.size() != 32) {
            return false;
        }
        Envelope reveal;
        reveal.t = QStringLiteral("pair.reveal");
        reveal.b = QJsonObject{{QStringLiteral("nd"), QString::fromLatin1(m_nd.toHex())}};
        m_state = State::Deciding;
        Q_EMIT frame(reveal);
        Q_EMIT codeReady(pairingCode(m_spkiD, m_spkiP, m_nd, np));
        return true;
    }
    if (f.t == QLatin1String("pair.confirm") && m_state == State::Deciding) {
        const QJsonValue v = f.b.value(QStringLiteral("accept"));
        if (!v.isBool()) {
            return false;
        }
        if (!v.toBool()) {
            finishAbort(QStringLiteral("pair-rejected"));
            return true;
        }
        m_peerAccepted = true;
        maybeComplete();
        return true;
    }
    return false;
}

void DesktopPairing::decide(bool accept)
{
    if (m_state != State::Deciding || m_localAccepted) {
        return;
    }
    Envelope confirm;
    confirm.t = QStringLiteral("pair.confirm");
    confirm.b = QJsonObject{{QStringLiteral("accept"), accept}};
    Q_EMIT frame(confirm);
    if (!accept) {
        finishAbort(QStringLiteral("pair-rejected"));
        return;
    }
    m_localAccepted = true;
    maybeComplete();
}

void DesktopPairing::maybeComplete()
{
    if (m_localAccepted && m_peerAccepted && m_state == State::Deciding) {
        m_state = State::Done;
        m_timeout.stop();
        Q_EMIT completed();
    }
}

void DesktopPairing::finishAbort(const QString &reason)
{
    if (!active()) {
        return;
    }
    m_state = State::Done;
    m_timeout.stop();
    Q_EMIT aborted(reason);
}

} // namespace QindaQt::Link
