// SPDX-License-Identifier: GPL-3.0-or-later
#include "pluginunloadworkflow.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRectF>
#include <QThread>
#include <QVariantMap>
#include <QWindow>

#include <cmath>
#include <functional>
#include <optional>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr auto CompositorPath = "/org/qindaqt/Compositor";
constexpr auto CompositorInterface = "org.qindaqt.Compositor1";
constexpr auto KWinService = "org.kde.KWin";
constexpr auto KWinPath = "/KWin";
constexpr auto KWinInterface = "org.kde.KWin";
constexpr auto PluginPath = "/Plugins";
constexpr auto PluginInterface = "org.kde.KWin.Plugins";
constexpr auto PluginId = "qindaqt_compositor";

std::optional<QJsonObject> decode(const QDBusReply<QByteArray> &reply,
                                  const QString &operation,
                                  QString *error)
{
    if (!reply.isValid()) {
        *error = QStringLiteral("%1 D-Bus call failed: %2")
                     .arg(operation, reply.error().message());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply.value(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("%1 returned invalid JSON: %2")
                     .arg(operation, parseError.errorString());
        return std::nullopt;
    }
    return document.object();
}

std::optional<QJsonObject> compositorCall(QDBusInterface &endpoint,
                                          const QString &method,
                                          const QVariantList &arguments,
                                          QString *error)
{
    const QDBusReply<QByteArray> reply = endpoint.callWithArgumentList(
        QDBus::Block, method, arguments);
    return decode(reply, method, error);
}

std::optional<QJsonObject> listedWindow(const QJsonArray &windows,
                                        const QString &title)
{
    for (const auto &entry : windows) {
        const auto window = entry.toObject();
        if (window.value(QStringLiteral("title")).toString() == title) {
            return window;
        }
    }
    return std::nullopt;
}

std::optional<QJsonArray> compositorWindows(QDBusInterface &endpoint, QString *error)
{
    const auto result = compositorCall(endpoint, QStringLiteral("Windows"), {}, error);
    if (!result || result->value(QStringLiteral("status")) != QStringLiteral("ok")
        || !result->value(QStringLiteral("windows")).isArray()) {
        if (error->isEmpty()) {
            *error = QStringLiteral("Windows returned an unexpected response");
        }
        return std::nullopt;
    }
    return result->value(QStringLiteral("windows")).toArray();
}

QRectF compositorFrame(const QJsonObject &window)
{
    const auto geometry = window.value(QStringLiteral("geometry")).toObject();
    return {geometry.value(QStringLiteral("x")).toDouble(),
            geometry.value(QStringLiteral("y")).toDouble(),
            geometry.value(QStringLiteral("width")).toDouble(),
            geometry.value(QStringLiteral("height")).toDouble()};
}

QVariant unwrapped(QVariant value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        return value.value<QDBusVariant>().variant();
    }
    return value;
}

std::optional<QVariantMap> coreWindowInfo(QDBusInterface &kwin,
                                          const QString &windowId,
                                          QString *error)
{
    const QDBusReply<QVariantMap> reply = kwin.call(QStringLiteral("getWindowInfo"), windowId);
    if (!reply.isValid()) {
        *error = QStringLiteral("KWin getWindowInfo failed: %1").arg(reply.error().message());
        return std::nullopt;
    }
    if (reply.value().isEmpty()) {
        *error = QStringLiteral("KWin no longer reports window '%1'").arg(windowId);
        return std::nullopt;
    }
    return reply.value();
}

QRectF coreFrame(const QVariantMap &window)
{
    return {unwrapped(window.value(QStringLiteral("x"))).toDouble(),
            unwrapped(window.value(QStringLiteral("y"))).toDouble(),
            unwrapped(window.value(QStringLiteral("width"))).toDouble(),
            unwrapped(window.value(QStringLiteral("height"))).toDouble()};
}

QString coreCaption(const QVariantMap &window)
{
    return unwrapped(window.value(QStringLiteral("caption"))).toString();
}

bool coreMinimized(const QVariantMap &window)
{
    return unwrapped(window.value(QStringLiteral("minimized"))).toBool();
}

bool nearlyEqual(qreal first, qreal second)
{
    return std::abs(first - second) <= 1.0;
}

