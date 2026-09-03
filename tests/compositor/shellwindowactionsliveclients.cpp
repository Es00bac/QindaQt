// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellwindowactionsliveclients.h"

#include <KWayland/Client/appmenu.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>

#include <QBackingStore>
#include <QColor>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>

#include <cstdio>

namespace QindaQt::Compositor::TestSupport {
namespace {

constexpr auto ServiceName = "org.qindaqt.Compositor";
constexpr auto ShellPath = "/org/qindaqt/CompositorShell";
constexpr auto ShellInterface = "org.qindaqt.CompositorShell1";
constexpr int TimeoutMilliseconds = 12'000;

class PaintedWindow final : public QWindow
{
public:
    PaintedWindow()
        : m_store(this)
    {
    }

protected:
    void exposeEvent(QExposeEvent *event) override
    {
        QWindow::exposeEvent(event);
        paint();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWindow::resizeEvent(event);
        paint();
    }

private:
    void paint()
    {
        if (!isExposed() || size().isEmpty()) return;
        m_store.resize(size());
        const QRegion region(QRect(QPoint{}, size()));
        m_store.beginPaint(region);
        QPainter painter(m_store.paintDevice());
        painter.fillRect(region.boundingRect(), QColor(QStringLiteral("#31506b")));
        painter.end();
        m_store.endPaint();
        m_store.flush(region);
    }

    QBackingStore m_store;
};

} // namespace

int runShellWindowActionsLiveWindow(QGuiApplication &application,
                                    const QString &title,
                                    bool announceAppMenu)
{
    PaintedWindow window;
    window.setTitle(title);
    window.resize(360, 240);
    window.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 25);

    KWayland::Client::Registry registry;
    KWayland::Client::AppMenuManager *appMenuManager = nullptr;
    KWayland::Client::AppMenu *appMenu = nullptr;
    if (announceAppMenu) {
        auto *native = application.nativeInterface<
            QNativeInterface::QWaylandApplication>();
        if (!native || !native->display()) {
            return 4;
        }
        QObject::connect(
            &registry, &KWayland::Client::Registry::appMenuAnnounced,
            &application, [&](quint32 name, quint32 version) {
                if (appMenu) return;
                appMenuManager = registry.createAppMenuManager(
                    name, version, &application);
                auto *surface = KWayland::Client::Surface::fromWindow(&window);
                if (!appMenuManager || !appMenuManager->isValid() || !surface) {
                    return;
                }
                appMenu = appMenuManager->create(surface, &window);
                if (appMenu && appMenu->isValid()) {
                    appMenu->setAddress(
                        QString::fromLatin1(ShellWindowActionsLiveAppMenuService),
                        QString::fromLatin1(ShellWindowActionsLiveAppMenuPath));
                }
            });
        registry.create(native->display());
        registry.setup();
        QElapsedTimer wait;
        wait.start();
        while ((!appMenu || !appMenu->isValid()) && wait.elapsed() < 2'000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        if (!appMenu || !appMenu->isValid()) {
            return 5;
        }
    }

    QTextStream input(stdin);
    QSocketNotifier inputNotifier(fileno(stdin), QSocketNotifier::Read,
                                  &application);
    QObject::connect(&inputNotifier, &QSocketNotifier::activated,
                     &application, [&] {
        const QString command = input.readLine();
        if (!appMenu) return;
        if (command == QLatin1StringView("valid")) {
            appMenu->setAddress(
                QString::fromLatin1(ShellWindowActionsLiveAppMenuService),
                QString::fromLatin1(ShellWindowActionsLiveAppMenuPath));
        } else if (command == QLatin1StringView("overlong-service")) {
            appMenu->setAddress(QStringLiteral("org.") + QString(252, u'a'),
                                QString::fromLatin1(
                                    ShellWindowActionsLiveAppMenuPath));
        } else if (command == QLatin1StringView("malformed-service")) {
            appMenu->setAddress(QStringLiteral("invalid"),
                                QString::fromLatin1(
                                    ShellWindowActionsLiveAppMenuPath));
        } else if (command == QLatin1StringView("malformed-path")) {
            appMenu->setAddress(
                QString::fromLatin1(ShellWindowActionsLiveAppMenuService),
                QStringLiteral("/org/qindaqt/bad-menu"));
        }
    });
    QTextStream(stdout)
        << ShellWindowActionsLiveClientMarker
        << QJsonDocument(QJsonObject{
               {QStringLiteral("title"), title},
               {QStringLiteral("nativeWindowId"),
                QString::number(static_cast<qulonglong>(window.winId()))}})
               .toJson(QJsonDocument::Compact)
        << Qt::endl;
    QTimer::singleShot(TimeoutMilliseconds * 2, &application,
                       &QCoreApplication::quit);
    const int result = application.exec();
    if (appMenu) appMenu->release();
    if (appMenuManager) appMenuManager->release();
    if (registry.isValid()) registry.release();
    return result;
}

