// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mirror_controller.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

// Headless modes of qindaqt-link-mirror: --grab (with optional scripted --input and a second
// grab), --probe and --list. They never show the window; main.cpp selects the offscreen QPA
// platform for them, so evidence runs never map anything on the user's desktop.
//
// AGENT-NOTE: scripted input is delivered as synthetic Qt events to the MirrorWindow, so it
// exercises the same InputTranslator and ViewportMapper path as a real pointer.
//
// Privacy: --list prints package names and labels of the phone's launchable apps; evidence
// reports should quote counts only.
namespace qindaqt::link_mirror {

class EvidenceRunner : public QObject {
    Q_OBJECT

public:
    EvidenceRunner(const MirrorOptions &options, MirrorController &controller, MirrorWindow &window,
                   ViewportMapper &mapper, QObject *parent = nullptr);

private:
    enum class Stage { WaitFirstGrab, Inputs, WaitSecondGrab, Done };

    void onFrame();
    void onDisplay(const DisplayInfo &display);
    void runNextInput();
    void deliver(const InputStep &step);
    void writeGrab(const QString &path);
    void probeTick();
    void checkPausedGrab();
    void done(int exitCode, const QString &message = {});

    const MirrorOptions &m_options;
    MirrorController &m_controller;
    MirrorWindow &m_window;
    ViewportMapper &m_mapper;
    Stage m_stage = Stage::WaitFirstGrab;
    int m_framesSinceMark = 0;
    qsizetype m_nextInput = 0;
    QElapsedTimer m_sinceMark;
    QTimer m_deadline;
    QTimer m_probeTimer;
    QTimer m_pausedGrabCheck;
    QElapsedTimer m_probeClock;
    quint64 m_probeStartFrames = 0;
    int m_probeTicks = 0;
    int m_listed = 0;
};

} // namespace qindaqt::link_mirror