bool sameFrame(const QRectF &first, const QRectF &second)
{
    return nearlyEqual(first.x(), second.x()) && nearlyEqual(first.y(), second.y())
        && nearlyEqual(first.width(), second.width())
        && nearlyEqual(first.height(), second.height());
}

QJsonObject frameJson(const QRectF &frame)
{
    return {{QStringLiteral("x"), frame.x()},
            {QStringLiteral("y"), frame.y()},
            {QStringLiteral("width"), frame.width()},
            {QStringLiteral("height"), frame.height()}};
}

bool await(const std::function<bool()> &condition, int timeoutMilliseconds = 2500)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (condition()) {
            return true;
        }
        QThread::msleep(10);
    }
    return false;
}

bool serviceIsRegistered(QLatin1StringView service)
{
    const auto reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(
        QString(service));
    return reply.isValid() && reply.value();
}

class PluginUnloadWorkflow final
{
public:
    PluginUnloadWorkflow(QWindow &primary,
                         QWindow &secondary,
                         QWindow &tertiary,
                         QWindow &quaternary)
        : m_primary(primary)
        , m_secondary(secondary)
        , m_tertiary(tertiary)
        , m_quaternary(quaternary)
        , m_endpoint(QString::fromLatin1(CompositorService),
                     QString::fromLatin1(CompositorPath),
                     QString::fromLatin1(CompositorInterface))
        , m_kwin(QString::fromLatin1(KWinService),
                 QString::fromLatin1(KWinPath),
                 QString::fromLatin1(KWinInterface))
        , m_plugins(QString::fromLatin1(KWinService),
                    QString::fromLatin1(PluginPath),
                    QString::fromLatin1(PluginInterface))
    {
    }

    PluginUnloadResult run()
    {
        if (!m_endpoint.isValid() || !m_kwin.isValid() || !m_plugins.isValid()) {
            fail(QStringLiteral("required compositor/KWin D-Bus interfaces are unavailable"));
            return m_result;
        }
        if (!discoverWindows() || !groupWindows() || !unloadPlugin()
            || !verifyRestoredFrames() || !verifyClientUsability()) {
            return m_result;
        }
        m_result.evidence = {
            {QStringLiteral("containerId"), m_containerId},
            {QStringLiteral("secondContainerId"), m_secondContainerId},
            {QStringLiteral("primaryWindowId"), m_primaryId},
            {QStringLiteral("secondaryWindowId"), m_secondaryId},
            {QStringLiteral("tertiaryWindowId"), m_tertiaryId},
            {QStringLiteral("quaternaryWindowId"), m_quaternaryId},
            {QStringLiteral("primaryRestoreFrame"), frameJson(m_primaryRestoreFrame)},
            {QStringLiteral("secondaryRestoreFrame"), frameJson(m_secondaryRestoreFrame)},
            {QStringLiteral("tertiaryRestoreFrame"), frameJson(m_tertiaryRestoreFrame)},
            {QStringLiteral("quaternaryRestoreFrame"), frameJson(m_quaternaryRestoreFrame)},
            {QStringLiteral("observer"), QStringLiteral("org.kde.KWin.getWindowInfo")},
        };
        return m_result;
    }

private:
    void fail(QString failure)
    {
        m_result.failure = std::move(failure);
    }

    bool discoverWindows()
    {
        std::optional<QJsonObject> primaryWindow;
        std::optional<QJsonObject> secondaryWindow;
        std::optional<QJsonObject> tertiaryWindow;
        std::optional<QJsonObject> quaternaryWindow;
        const bool discovered = await([&] {
            m_error.clear();
            const auto windows = compositorWindows(m_endpoint, &m_error);
            if (!windows) {
                return false;
            }
            primaryWindow = listedWindow(*windows, m_primary.title());
            secondaryWindow = listedWindow(*windows, m_secondary.title());
            tertiaryWindow = listedWindow(*windows, m_tertiary.title());
            quaternaryWindow = listedWindow(*windows, m_quaternary.title());
            return primaryWindow.has_value() && secondaryWindow.has_value()
                && tertiaryWindow.has_value() && quaternaryWindow.has_value();
        });
        if (!discovered) {
            fail(m_error.isEmpty() ? QStringLiteral("probe windows were not discovered")
                                   : m_error);
            return false;
        }
        m_primaryId = primaryWindow->value(QStringLiteral("id")).toString();
        m_secondaryId = secondaryWindow->value(QStringLiteral("id")).toString();
        m_tertiaryId = tertiaryWindow->value(QStringLiteral("id")).toString();
        m_quaternaryId = quaternaryWindow->value(QStringLiteral("id")).toString();
        m_primaryRestoreFrame = compositorFrame(*primaryWindow);
        m_secondaryRestoreFrame = compositorFrame(*secondaryWindow);
        m_tertiaryRestoreFrame = compositorFrame(*tertiaryWindow);
        m_quaternaryRestoreFrame = compositorFrame(*quaternaryWindow);
        return true;
    }

