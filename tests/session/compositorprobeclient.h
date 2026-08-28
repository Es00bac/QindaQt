// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRectF>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <memory>
#include <optional>

class QDBusInterface;

namespace QindaQt::Test {

struct ObservedWindow final
{
    QString id;
    QString title;
    QString containerId;
    // AGENT-CONTRACT: `frame` is the Wayland client's acknowledged geometry;
    // `targetFrame` is the compositor's committed logical target and remains
    // authoritative while an inactive minimized client cannot ack a configure.
    QRectF frame;
    QRectF targetFrame;
    bool minimized = false;
    bool active = false;
    bool skipTaskbar = false;
    bool skipSwitcher = false;
    bool keepAbove = false;
    bool keepBelow = false;
    int stackIndex = -1;
    bool serverDecorated = false;
    QString decorationClass;
};

using WindowInventory = QHash<QString, ObservedWindow>;

// Test-only client for the public compositor D-Bus surface. Keeping transport,
// schema checks, and event-loop-aware polling here prevents workflow scenarios
// from silently reaching through the process boundary they are meant to prove.
class CompositorProbeClient final : public QObject
{
    Q_OBJECT

public:
    CompositorProbeClient();
    ~CompositorProbeClient();

    CompositorProbeClient(const CompositorProbeClient &) = delete;
    CompositorProbeClient &operator=(const CompositorProbeClient &) = delete;

    [[nodiscard]] std::optional<QJsonObject> call(const QString &method, QString *error);
    [[nodiscard]] std::optional<QJsonObject> call(const QString &method, const QVariant &argument,
                                                  QString *error);
    [[nodiscard]] std::optional<QJsonObject>
    callWithArguments(const QString &method, const QVariantList &arguments, QString *error);

    [[nodiscard]] std::optional<QJsonArray> windows(QString *error);
    [[nodiscard]] std::optional<QJsonArray> outputs(QString *error);
    [[nodiscard]] std::optional<QJsonArray> containers(QString *error);
    [[nodiscard]] int outputsChangedCount() const noexcept;
    [[nodiscard]] std::optional<WindowInventory>
    awaitWindows(const QStringList &titles,
                 const std::function<bool(const WindowInventory &)> &ready, QString *error,
                 int timeoutMilliseconds = 2000);

    [[nodiscard]] std::optional<QJsonObject> dock(const QString &targetWindowId,
                                                  const QString &incomingWindowId, double ratio,
                                                  QString *error);

private:
    [[nodiscard]] std::optional<QJsonArray> arrayReply(const QString &method,
                                                       QLatin1StringView field, QString *error);

    std::unique_ptr<QDBusInterface> m_endpoint;
    int m_outputsChangedCount = 0;

private Q_SLOTS:
    void recordOutputsChanged();
};

[[nodiscard]] bool nearlyEqual(qreal first, qreal second);
[[nodiscard]] bool sameGeometry(const QRectF &first, const QRectF &second);

} // namespace QindaQt::Test
