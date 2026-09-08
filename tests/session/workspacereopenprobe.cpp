// SPDX-License-Identifier: GPL-3.0-or-later
//
// Two-mode executable for the saved-workspace two-session nested proof.
//
// Client mode (QINDAQT_WREOPEN_APP_ID/TITLES/SIZES set): a plain painted
// Wayland client whose windows carry a real desktop-entry application
// identity, so the production workspace port captures and matches them.
//
// Phase mode (QINDAQT_WREOPEN_PHASE=save|reopen): the compositor session
// client. It spawns the fixture clients, drives the production workspace UI
// through the scenario-gated development input device only, and prints one
// QINDAQT_WORKSPACE_REOPEN=<json> evidence line before exiting (which ends
// the nested session through KWin's --exit-with-session).

#include "compositorprobeclient.h"
#include "workspacereopenphases.h"
#include "workspacereopeninput.h"

#include <QBackingStore>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QPainter>
#include <QProcess>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

#include <memory>
#include <optional>
#include <vector>

namespace {

class PaintedClientWindow final : public QWindow
{
public:
    PaintedClientWindow(QString title, QSize size, QColor color)
        : m_store(this), m_color(std::move(color))
    {
        setTitle(std::move(title));
        resize(size);
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
        if (!isExposed() || size().isEmpty()) {
            return;
        }
        m_store.resize(size());
        const QRegion region(QRect(QPoint{}, size()));
        m_store.beginPaint(region);
        QPainter painter(m_store.paintDevice());
        painter.fillRect(region.boundingRect(), m_color);
        painter.end();
        m_store.endPaint();
        m_store.flush(region);
    }

    QBackingStore m_store;
    QColor m_color;
};

int runClientMode(QGuiApplication &application)
{
    const auto titles =
        QString::fromUtf8(qgetenv("QINDAQT_WREOPEN_TITLES")).split(QLatin1Char(';'));
    const auto sizes =
        QString::fromUtf8(qgetenv("QINDAQT_WREOPEN_SIZES")).split(QLatin1Char(';'));
    const auto activate =
        QString::fromUtf8(qgetenv("QINDAQT_WREOPEN_ACTIVATE")).split(QLatin1Char(';'));
    if (titles.isEmpty() || titles.size() != sizes.size()) {
        QTextStream(stderr) << "workspace client mode needs matching "
                               "QINDAQT_WREOPEN_TITLES and _SIZES\n";
        return 2;
    }
    std::vector<std::unique_ptr<PaintedClientWindow>> windows;
    static const QList<QColor> colors{
        QColor(QStringLiteral("#31506b")), QColor(QStringLiteral("#6b3150")),
        QColor(QStringLiteral("#506b31"))};
    for (qsizetype index = 0; index < titles.size(); ++index) {
        const auto parts = sizes.at(index).split(QLatin1Char('x'));
        auto window = std::make_unique<PaintedClientWindow>(
            titles.at(index),
            QSize(parts.value(0).toInt(), parts.value(1).toInt()),
            colors.at(int(index % colors.size())));
        window->show();
        windows.push_back(std::move(window));
    }
    // Self-activation must win over the other fixture clients, so it runs
    // after a delay rather than racing their initial mapping.
    QTimer::singleShot(600, &application, [&windows, &activate] {
        for (const auto &window : windows) {
            if (activate.contains(window->title())) {
                window->raise();
                window->requestActivate();
            }
        }
    });
    return application.exec();
}

int runPhase(QGuiApplication &application, const QString &phase)
{
    QindaQt::Test::CompositorProbeClient client;
    QTimer::singleShot(700, &application, [&application, &client, phase] {
        QString error;
        std::optional<QJsonObject> evidence;
        if (phase == QStringLiteral("save")) {
            evidence = QindaQt::Test::exerciseWorkspaceSavePhase(client, &error);
        } else if (phase == QStringLiteral("reopen")) {
            evidence = QindaQt::Test::exerciseWorkspaceReopenPhase(client, &error);
        } else {
            error = QStringLiteral("unknown workspace reopen phase '%1'").arg(phase);
        }
        if (!evidence) {
            QTextStream(stderr)
                << "workspace reopen " << phase << " phase failed: " << error << '\n';
            application.exit(1);
            return;
        }
        QTextStream(stdout) << "QINDAQT_WORKSPACE_REOPEN="
                            << QJsonDocument(*evidence).toJson(QJsonDocument::Compact)
                            << '\n';
        application.exit(0);
    });
    return application.exec();
}

} // namespace

int main(int argc, char *argv[])
{
    const auto appId = QString::fromUtf8(qgetenv("QINDAQT_WREOPEN_APP_ID"));
    if (!appId.isEmpty()) {
        QGuiApplication::setDesktopFileName(appId);
    }
    QGuiApplication application(argc, argv);
    if (!appId.isEmpty()) {
        return runClientMode(application);
    }
    const auto phase = QString::fromUtf8(qgetenv("QINDAQT_WREOPEN_PHASE"));
    if (phase.isEmpty()) {
        QTextStream(stderr) << "set QINDAQT_WREOPEN_PHASE=save|reopen or the "
                               "client-mode variables\n";
        return 2;
    }
    return runPhase(application, phase);
}