    bool dockPair(const QString &firstId,
                  const QString &secondId,
                  const QString &orientation,
                  QString *containerId)
    {
        const auto reply = compositorCall(
            m_endpoint, QStringLiteral("DockWindows"),
            {firstId, secondId, orientation, QStringLiteral("second"), 0.5},
            &m_error);
        if (!reply || reply->value(QStringLiteral("status")) != QStringLiteral("docked")) {
            fail(m_error.isEmpty() ? QStringLiteral("DockWindows did not commit") : m_error);
            return false;
        }
        *containerId = reply->value(QStringLiteral("containerId")).toString();
        return true;
    }

    bool groupWindows()
    {
        if (!dockPair(m_primaryId, m_secondaryId, QStringLiteral("horizontal"),
                      &m_containerId)
            || !dockPair(m_tertiaryId, m_quaternaryId, QStringLiteral("vertical"),
                         &m_secondContainerId)) {
            return false;
        }
        m_result.grouped = await([&] {
            m_error.clear();
            const auto windows = compositorWindows(m_endpoint, &m_error);
            const auto first = windows ? listedWindow(*windows, m_primary.title()) : std::nullopt;
            const auto second = windows ? listedWindow(*windows, m_secondary.title()) : std::nullopt;
            const auto third = windows ? listedWindow(*windows, m_tertiary.title()) : std::nullopt;
            const auto fourth = windows ? listedWindow(*windows, m_quaternary.title()) : std::nullopt;
            if (!first || !second || !third || !fourth) {
                return false;
            }
            const auto firstFrame = compositorFrame(*first);
            const auto secondFrame = compositorFrame(*second);
            const auto thirdFrame = compositorFrame(*third);
            const auto fourthFrame = compositorFrame(*fourth);
            return first->value(QStringLiteral("containerId")) == m_containerId
                && second->value(QStringLiteral("containerId")) == m_containerId
                && nearlyEqual(firstFrame.x() + firstFrame.width(), secondFrame.x())
                && !sameFrame(firstFrame, m_primaryRestoreFrame)
                && !sameFrame(secondFrame, m_secondaryRestoreFrame)
                && third->value(QStringLiteral("containerId")) == m_secondContainerId
                && fourth->value(QStringLiteral("containerId")) == m_secondContainerId
                && nearlyEqual(thirdFrame.y() + thirdFrame.height(), fourthFrame.y())
                && !sameFrame(thirdFrame, m_tertiaryRestoreFrame)
                && !sameFrame(fourthFrame, m_quaternaryRestoreFrame);
        });
        if (!m_result.grouped) {
            fail(m_error.isEmpty() ? QStringLiteral("docked frames never became observable")
                                   : m_error);
        }
        return m_result.grouped;
    }

    bool unloadPlugin()
    {
        // KWin 6.6.5 exposes the runtime binary-plugin manager at this exact
        // service/path/interface. The void call returns only after erase() has
        // run the plugin destructor on KWin's compositor thread.
        const auto reply = m_plugins.call(QStringLiteral("UnloadPlugin"),
                                          QString::fromLatin1(PluginId));
        m_result.unloadCallSucceeded = reply.type() != QDBusMessage::ErrorMessage;
        if (!m_result.unloadCallSucceeded) {
            fail(QStringLiteral("UnloadPlugin failed: %1").arg(reply.errorMessage()));
            return false;
        }
        m_result.serviceRemoved = await([] {
            return !serviceIsRegistered(QLatin1StringView(CompositorService));
        });
        m_result.pluginRemoved = !m_plugins.property("LoadedPlugins").toStringList().contains(
            QString::fromLatin1(PluginId));
        if (!m_result.serviceRemoved || !m_result.pluginRemoved) {
            fail(QStringLiteral("KWin did not finish unloading the QindaQt plugin"));
            return false;
        }
        return true;
    }

