// SPDX-License-Identifier: GPL-3.0-or-later
#include "pluginunloadworkflow.h"
#include "hybridpointerunloadworkflow.h"

#include <QBackingStore>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegion>
#include <QResizeEvent>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

namespace {

class PaintedWindow final : public QWindow
{
public:
    PaintedWindow(QString title, QSize initialSize, QColor color)
        : m_store(this)
        , m_color(std::move(color))
    {
        setTitle(std::move(title));
        resize(initialSize);
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

QJsonObject resultJson(const QindaQt::Test::PluginUnloadResult &result)
{
    return {{QStringLiteral("grouped"), result.grouped},
            {QStringLiteral("unloadCallSucceeded"), result.unloadCallSucceeded},
            {QStringLiteral("serviceRemoved"), result.serviceRemoved},
            {QStringLiteral("pluginRemoved"), result.pluginRemoved},
            {QStringLiteral("framesRestored"), result.framesRestored},
            {QStringLiteral("clientsUsable"), result.clientsUsable},
            {QStringLiteral("failure"), result.failure},
            {QStringLiteral("evidence"), result.evidence}};
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    PaintedWindow primary(QStringLiteral("QindaQt plugin unload primary"),
                          QSize(640, 480), QColor(QStringLiteral("#31506b")));
    PaintedWindow secondary(QStringLiteral("QindaQt plugin unload secondary"),
                            QSize(520, 360), QColor(QStringLiteral("#6b3150")));
    PaintedWindow tertiary(QStringLiteral("QindaQt plugin unload tertiary"),
                           QSize(460, 340), QColor(QStringLiteral("#506b31")));
    PaintedWindow quaternary(QStringLiteral("QindaQt plugin unload quaternary"),
                             QSize(410, 300), QColor(QStringLiteral("#6b5031")));
    primary.show();
    secondary.show();
    tertiary.show();
    quaternary.show();

    QTimer::singleShot(700, &application, [&] {
        const bool hybridPointerExpected =
            qEnvironmentVariableIntValue("QINDAQT_EXPECT_HYBRID_POINTER_UNLOAD") == 1;
        if (hybridPointerExpected) {
            // The virtual backend initially stacks clients. This mirrors the
            // complete pointer proof and makes the selected title hit stable.
            tertiary.raise();
            tertiary.requestActivate();
        }
        const auto result = hybridPointerExpected
            ? QindaQt::Test::exerciseHybridPointerPluginUnload(
                  primary, secondary, tertiary, quaternary,
                  qEnvironmentVariable("QINDAQT_DOTOOL"))
            : QindaQt::Test::exercisePluginUnload(
                  primary, secondary, tertiary, quaternary);
        const auto document = resultJson(result);
        QTextStream(stdout) << "QINDAQT_PLUGIN_UNLOAD="
                            << QJsonDocument(document).toJson(QJsonDocument::Compact) << '\n';
        const bool passed = result.grouped && result.unloadCallSucceeded
            && result.serviceRemoved && result.pluginRemoved && result.framesRestored
            && result.clientsUsable;
        application.exit(passed ? 0 : 1);
    });
    return application.exec();
}
