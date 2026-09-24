// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

#include "public/desktop_file_boundary.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopSurface {

namespace {

// The platform clipboard only exists under a QGuiApplication; GUI-less
// controller tests run under QCoreApplication and leave this null, which the
// clipboard policy treats as fail-closed.
QClipboard *applicationClipboard()
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) == nullptr) {
        return nullptr;
    }
    return QGuiApplication::clipboard();
}

} // namespace

DesktopContentsController::DesktopContentsController(QObject *parent)
    : DesktopContentsController(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                parent)
{
}

DesktopContentsController::DesktopContentsController(QString root, QObject *parent)
    : DesktopContentsController(std::move(root), std::nullopt, applicationClipboard(), parent)
{
}

DesktopContentsController::DesktopContentsController(QString root, QStringList fileManagerPrograms,
                                                     QObject *parent)
    : DesktopContentsController(std::move(root), std::move(fileManagerPrograms), nullptr, parent)
{
}

DesktopContentsController::DesktopContentsController(QString root,
                                                     std::optional<QStringList> fileManagerPrograms,
                                                     QClipboard *clipboard, QObject *parent)
    : QObject(parent), m_root(std::move(root)),
      m_fileManagerPrograms(std::move(fileManagerPrograms)),
      m_clipboard(clipboard)
{
    initializeMutation();
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(150);
    connect(&m_refreshTimer, &QTimer::timeout, this, &DesktopContentsController::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_refreshTimer,
            qOverload<>(&QTimer::start));
    // AGENT-GUARD: watch only an existing directory. addPath() warns about a
    // missing one, and the surface's offscreen rows treat warnings as fatal.
    if (!m_root.isEmpty() && QFileInfo(m_root).isDir()) {
        m_watcher.addPath(m_root);
    }
    refresh();
}

DesktopContentsController::~DesktopContentsController() = default;

void DesktopContentsController::initializeMutation()
{
    using QindaQt::Apps::FileManager::ClipboardController;
    using QindaQt::Apps::FileManager::MutationController;
    using QindaQt::Apps::FileManager::Desktop::FileBoundary;
    m_mutation = FileBoundary::createLocalMutationController(this);
    connect(m_mutation.get(), &MutationController::mutationCommitted, this,
            &DesktopContentsController::refresh);
    connect(m_mutation.get(), &MutationController::stateChanged, this, [this]() {
        if (!m_mutation->failureMessage().isEmpty()) {
            publishFeedback(m_mutation->failureMessage());
        }
    });
    if (m_clipboard != nullptr) {
        m_clipboardController =
            FileBoundary::createLocalClipboardController(*m_mutation, *m_clipboard, this);
        connect(m_clipboardController.get(), &ClipboardController::stateChanged, this,
                [this]() { emit clipboardChanged(); });
    }
}

void DesktopContentsController::refresh()
{
    using QindaQt::Apps::FileManager::Desktop::FileBoundary;
    const auto listing = FileBoundary::listLocalFolder(m_root);

    QVariantList rows;
    QHash<QString, ListedEntry> listed;
    if (listing.ok()) {
        rows.reserve(listing.entries.size());
        for (const auto &entry : listing.entries) {
            // Dot-entries stay hidden on the desktop presentation, matching
            // every other stock desktop; the boundary itself is not asked to
            // filter, so this is purely a presentation choice.
            if (entry.isHidden) {
                continue;
            }
            listed.insert(entry.absolutePath,
                          {entry.isDirectory, entry.device, entry.inode, entry.identitySize,
                           entry.modifiedNanoseconds, entry.mode});
            rows.append(QVariantMap{
                {QStringLiteral("id"), entry.absolutePath},
                {QStringLiteral("label"), entry.name},
                {QStringLiteral("path"), entry.absolutePath},
                {QStringLiteral("iconName"),
                 entry.isDirectory ? QStringLiteral("folder") : QStringLiteral("text-x-generic")},
                {QStringLiteral("accessibleName"),
                 QStringLiteral("%1, %2").arg(entry.name, entry.absolutePath)},
                {QStringLiteral("isDirectory"), entry.isDirectory},
                // Listing-time identity, exactly the shape the mutation
                // batch and clipboard contracts consume (decimal strings),
                // so the QML passes selected rows through unchanged.
                {QStringLiteral("device"), QString::number(entry.device)},
                {QStringLiteral("inode"), QString::number(entry.inode)},
                {QStringLiteral("identitySize"), QString::number(entry.identitySize)},
                {QStringLiteral("modifiedNanoseconds"),
                 QString::number(entry.modifiedNanoseconds)},
                {QStringLiteral("mode"), QString::number(entry.mode)},
                // Stable across a rename and unique within the mounted
                // filesystem. Presentation uses this only as a layout key;
                // mutation still receives the complete identity below.
                {QStringLiteral("layoutKey"),
                 QStringLiteral("%1:%2").arg(entry.device).arg(entry.inode)},
            });
        }
    }
    // An unchanged listing (the watcher also fires after this controller's
    // own mutations) keeps the rows, so the icons are not rebuilt for nothing.
    if (rows != m_rows) {
        m_listed = std::move(listed);
        m_rows = std::move(rows);
        Q_EMIT rowsChanged();
    }
    publishFeedback(listing.ok() ? QString() : listing.diagnostic);
}

