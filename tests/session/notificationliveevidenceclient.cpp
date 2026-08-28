// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationliveevidenceclient.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QThread>

namespace QindaQt::Test {
namespace {

constexpr auto ServiceName = "org.qindaqt.ShellDevelopment";
constexpr auto ObjectPath = "/org/qindaqt/ShellDevelopment";
constexpr auto InterfaceName = "org.qindaqt.ShellDevelopment1";

std::optional<QJsonObject> decode(const QDBusReply<QByteArray> &reply, QString *error)
{
    if (!reply.isValid()) {
        *error =
            QStringLiteral("shell evidence D-Bus call failed: %1").arg(reply.error().message());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
        *error = QStringLiteral("shell evidence returned invalid schema-1 JSON: %1")
                     .arg(parseError.errorString());
        return std::nullopt;
    }
    return document.object();
}

} // namespace

NotificationLiveEvidenceClient::NotificationLiveEvidenceClient()
    : m_endpoint(std::make_unique<QDBusInterface>(
          QString::fromLatin1(ServiceName), QString::fromLatin1(ObjectPath),
          QString::fromLatin1(InterfaceName), QDBusConnection::sessionBus()))
{}

NotificationLiveEvidenceClient::~NotificationLiveEvidenceClient() = default;

bool NotificationLiveEvidenceClient::authenticate(qint64 expectedShellProcessId, QString *error)
{
    m_authenticatedShellProcessId = 0;
    if (expectedShellProcessId <= 1) {
        *error = QStringLiteral("expected shell PID is invalid");
        return false;
    }
    auto *const busInterface = QDBusConnection::sessionBus().interface();
    if (busInterface == nullptr) {
        *error = QStringLiteral("private bus has no daemon interface");
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 7'500) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const QDBusReply<uint> pid = busInterface->servicePid(QString::fromLatin1(ServiceName));
        if (pid.isValid() && static_cast<qint64>(pid.value()) == expectedShellProcessId) {
            m_authenticatedShellProcessId = expectedShellProcessId;
            auto current = snapshot(error);
            if (current
                && current->value(QStringLiteral("shellPid")).toString().toLongLong()
                       == expectedShellProcessId) {
                error->clear();
                return true;
            }
            m_authenticatedShellProcessId = 0;
        }
        QThread::msleep(20);
    }
    *error = QStringLiteral("shell evidence service did not authenticate to expected PID %1")
                 .arg(expectedShellProcessId);
    return false;
}

std::optional<QJsonObject> NotificationLiveEvidenceClient::snapshot(QString *error) const
{
    error->clear();
    if (m_authenticatedShellProcessId <= 1) {
        *error = QStringLiteral("shell evidence client is not authenticated");
        return std::nullopt;
    }
    auto current = decode(m_endpoint->call(QStringLiteral("Snapshot")), error);
    if (!current) {
        return std::nullopt;
    }
    const qint64 observedProcessId =
        current->value(QStringLiteral("shellPid")).toString().toLongLong();
    if (observedProcessId != m_authenticatedShellProcessId) {
        *error = QStringLiteral("shell evidence snapshot PID changed: authenticated=%1 observed=%2")
                     .arg(m_authenticatedShellProcessId)
                     .arg(observedProcessId);
        return std::nullopt;
    }
    return current;
}

std::optional<QJsonObject> NotificationLiveEvidenceClient::awaitSnapshot(
    const std::function<bool(const QJsonObject &)> &condition, QString *error,
    int timeoutMilliseconds) const
{
    QElapsedTimer timer;
    timer.start();
    QString lastError;
    std::optional<QJsonObject> lastSnapshot;
    while (timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QString snapshotError;
        auto current = snapshot(&snapshotError);
        if (current) {
            lastSnapshot = current;
            if (condition(*current)) {
                error->clear();
                return current;
            }
        } else {
            lastError = std::move(snapshotError);
        }
        QThread::msleep(20);
    }
    *error = !lastError.isEmpty()
                 ? lastError
                 : QStringLiteral("timed out waiting for shell evidence; last=%1")
                       .arg(lastSnapshot
                                ? QString::fromUtf8(
                                      QJsonDocument(*lastSnapshot).toJson(QJsonDocument::Compact))
                                : QStringLiteral("unavailable"));
    return std::nullopt;
}

QJsonObject presentationEvidence(const QJsonObject &snapshot)
{
    return snapshot.value(QStringLiteral("presentation")).toObject();
}

QJsonObject quietingEvidence(const QJsonObject &snapshot)
{
    return snapshot.value(QStringLiteral("quieting")).toObject();
}

QJsonObject windowEvidence(const QJsonObject &snapshot, QLatin1StringView role)
{
    return snapshot.value(QStringLiteral("windows")).toObject().value(role).toObject();
}

quint64 observationCount(const QJsonObject &snapshot, QLatin1StringView name)
{
    return snapshot.value(QStringLiteral("observations"))
        .toObject()
        .value(name)
        .toString()
        .toULongLong();
}

} // namespace QindaQt::Test
