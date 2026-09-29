// SPDX-License-Identifier: GPL-3.0-or-later
#include "capture_result.h"

#include "capture_file_policy.h"
#include "clipboard_publisher.h"
#include "result_notifier.h"

#include <qindaqt/services/screenshot_preferences/file_name_pattern.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

namespace QindaQt::Screenshot {
namespace {

using Services::ScreenshotPreferences::expandFileNamePattern;
using Services::ScreenshotPreferences::resolveScreenshotFolder;

} // namespace

CaptureResult::CaptureResult(FrameStore &frames, ClipboardPublisher &clipboard,
                             ResultNotifier &notifier, DesktopActions actions,
                             SaveTargetSource target, QObject *parent)
    : QObject(parent)
    , m_frames(frames)
    , m_clipboard(clipboard)
    , m_notifier(notifier)
    , m_actions(std::move(actions))
    , m_target(std::move(target))
{
    connect(&m_clipboard, &ClipboardPublisher::copied, this,
            [this] { setMessage(tr("Copied to the clipboard."), false); });
    connect(&m_clipboard, &ClipboardPublisher::failed, this,
            [this](const QString &message) { setMessage(message, true); });
}

CaptureResult::~CaptureResult() = default;

void CaptureResult::setImage(const QImage &image, const QString &modeId)
{
    m_frames.setResult(image);
    m_modeId = modeId;
    setSavedPath({});
    setMessage({}, false);
    Q_EMIT imageChanged();
}

QImage CaptureResult::image() const
{
    return m_frames.result();
}

bool CaptureResult::hasImage() const
{
    return !m_frames.result().isNull();
}

QString CaptureResult::source() const
{
    return hasImage() ? QStringLiteral("image://capture/result/%1").arg(m_frames.resultRevision())
                      : QString();
}

QSize CaptureResult::size() const
{
    return m_frames.result().size();
}

void CaptureResult::copy()
{
    m_clipboard.copyImage(image());
}

QString CaptureResult::save()
{
    const SaveTarget target = m_target ? m_target() : SaveTarget{};
    const QString folder = resolveScreenshotFolder(target.folder);
    const QString name = expandFileNamePattern(target.pattern, QDateTime::currentDateTime(), m_modeId);
    const SaveResult result = saveWithoutOverwriting(image(), folder, name);
    return finishSave(result.path, result.error);
}

bool CaptureResult::saveAs(const QUrl &file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : QString();
    if (path.isEmpty()) {
        setMessage(tr("Choose a file on this computer."), true);
        return false;
    }
    const SaveResult result = saveToConfirmedPath(image(), path);
    return !finishSave(result.path, result.error).isEmpty();
}

QString CaptureResult::saveToCommandLinePath(const QString &path)
{
    if (path.isEmpty())
        return save();
    const QFileInfo info(path);
    const SaveTarget target = m_target ? m_target() : SaveTarget{};
    if (info.isDir()) {
        const QString name =
            expandFileNamePattern(target.pattern, QDateTime::currentDateTime(), m_modeId);
        const SaveResult result = saveWithoutOverwriting(image(), info.absoluteFilePath(), name);
        return finishSave(result.path, result.error);
    }
    if (info.exists()) {
        // AGENT-GUARD: a script's explicit path is still never overwritten;
        // the caller learns why instead of losing an older capture.
        return finishSave({}, tr("%1 already exists.").arg(info.absoluteFilePath()));
    }
    const SaveResult result = saveWithoutOverwriting(image(), info.absolutePath(), info.fileName());
    return finishSave(result.path, result.error);
}

QString CaptureResult::finishSave(const QString &path, const QString &error)
{
    if (path.isEmpty() || !error.isEmpty()) {
        setMessage(tr("The screenshot was not saved: %1").arg(error), true);
        return {};
    }
    setSavedPath(path);
    setMessage(tr("Saved as %1").arg(QFileInfo(path).fileName()), false);
    m_notifier.notify(ResultKind::ScreenshotSaved, path);
    Q_EMIT saved(path);
    return path;
}

void CaptureResult::open()
{
    const QString path = m_savedPath.isEmpty() ? save() : m_savedPath;
    if (!path.isEmpty())
        handleOpen(path);
}

void CaptureResult::showInFolder()
{
    const QString path = m_savedPath.isEmpty() ? save() : m_savedPath;
    if (!path.isEmpty())
        handleShowInFolder(path);
}

void CaptureResult::clear()
{
    m_frames.setResult({});
    setSavedPath({});
    setMessage({}, false);
    Q_EMIT imageChanged();
}

QUrl CaptureResult::suggestedSaveUrl() const
{
    const SaveTarget target = m_target ? m_target() : SaveTarget{};
    return QUrl::fromLocalFile(QDir(resolveScreenshotFolder(target.folder))
                                   .filePath(expandFileNamePattern(
                                       target.pattern, QDateTime::currentDateTime(), m_modeId)));
}

QUrl CaptureResult::saveFolderUrl() const
{
    const SaveTarget target = m_target ? m_target() : SaveTarget{};
    return QUrl::fromLocalFile(resolveScreenshotFolder(target.folder));
}

void CaptureResult::handleOpen(const QString &path)
{
    QString error;
    if (!m_actions.openFile(path, &error))
        setMessage(error, true);
}

void CaptureResult::handleCopyFile(const QString &path)
{
    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull()) {
        setMessage(tr("The saved screenshot could not be read."), true);
        return;
    }
    m_clipboard.copyImage(image);
}

void CaptureResult::handleShowInFolder(const QString &path)
{
    QString error;
    if (!m_actions.showInFolder(path, &error))
        setMessage(error, true);
}

void CaptureResult::setMessage(const QString &message, bool error)
{
    if (m_message == message && m_messageIsError == error)
        return;
    m_message = message;
    m_messageIsError = error;
    Q_EMIT messageChanged();
}

void CaptureResult::setSavedPath(const QString &path)
{
    if (m_savedPath == path)
        return;
    m_savedPath = path;
    Q_EMIT savedPathChanged();
}

} // namespace QindaQt::Screenshot
