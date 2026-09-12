// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

#include "public/desktop_file_boundary.h"

#include <QStandardPaths>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell::DesktopSurface {

DesktopContentsController::DesktopContentsController(QObject *parent)
    : DesktopContentsController(
          QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
          parent)
{
}

DesktopContentsController::DesktopContentsController(QString root, QObject *parent)
    : QObject(parent), m_root(std::move(root))
{
    refresh();
}

void DesktopContentsController::refresh()
{
    using QindaQt::Apps::FileManager::Desktop::FileBoundary;
    const auto listing = FileBoundary::listLocalFolder(m_root);

    QVariantList rows;
    if (listing.ok()) {
        rows.reserve(listing.entries.size());
        for (const auto &entry : listing.entries) {
            // Dot-entries stay hidden on the desktop presentation, matching
            // every other stock desktop; the boundary itself is not asked to
            // filter, so this is purely a presentation choice.
            if (entry.isHidden) {
                continue;
            }
            rows.append(QVariantMap{
                {QStringLiteral("id"), entry.absolutePath},
                {QStringLiteral("label"), entry.name},
                {QStringLiteral("path"), entry.absolutePath},
                {QStringLiteral("iconName"),
                 entry.isDirectory ? QStringLiteral("folder")
                                   : QStringLiteral("text-x-generic")},
                {QStringLiteral("accessibleName"),
                 QStringLiteral("%1, %2").arg(entry.name, entry.absolutePath)},
                {QStringLiteral("isDirectory"), entry.isDirectory},
            });
        }
    }
    m_rows = std::move(rows);
    Q_EMIT rowsChanged();
    publishFeedback(listing.ok() ? QString() : listing.diagnostic);
}

bool DesktopContentsController::open(const QString &absolutePath)
{
    using QindaQt::Apps::FileManager::Desktop::FileBoundary;
    const auto result = FileBoundary::launchLocalFile(absolutePath);
    if (!result.ok()) {
        publishFeedback(result.diagnostic);
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
