// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QElapsedTimer>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>
#include <QTextStream>
#include <QThread>
#include <QWidget>

namespace {

QPalette paletteForScheme(Qt::ColorScheme scheme)
{
    // AGENT-NOTE: The offscreen platform updates QStyleHints live but retains
    // an explicitly applied palette. This probe models a toolkit consumer's
    // reaction instead of claiming Qt overwrites application palette policy.
    QPalette palette = QApplication::style()->standardPalette();
    if (scheme == Qt::ColorScheme::Dark) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#323232")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#f0f0f0")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#252525")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#f0f0f0")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#3c3c3c")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f0f0f0")));
    }
    return palette;
}

bool paletteMatches(Qt::ColorScheme scheme)
{
    const QPalette palette = QApplication::palette();
    const qreal window = palette.color(QPalette::Window).lightnessF();
    const qreal text = palette.color(QPalette::WindowText).lightnessF();
    return scheme == Qt::ColorScheme::Dark ? window < text : window > text;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("qindaqt-portal-toolkit-probe"));
    QWidget window;
    window.setWindowTitle(QStringLiteral("QindaQt portal toolkit probe"));
    window.resize(64, 64);
    window.show();
    QTextStream output(stdout);
    QStyleHints *styleHints = QApplication::styleHints();
    bool initialObserved = false;
    bool liveSignalObserved = false;
    QApplication::setPalette(paletteForScheme(styleHints->colorScheme()));
    output << "START scheme=" << static_cast<int>(styleHints->colorScheme())
           << " window=" << QApplication::palette().color(QPalette::Window).name()
           << " text=" << QApplication::palette().color(QPalette::WindowText).name()
           << '\n';
    output.flush();
    QObject::connect(styleHints, &QStyleHints::colorSchemeChanged, &application,
                     [&](Qt::ColorScheme scheme) {
                         liveSignalObserved = true;
                         QApplication::setPalette(paletteForScheme(scheme));
                         output << "SIGNAL scheme=" << static_cast<int>(scheme)
                                << " window="
                                << QApplication::palette().color(QPalette::Window).name()
                                << '\n';
                         output.flush();
                     });

    QElapsedTimer timeout;
    timeout.start();
    while (timeout.elapsed() < 10'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const Qt::ColorScheme scheme = styleHints->colorScheme();
        if (!initialObserved && scheme == Qt::ColorScheme::Dark
            && paletteMatches(scheme)) {
            initialObserved = true;
            output << "READY dark palette\n";
            output.flush();
        } else if (initialObserved && liveSignalObserved
                   && scheme == Qt::ColorScheme::Light
                   && paletteMatches(scheme)) {
            output << "CHANGED light palette\n";
            output.flush();
            return 0;
        }
        QThread::msleep(10);
    }
    const QPalette palette = QApplication::palette();
    QTextStream(stderr) << "toolkit probe timed out: initial=" << initialObserved
                        << " liveSignal=" << liveSignalObserved
                        << " scheme=" << static_cast<int>(styleHints->colorScheme())
                        << " window=" << palette.color(QPalette::Window).name()
                        << " text=" << palette.color(QPalette::WindowText).name() << '\n';
    return 2;
}
