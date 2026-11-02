// SPDX-License-Identifier: GPL-3.0-or-later
#include "evidence_runner.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QWheelEvent>

#include <cstdio>

namespace qindaqt::link_mirror {
namespace {

constexpr int kDeadlineMs = 90'000;
constexpr int kInputGapMs = 400;
// After scripted input, wait at least this long as well as the frame count: a static screen
// only produces repeat frames every 100 ms, which can satisfy the count before the app redraws.
constexpr qint64 kSettleMs = 1500;

void print(const QString &line)
{
    std::fputs(qPrintable(line + QLatin1Char('\n')), stdout);
    std::fflush(stdout);
}

} // namespace

EvidenceRunner::EvidenceRunner(const MirrorOptions &options, MirrorController &controller, MirrorWindow &window,
                               ViewportMapper &mapper, QObject *parent)
    : QObject(parent), m_options(options), m_controller(controller), m_window(window), m_mapper(mapper)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] { done(1, QStringLiteral("evidence run timed out")); });
    m_deadline.start(options.probe ? (options.probeSeconds * 1000 + kDeadlineMs) : kDeadlineMs);
    m_probeTimer.setInterval(1000);
    connect(&m_probeTimer, &QTimer::timeout, this, &EvidenceRunner::probeTick);
    connect(&controller, &MirrorController::frameShown, this, &EvidenceRunner::onFrame);
    connect(&controller, &MirrorController::displayReady, this, &EvidenceRunner::onDisplay);
    connect(&controller, &MirrorController::appListed, this, [this](const AppEntry &app) {
        ++m_listed;
        print(QStringLiteral("%1\t%2\ticon=%3x%4")
                  .arg(app.packageName, app.label)
                  .arg(app.icon.width())
                  .arg(app.icon.height()));
    });
    connect(&controller, &MirrorController::inventoryDone, this, [this](int count) {
        if (m_options.list) {
            done(0, QStringLiteral("apps=%1").arg(count));
        }
    });
}

void EvidenceRunner::onDisplay(const DisplayInfo &display)
{
    print(QStringLiteral("display id=%1 size=%2x%3 dpi=%4 flags=0x%5")
              .arg(display.displayId)
              .arg(display.size.width())
              .arg(display.size.height())
              .arg(display.dpi)
              .arg(QString::number(display.flags, 16)));
    if (m_options.probe && !m_probeTimer.isActive()) {
        m_probeClock.start();
        m_probeStartFrames = m_controller.decodedFrames();
        m_probeTimer.start();
    }
}

void EvidenceRunner::probeTick()
{
    ++m_probeTicks;
    const quint64 frames = m_controller.decodedFrames() - m_probeStartFrames;
    const double seconds = static_cast<double>(m_probeClock.elapsed()) / 1000.0;
    const QSize size = m_controller.streamSize();
    const QString line = QStringLiteral("decoder=%1 resolution=%2x%3 frames=%4 seconds=%5 fps=%6")
                             .arg(decoderPathName(m_controller.decoderPath()))
                             .arg(size.width())
                             .arg(size.height())
                             .arg(frames)
                             .arg(seconds, 0, 'f', 1)
                             .arg(seconds > 0 ? static_cast<double>(frames) / seconds : 0.0, 0, 'f', 1);
    print(line);
    if (m_probeTicks >= m_options.probeSeconds) {
        done(frames > 0 ? 0 : 1, frames > 0 ? QString() : QStringLiteral("no frames decoded"));
    }
}

void EvidenceRunner::onFrame()
{
    if (m_options.grabPath.isEmpty()) {
        return;
    }
    ++m_framesSinceMark;
    if (m_stage == Stage::WaitFirstGrab && m_framesSinceMark >= m_options.grabAfterFrames) {
        writeGrab(m_options.grabPath);
        if (m_options.inputSteps.isEmpty()) {
            done(0);
            return;
        }
        m_stage = Stage::Inputs;
        runNextInput();
    } else if (m_stage == Stage::WaitSecondGrab && m_framesSinceMark >= m_options.grabAfterFrames
               && m_sinceMark.elapsed() >= kSettleMs) {
        writeGrab(m_options.grabAfterInputPath);
        done(0);
    }
}

void EvidenceRunner::runNextInput()
{
    if (m_nextInput >= m_options.inputSteps.size()) {
        m_stage = Stage::WaitSecondGrab;
        m_framesSinceMark = 0;
        m_sinceMark.start();
        return;
    }
    deliver(m_options.inputSteps.at(m_nextInput++));
    QTimer::singleShot(kInputGapMs, this, &EvidenceRunner::runNextInput);
}

void EvidenceRunner::deliver(const InputStep &step)
{
    const QPointF global = step.position;
    switch (step.kind) {
    case InputStepKind::Tap: {
        QMouseEvent press(QEvent::MouseButtonPress, step.position, global, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(&m_window, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, step.position, global, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QCoreApplication::sendEvent(&m_window, &release);
        print(QStringLiteral("input tap %1,%2").arg(step.position.x()).arg(step.position.y()));
        return;
    }
    case InputStepKind::Scroll: {
        QWheelEvent wheel(step.position, global, QPoint(), QPoint(0, step.scrollNotches * 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&m_window, &wheel);
        print(QStringLiteral("input scroll %1 notches").arg(step.scrollNotches));
        return;
    }
    case InputStepKind::Text: {
        for (const QChar character : step.text) {
            QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
            QCoreApplication::sendEvent(&m_window, &press);
        }
        print(QStringLiteral("input text of %1 characters").arg(step.text.size()));
        return;
    }
    case InputStepKind::Key: {
        QKeyEvent press(QEvent::KeyPress, step.qtKey, Qt::NoModifier);
        QCoreApplication::sendEvent(&m_window, &press);
        QKeyEvent release(QEvent::KeyRelease, step.qtKey, Qt::NoModifier);
        QCoreApplication::sendEvent(&m_window, &release);
        print(QStringLiteral("input key 0x%1").arg(QString::number(step.qtKey, 16)));
        return;
    }
    case InputStepKind::Resize: {
        m_mapper.setViewSize(step.size);
        m_controller.handleViewResized(step.size, 1.0);
        print(QStringLiteral("input resize %1x%2").arg(step.size.width()).arg(step.size.height()));
        return;
    }
    }
}

void EvidenceRunner::writeGrab(const QString &path)
{
    const QImage frame = m_window.frame();
    const QImage snapshot = m_window.renderSnapshot(m_mapper.viewSize().toSize());
    const bool saved = snapshot.save(path, "PNG");
    print(QStringLiteral("grab %1 decoder=%2 frame=%3x%4 view=%5x%6 %7")
              .arg(path, decoderPathName(m_controller.decoderPath()))
              .arg(frame.width())
              .arg(frame.height())
              .arg(snapshot.width())
              .arg(snapshot.height())
              .arg(saved ? QStringLiteral("saved") : QStringLiteral("FAILED")));
    m_framesSinceMark = 0;
    if (!saved) {
        done(1, QStringLiteral("could not write %1").arg(path));
    }
}

void EvidenceRunner::done(int exitCode, const QString &message)
{
    if (m_stage == Stage::Done) {
        return;
    }
    m_stage = Stage::Done;
    m_probeTimer.stop();
    m_deadline.stop();
    if (!message.isEmpty()) {
        print(message);
    }
    m_controller.finish(exitCode);
}

} // namespace qindaqt::link_mirror
