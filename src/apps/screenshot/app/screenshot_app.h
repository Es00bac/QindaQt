// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "command_line.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

class QQmlApplicationEngine;
class QQuickWindow;

namespace QindaQt::Screenshot {

class RecordController;

// The process composition root: owns every production collaborator, runs
// the one flow the invocation asked for, and decides when the process may
// exit.
//
// AGENT-CONTRACT (ADR-0289): a windowless launch stays resident only while
// it still owes the user something — a notification whose buttons must
// work, or a clipboard selection it must serve — and never longer than the
// notifier's bound. The window, once the user closes it, releases its hold
// the same way. Everything here is GUI-thread confined.
class ScreenshotApp final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY resultMessageChanged)

public:
    explicit ScreenshotApp(Invocation invocation, QObject *parent = nullptr);
    ~ScreenshotApp() override;

    // Starts the requested flow. Returns false when the QML window could not
    // be loaded; the caller exits with exitCode().
    bool start();
    [[nodiscard]] int exitCode() const { return m_exitCode; }
    [[nodiscard]] QString resultMessage() const { return m_resultMessage; }

    Q_INVOKABLE void openStreamingSettings();
    Q_INVOKABLE void openCaptureSettings();
    Q_INVOKABLE void openRecording(const QString &path);
    Q_INVOKABLE void showRecordingInFolder(const QString &path);
    Q_INVOKABLE void copyRecordingPath(const QString &path);

Q_SIGNALS:
    void resultMessageChanged();

private:
    class Private;

    bool loadWindow();
    void showWindow();
    void startObs();
    void seedOptionsFromPreferences();
    void onCaptured(const QImage &image, const QString &modeId);
    void onCaptureEnded(bool cancelled, const QString &message);
    void onRecordingSaved(const QString &path);
    void runRecordToggle();
    void finishWork(int exitCode);
    void setResultMessage(const QString &message);
    void maybeQuit();

    Invocation m_invocation;
    std::unique_ptr<Private> d;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QPointer<QQuickWindow> m_window;
    QString m_resultMessage;
    int m_exitCode = 0;
    bool m_workPending = false;
    bool m_windowOpen = false;
    bool m_optionsSeeded = false;
    bool m_obsStarted = false;
};

} // namespace QindaQt::Screenshot
