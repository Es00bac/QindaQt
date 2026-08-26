// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/qt_output_inventory.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QScreen>
#include <QThread>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::ShellSurface {
namespace {

QtOutputInventoryResult failure(QtOutputInventoryErrorCode code, QString message)
{
    return {{}, code, std::move(message)};
}

QGuiApplication *guiApplication(QString *error)
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (application == nullptr) {
        if (error) {
            *error = QStringLiteral("a QGuiApplication is required to enumerate outputs");
        }
        return nullptr;
    }
    if (application->thread() != QThread::currentThread()) {
        if (error) {
            *error = QStringLiteral("Qt output inventory must be read on the GUI thread");
        }
        return nullptr;
    }
    return application;
}

bool hasSafeExtent(const QRect &geometry)
{
    if (!geometry.isValid()) {
        return false;
    }
    const qint64 width = static_cast<qint64>(geometry.right()) - geometry.left() + 1;
    const qint64 height = static_cast<qint64>(geometry.bottom()) - geometry.top() + 1;
    return width > 0 && width <= std::numeric_limits<int>::max() && height > 0 &&
        height <= std::numeric_limits<int>::max();
}

} // namespace

QtOutputInventoryResult QtOutputInventory::read()
{
    QString diagnostic;
    auto *application = guiApplication(&diagnostic);
    if (application == nullptr) {
        const auto code = QCoreApplication::instance() == nullptr ||
                qobject_cast<QGuiApplication *>(QCoreApplication::instance()) == nullptr
            ? QtOutputInventoryErrorCode::MissingGuiApplication
            : QtOutputInventoryErrorCode::WrongThread;
        return failure(code, std::move(diagnostic));
    }

    const auto screens = application->screens();
    if (screens.isEmpty()) {
        return failure(QtOutputInventoryErrorCode::EmptyInventory,
                       QStringLiteral("Qt reports no enabled outputs"));
    }

    QVector<ShellLayout::LogicalOutput> staged;
    staged.reserve(screens.size());
    QHash<QString, QScreen *> seen;
    for (QScreen *screen : screens) {
        if (screen == nullptr || screen->name().trimmed().isEmpty()) {
            return failure(QtOutputInventoryErrorCode::InvalidScreenId,
                           QStringLiteral("Qt reports an output without a stable name"));
        }
        if (seen.contains(screen->name())) {
            return failure(QtOutputInventoryErrorCode::DuplicateScreenId,
                           QStringLiteral("Qt reports duplicate output name '%1'")
                               .arg(screen->name()));
        }
        if (!hasSafeExtent(screen->geometry())) {
            return failure(QtOutputInventoryErrorCode::InvalidGeometry,
                           QStringLiteral("output '%1' has invalid logical geometry")
                               .arg(screen->name()));
        }
        const qreal scale = screen->devicePixelRatio();
        if (!std::isfinite(scale) || scale <= 0.0) {
            return failure(QtOutputInventoryErrorCode::InvalidScale,
                           QStringLiteral("output '%1' has invalid scale metadata")
                               .arg(screen->name()));
        }
        seen.insert(screen->name(), screen);
        staged.push_back({screen->name(), screen->geometry(), scale});
    }
    return {std::move(staged), QtOutputInventoryErrorCode::None, {}};
}

QScreen *QtOutputInventory::screenForId(const QString &id, QString *error)
{
    QString diagnostic;
    auto *application = guiApplication(&diagnostic);
    if (application == nullptr) {
        if (error) {
            *error = std::move(diagnostic);
        }
        return nullptr;
    }
    if (id.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("the requested output id is empty");
        }
        return nullptr;
    }

    QScreen *match = nullptr;
    for (QScreen *screen : application->screens()) {
        if (screen != nullptr && screen->name() == id) {
            if (match != nullptr) {
                if (error) {
                    *error = QStringLiteral("Qt reports duplicate output name '%1'").arg(id);
                }
                return nullptr;
            }
            match = screen;
        }
    }
    if (match == nullptr && error) {
        *error = QStringLiteral("Qt output '%1' is no longer available").arg(id);
    }
    return match;
}

} // namespace QindaQt::ShellSurface