bool DesktopContentsController::open(const QString &absolutePath)
{
    using QindaQt::Apps::FileManager::Desktop::FileBoundary;
    using QindaQt::Apps::FileManager::Desktop::ListedIdentity;
    const auto listed = m_listed.constFind(absolutePath);
    if (listed == m_listed.cend()) {
        // AGENT-GUARD: activation opens only what the Desktop listed; an
        // unknown path is never launched or reinterpreted as another item.
        publishFeedback(QStringLiteral("%1 is not on the Desktop").arg(absolutePath));
        return false;
    }
    QString diagnostic;
    if (listed->isDirectory) {
        const ListedIdentity identity{listed->device, listed->inode};
        const auto result =
            m_fileManagerPrograms
                ? FileBoundary::openLocalFolder(absolutePath, identity, *m_fileManagerPrograms)
                : FileBoundary::openLocalFolder(absolutePath, identity);
        diagnostic = result.diagnostic;
        if (!result.ok() && diagnostic.isEmpty()) {
            diagnostic = QStringLiteral("%1 could not be opened").arg(absolutePath);
        }
    } else {
        const auto result = FileBoundary::launchLocalFile(absolutePath);
        diagnostic = result.ok() ? QString() : result.diagnostic;
    }
    if (!diagnostic.isEmpty()) {
        publishFeedback(diagnostic);
        return false;
    }
    clearFeedback();
    return true;
}

bool DesktopContentsController::runFileManagerAction(const QString &actionId,
                                                     const QString &absolutePath)
{
    using QindaQt::Apps::FileManager::Desktop::FileBoundary;
    using QindaQt::Apps::FileManager::Desktop::FolderOpenResult;
    using QindaQt::Apps::FileManager::Desktop::ListedIdentity;
    FolderOpenResult result;
    if (absolutePath.isEmpty()) {
        result = m_fileManagerPrograms
                     ? FileBoundary::runLocalFolderAction(m_root, actionId, *m_fileManagerPrograms)
                     : FileBoundary::runLocalFolderAction(m_root, actionId);
    } else {
        const auto listed = m_listed.constFind(absolutePath);
        if (listed == m_listed.cend()) {
            publishFeedback(QStringLiteral("%1 is not on the Desktop").arg(absolutePath));
            return false;
        }
        const ListedIdentity identity{listed->device, listed->inode};
        result = m_fileManagerPrograms
                     ? FileBoundary::revealLocalItem(absolutePath, identity, actionId,
                                                     *m_fileManagerPrograms)
                     : FileBoundary::revealLocalItem(absolutePath, identity, actionId);
    }
    if (!result.ok()) {
        publishFeedback(result.diagnostic.isEmpty()
                            ? QStringLiteral("File Manager could not run %1").arg(actionId)
                            : result.diagnostic);
        return false;
    }
    clearFeedback();
    return true;
}

