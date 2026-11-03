// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/recording_presenter.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace QindaQt::Link {

RecordingPresenter::RecordingPresenter(QString path, QObject *parent)
    : NotificationPresenter(parent), m_path(std::move(path))
{
    m_poll.setInterval(100);
    connect(&m_poll, &QTimer::timeout, this, [this] { pollCommands(); });
    m_poll.start();
}

void RecordingPresenter::append(const QJsonObject &line) const
{
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Append)) {
        f.write(QJsonDocument(line).toJson(QJsonDocument::Compact) + '\n');
    }
}

quint32 RecordingPresenter::present(const PresentedNotification &n, quint32 replaces)
{
    const quint32 handle = replaces != 0 ? replaces : ++m_next;
    QJsonArray keys;
    for (const auto &a : n.actions) keys.append(a.first);
    append({{QStringLiteral("op"), QStringLiteral("present")}, {QStringLiteral("handle"), double(handle)},
            {QStringLiteral("replaces"), double(replaces)}, {QStringLiteral("app"), n.appName},
            {QStringLiteral("summaryLen"), int(n.summary.size())}, {QStringLiteral("bodyLen"), int(n.body.size())},
            {QStringLiteral("summarySha256"),
             QString::fromLatin1(QCryptographicHash::hash(n.summary.toUtf8(), QCryptographicHash::Sha256).toHex())},
            {QStringLiteral("hasIcon"), !n.iconPath.isEmpty()}, {QStringLiteral("actions"), keys},
            {QStringLiteral("quiet"), n.quiet}});
    return handle;
}

void RecordingPresenter::close(quint32 handle)
{
    append({{QStringLiteral("op"), QStringLiteral("close")}, {QStringLiteral("handle"), double(handle)}});
}

void RecordingPresenter::pollCommands()
{
    const QString cmdPath = m_path + QStringLiteral(".cmd");
    QFile f(cmdPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;
    const QList<QByteArray> lines = f.readAll().split('\n');
    f.close();
    QFile::remove(cmdPath);
    for (const QByteArray &raw : lines) {
        const QList<QByteArray> w = raw.trimmed().split(' ');
        if (w.size() >= 2 && w[0] == "dismiss") Q_EMIT userDismissed(w[1].toUInt());
        if (w.size() >= 3 && w[0] == "action") Q_EMIT actionInvoked(w[1].toUInt(), QString::fromUtf8(w[2]));
    }
}

} // namespace QindaQt::Link
