// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QRect>
#include <QRectF>
#include <QStringList>
#include <optional>
namespace QindaQt::WindowManagement {
inline constexpr qsizetype MaximumRequestBytes = 4096;
enum class Operation {
    Focus, Raise, Minimize, Restore, Close, Shade, Unshade, Iconify, Uniconify,
    Maximize, Fullscreen, Rename, Color, Place, Detach, GroupTab, GroupTile,
    NextTab, PreviousTab, ActivateTab, ReorderTab, ResizeSplit, Launch,
};
enum class Status {
    Accepted, Dispatched, Ambiguous, Stale, Invalid, Denied, Unavailable,
    Cancelled, ResourceLimit,
};
struct Target final {
    enum class Kind { Current, Window, Container };
    Kind kind = Kind::Current;
    QString id;
    QString name;
    friend bool operator==(const Target &, const Target &) = default;
};
// Value-only request, independent of input device and platform handles. All
// strings and geometry are validated at the wire boundary before resolution.
struct Command final {
    Operation operation = Operation::Focus;
    Target target;
    QJsonObject arguments;
};
struct Result final {
    Status status = Status::Unavailable;
    QString message;
    QStringList candidates;
    QString contextId;
    QString windowId;
    QString containerId;
};
[[nodiscard]] QString operationName(Operation operation);
[[nodiscard]] QString statusName(Status status);
[[nodiscard]] std::optional<Command> decodeCommand(const QByteArray &wire,
                                                  QString *error = nullptr);
[[nodiscard]] QByteArray encodeResult(const Result &result);
// Rectangles are normalized to the usable output, including panel exclusion.
// Invalid/empty/out-of-bounds input has no geometry result; rounded edges stay
// inside the area even at fractional output scales and negative origins.
[[nodiscard]] std::optional<QRect> regionalFrame(const QRect &usableArea,
                                                const QRectF &region);
[[nodiscard]] QRectF insetRegion(double fraction = 0.9);
} // namespace QindaQt::WindowManagement