    bool verifyRestoredFrames()
    {
        m_result.framesRestored = await([&] {
            m_error.clear();
            const auto first = coreWindowInfo(m_kwin, m_primaryId, &m_error);
            const auto second = coreWindowInfo(m_kwin, m_secondaryId, &m_error);
            const auto third = coreWindowInfo(m_kwin, m_tertiaryId, &m_error);
            const auto fourth = coreWindowInfo(m_kwin, m_quaternaryId, &m_error);
            return first && second && third && fourth
                && sameFrame(coreFrame(*first), m_primaryRestoreFrame)
                && sameFrame(coreFrame(*second), m_secondaryRestoreFrame)
                && sameFrame(coreFrame(*third), m_tertiaryRestoreFrame)
                && sameFrame(coreFrame(*fourth), m_quaternaryRestoreFrame)
                && !coreMinimized(*first) && !coreMinimized(*second)
                && !coreMinimized(*third) && !coreMinimized(*fourth);
        });
        if (!m_result.framesRestored) {
            fail(m_error.isEmpty() ? QStringLiteral("core KWin observer saw stranded frames")
                                   : m_error);
        }
        return m_result.framesRestored;
    }

    bool verifyClientUsability()
    {
        const auto usableTitle = QStringLiteral("QindaQt plugin unload primary usable");
        const QSize requestedSize(qRound(m_primaryRestoreFrame.width()) + 31,
                                  qRound(m_primaryRestoreFrame.height()) + 19);
        m_primary.setTitle(usableTitle);
        m_primary.resize(requestedSize);
        m_result.clientsUsable = await([&] {
            m_error.clear();
            const auto first = coreWindowInfo(m_kwin, m_primaryId, &m_error);
            const auto second = coreWindowInfo(m_kwin, m_secondaryId, &m_error);
            const auto third = coreWindowInfo(m_kwin, m_tertiaryId, &m_error);
            const auto fourth = coreWindowInfo(m_kwin, m_quaternaryId, &m_error);
            return first && second && third && fourth
                && m_primary.isExposed() && m_secondary.isExposed()
                && m_tertiary.isExposed() && m_quaternary.isExposed()
                && m_primary.size() == requestedSize && coreCaption(*first) == usableTitle
                && sameFrame(coreFrame(*second), m_secondaryRestoreFrame)
                && sameFrame(coreFrame(*third), m_tertiaryRestoreFrame)
                && sameFrame(coreFrame(*fourth), m_quaternaryRestoreFrame);
        });
        if (!m_result.clientsUsable) {
            fail(m_error.isEmpty() ? QStringLiteral("clients stopped responding after unload")
                                   : m_error);
        }
        return m_result.clientsUsable;
    }

    QWindow &m_primary;
    QWindow &m_secondary;
    QWindow &m_tertiary;
    QWindow &m_quaternary;
    QDBusInterface m_endpoint;
    QDBusInterface m_kwin;
    QDBusInterface m_plugins;
    PluginUnloadResult m_result;
    QString m_error;
    QString m_primaryId;
    QString m_secondaryId;
    QString m_tertiaryId;
    QString m_quaternaryId;
    QString m_containerId;
    QString m_secondContainerId;
    QRectF m_primaryRestoreFrame;
    QRectF m_secondaryRestoreFrame;
    QRectF m_tertiaryRestoreFrame;
    QRectF m_quaternaryRestoreFrame;
};

} // namespace

PluginUnloadResult exercisePluginUnload(QWindow &primary,
                                        QWindow &secondary,
                                        QWindow &tertiary,
                                        QWindow &quaternary)
{
    // Plugin loading can trail the first client configure when portal startup
    // is slow. Wait on D-Bus ownership instead of baking scheduler timing into
    // the nested lifecycle proof.
    const bool servicesReady = await([] {
        return serviceIsRegistered(QLatin1StringView(CompositorService))
            && serviceIsRegistered(QLatin1StringView(KWinService));
    }, 5000);
    if (!servicesReady) {
        PluginUnloadResult result;
        result.failure = QStringLiteral("required compositor/KWin D-Bus services did not start");
        return result;
    }
    return PluginUnloadWorkflow(primary, secondary, tertiary, quaternary).run();
}

} // namespace QindaQt::Test
