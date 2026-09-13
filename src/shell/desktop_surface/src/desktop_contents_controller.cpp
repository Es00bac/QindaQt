// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

#include "public/desktop_file_boundary.h"

#include <QStandardPaths>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopSurface {

DesktopContentsController::DesktopContentsController(QObject *parent)
    : DesktopContentsController(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                parent)
{
}

DesktopContentsController::DesktopContentsController(QString root, QObject *parent)
    : QObject(parent), m_root(std::move(root))
{
    initializeMutation();
    refresh();
}

DesktopContentsController::DesktopContentsController(QString root, QStringList fileManagerPrograms,
                                                     QObject *parent)
    : QObject(parent), m_root(std::move(root)),
      m_fileManagerPrograms(std::move(fileManagerPrograms))
{
    initializeMutation();
    refresh();
}

DesktopContentsController::~DesktopContentsController() = default;

void DesktopContentsController::initializeMutation()
{
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
                // Stable across a rename and unique within the mounted
                // filesystem. Presentation uses this only as a layout key;
                // mutation still receives the complete identity below.
                {QStringLiteral("layoutKey"),
                 QStringLiteral("%1:%2").arg(entry.device).arg(entry.inode)},
            });
        }
    }
    m_listed = std::move(listed);
    m_rows = std::move(rows);
    Q_EMIT rowsChanged();
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