int runUnauthorizedIdentityClient()
{
    QDBusInterface interface(QString::fromLatin1(ServiceName),
                             QString::fromLatin1(ShellPath),
                             QString::fromLatin1(ShellInterface),
                             QDBusConnection::sessionBus());
    const QDBusReply<QByteArray> reply = interface.call(
        QStringLiteral("ActiveWindowIdentity"));
    if (!reply.isValid()) {
        return 3;
    }
    QTextStream(stdout) << reply.value() << Qt::endl;
    return 0;
}

std::optional<quint64> parseLiveClientNativeWindowId(
    const QByteArray &output, const QString &expectedTitle)
{
    const auto marker = output.indexOf(ShellWindowActionsLiveClientMarker);
    if (marker < 0) {
        return std::nullopt;
    }
    const auto contentStart = marker
        + static_cast<qsizetype>(qstrlen(ShellWindowActionsLiveClientMarker));
    const auto end = output.indexOf('\n', contentStart);
    const QByteArray json = output.mid(contentStart,
                                       end < 0 ? -1 : end - contentStart);
    QJsonParseError parseError;
    const QJsonDocument metadata = QJsonDocument::fromJson(json, &parseError);
    bool idOk = false;
    const quint64 nativeId = metadata.object()
        .value(QStringLiteral("nativeWindowId")).toString().toULongLong(&idOk);
    if (parseError.error != QJsonParseError::NoError || !metadata.isObject()
        || metadata.object().value(QStringLiteral("title")).toString()
            != expectedTitle
        || !idOk || nativeId == 0) {
        return std::nullopt;
    }
    return nativeId;
}

bool isFailClosedUnauthorizedIdentityReply(const QByteArray &output)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(output, &error);
    const QJsonObject object = document.object();
    return error.error == QJsonParseError::NoError && document.isObject()
        && output.size() < 512
        && object.value(QStringLiteral("status")).toString()
            == QStringLiteral("unauthorized")
        && !object.contains(QStringLiteral("epoch"))
        && !object.contains(QStringLiteral("revision"))
        && !object.contains(QStringLiteral("actionRevision"))
        && !object.contains(QStringLiteral("activeWindow"));
}

bool proveUnauthorizedShellCalls(const QString &executable,
                                 const QString &windowId,
                                 const QString &epoch,
                                 quint64 revision,
                                 QString *failure)
{
    QProcess attacker;
    attacker.setProgram(executable);
    attacker.setArguments({QStringLiteral("--unauthorized"), windowId, epoch,
                           QString::number(revision)});
    attacker.start();
    if (!attacker.waitForFinished(3000) || attacker.exitCode() != 0) {
        if (failure) {
            *failure = QStringLiteral("unbound caller failed: %1")
                           .arg(QString::fromUtf8(attacker.readAllStandardError()));
        }
        return false;
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(
        attacker.readAllStandardOutput().trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || document.object().value(QStringLiteral("status")).toString()
            != QStringLiteral("unauthorized")) {
        if (failure) *failure = QStringLiteral("unbound action caller was admitted");
        return false;
    }

    attacker.start(executable, {QStringLiteral("--unauthorized-hostile")});
    if (!attacker.waitForFinished(5000) || attacker.exitCode() != 0) {
        if (failure) *failure = QStringLiteral("hostile-size caller failed");
        return false;
    }
    const QByteArray hostileOutput = attacker.readAllStandardOutput().trimmed();
    document = QJsonDocument::fromJson(hostileOutput, &parseError);
    const QJsonObject hostile = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || hostileOutput.size() >= 512
        || hostile.value(QStringLiteral("status")).toString()
            != QStringLiteral("unauthorized")
        || hostile.value(QStringLiteral("failure")).toObject()
               .value(QStringLiteral("code")).toString()
            != QStringLiteral("caller-pid-mismatch")
        || hostile.contains(QStringLiteral("windowId"))
        || hostile.contains(QStringLiteral("epoch"))
        || hostile.contains(QStringLiteral("revision"))) {
        if (failure) {
            *failure = QStringLiteral("hostile-size reply was not bounded and echo-free");
        }
        return false;
    }

    attacker.start(executable, {QStringLiteral("--unauthorized-identity")});
    if (!attacker.waitForFinished(3000) || attacker.exitCode() != 0
        || !isFailClosedUnauthorizedIdentityReply(
            attacker.readAllStandardOutput().trimmed())) {
        if (failure) *failure = QStringLiteral("unbound identity caller leaked facts");
        return false;
    }
    return true;
}

} // namespace QindaQt::Compositor::TestSupport
