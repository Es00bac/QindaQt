// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellwindowactionsliveclients.h"

#include <QBackingStore>
#include <QColor>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

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
                                    const QString &title)
{
    PaintedWindow window;
    window.setTitle(title);
    window.resize(360, 240);
    window.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
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
    return application.exec();
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

} // namespace QindaQt::Compositor::TestSupport