bool DesktopContentsController::rename(const QString &absolutePath, const QString &newName)
{
    const auto listed = m_listed.constFind(absolutePath);
    if (listed == m_listed.cend()) {
        publishFeedback(QStringLiteral("%1 is not on the Desktop").arg(absolutePath));
        return false;
    }
    const QVariantMap identity{
        {QStringLiteral("device"), QString::number(listed->device)},
        {QStringLiteral("inode"), QString::number(listed->inode)},
        {QStringLiteral("identitySize"), QString::number(listed->identitySize)},
        {QStringLiteral("modifiedNanoseconds"), QString::number(listed->modifiedNanoseconds)},
        {QStringLiteral("mode"), QString::number(listed->mode)},
    };
    if (!m_mutation->renameItem(absolutePath, newName, identity)) {
        if (!m_mutation->failureMessage().isEmpty()) {
            publishFeedback(m_mutation->failureMessage());
        }
        return false;
    }
    clearFeedback();
    return true;
}

bool DesktopContentsController::trashEntries(const QVariantList &items)
{
    QVariantList batch;
    QString diagnostic;
    if (!collectBatchItems(items, &batch, &diagnostic)) {
        publishFeedback(diagnostic);
        return false;
    }
    if (!m_mutation->trashItems(batch)) {
        if (!m_mutation->failureMessage().isEmpty()) {
            publishFeedback(m_mutation->failureMessage());
        }
        return false;
    }
    clearFeedback();
    return true;
}

bool DesktopContentsController::copySelection(const QVariantList &items)
{
    return dispatchClipboardSelection(
        items, [this](const QVariantList &batch) { return m_clipboardController->copySelection(batch); });
}

bool DesktopContentsController::cutSelection(const QVariantList &items)
{
    return dispatchClipboardSelection(
        items, [this](const QVariantList &batch) { return m_clipboardController->cutSelection(batch); });
}

bool DesktopContentsController::pasteIntoDesktop()
{
    if (!m_clipboardController) {
        publishFeedback(QStringLiteral("The clipboard is not available"));
        return false;
    }
    if (!m_clipboardController->pasteInto(m_root)) {
        if (!m_clipboardController->lastRejection().isEmpty()) {
            publishFeedback(m_clipboardController->lastRejection());
        }
        return false;
    }
    clearFeedback();
    return true;
}

bool DesktopContentsController::canPaste() const
{
    return m_clipboardController != nullptr && m_clipboardController->canPaste();
}

QString DesktopContentsController::clipboardMode() const
{
    return m_clipboardController != nullptr ? m_clipboardController->mode()
                                            : QStringLiteral("none");
}

bool DesktopContentsController::collectBatchItems(const QVariantList &items,
                                                  QVariantList *batch,
                                                  QString *diagnostic) const
{
    for (const QVariant &item : items) {
        const QVariantMap map = item.toMap();
        const QString path = map.value(QStringLiteral("path")).toString();
        const auto listed = m_listed.constFind(path);
        if (path.isEmpty() || listed == m_listed.cend()) {
            // AGENT-GUARD: only entries the last listing reported may be
            // mutated; an unknown path is refused before any batch item is
            // built, so nothing else gets trashed or clipboard-adopted.
            *diagnostic = path.isEmpty()
                ? QStringLiteral("A selected entry is no longer on the Desktop")
                : QStringLiteral("%1 is not on the Desktop").arg(path);
            return false;
        }
        batch->append(QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("device"), QString::number(listed->device)},
            {QStringLiteral("inode"), QString::number(listed->inode)},
            {QStringLiteral("identitySize"), QString::number(listed->identitySize)},
            {QStringLiteral("modifiedNanoseconds"),
             QString::number(listed->modifiedNanoseconds)},
            {QStringLiteral("mode"), QString::number(listed->mode)},
        });
    }
    return true;
}

bool DesktopContentsController::dispatchClipboardSelection(
    const QVariantList &items, const std::function<bool(const QVariantList &)> &dispatch)
{
    if (!m_clipboardController) {
        publishFeedback(QStringLiteral("The clipboard is not available"));
        return false;
    }
    QVariantList batch;
    QString diagnostic;
    if (!collectBatchItems(items, &batch, &diagnostic)) {
        publishFeedback(diagnostic);
        return false;
    }
    if (!dispatch(batch)) {
        if (!m_clipboardController->lastRejection().isEmpty()) {
            publishFeedback(m_clipboardController->lastRejection());
        }
        return false;
    }
    clearFeedback();
    return true;
}

void DesktopContentsController::clearFeedback()
{
    publishFeedback({});
}

void DesktopContentsController::publishFeedback(const QString &message)
{
    if (m_feedback == message) {
        return;
    }
    m_feedback = message;
    Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopSurface
