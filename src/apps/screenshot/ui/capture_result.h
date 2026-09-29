// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "desktop_actions.h"
#include "frame_store.h"

#include <QImage>
#include <QObject>
#include <QSize>
#include <QUrl>

#include <functional>

namespace QindaQt::Screenshot {

class ClipboardPublisher;
class ResultNotifier;

// Where a default Save goes: the resolved folder and the pattern.
struct SaveTarget {
    QString folder;
    QString pattern;
};
using SaveTargetSource = std::function<SaveTarget()>;

// The finished capture and everything a user can do with it.
//
// AGENT-CONTRACT: Save never overwrites (capture_file_policy); Save As
// writes exactly the path the dialog confirmed. A successful save posts one
// "Screenshot saved" notification whose buttons come back through
// handleOpen/handleCopyFile/handleShowInFolder, which work on the saved
// file, not on this object's current image, so a notification from an
// earlier capture still acts on its own file. Borrowed collaborators outlive
// this object; GUI thread only.
class CaptureResult final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY imageChanged)
    Q_PROPERTY(QString source READ source NOTIFY imageChanged)
    Q_PROPERTY(QSize size READ size NOTIFY imageChanged)
    Q_PROPERTY(QString savedPath READ savedPath NOTIFY savedPathChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)

public:
    CaptureResult(FrameStore &frames, ClipboardPublisher &clipboard, ResultNotifier &notifier,
                  DesktopActions actions, SaveTargetSource target, QObject *parent = nullptr);
    ~CaptureResult() override;

    void setImage(const QImage &image, const QString &modeId);
    [[nodiscard]] QImage image() const;
    [[nodiscard]] bool hasImage() const;
    [[nodiscard]] QString source() const;
    [[nodiscard]] QSize size() const;
    [[nodiscard]] QString savedPath() const { return m_savedPath; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] bool messageIsError() const { return m_messageIsError; }

    Q_INVOKABLE void copy();
    // Default folder and pattern; returns the path or empty on failure.
    Q_INVOKABLE QString save();
    Q_INVOKABLE bool saveAs(const QUrl &file);
    Q_INVOKABLE void open();
    Q_INVOKABLE void showInFolder();
    Q_INVOKABLE void clear();
    Q_INVOKABLE QUrl suggestedSaveUrl() const;
    Q_INVOKABLE QUrl saveFolderUrl() const;

    // Save to an explicit command-line path: a folder gets the default name,
    // an existing file is never replaced.
    QString saveToCommandLinePath(const QString &path);

    // Notification buttons.
    void handleOpen(const QString &path);
    void handleCopyFile(const QString &path);
    void handleShowInFolder(const QString &path);

Q_SIGNALS:
    void imageChanged();
    void savedPathChanged();
    void messageChanged();
    void saved(const QString &path);

private:
    void setMessage(const QString &message, bool error);
    void setSavedPath(const QString &path);
    QString finishSave(const QString &path, const QString &error);

    FrameStore &m_frames;
    ClipboardPublisher &m_clipboard;
    ResultNotifier &m_notifier;
    DesktopActions m_actions;
    SaveTargetSource m_target;
    QString m_modeId;
    QString m_savedPath;
    QString m_message;
    bool m_messageIsError = false;
};

} // namespace QindaQt::Screenshot
