// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArrayView>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Shell {

struct CompositorOutputAuthorityEntry final {
    QString outputId;
    quint32 priority = 0;

    friend bool operator==(const CompositorOutputAuthorityEntry &,
                           const CompositorOutputAuthorityEntry &) = default;
};

struct CompositorOutputAuthorityFrame final {
    // The adapter binds reads to this unique owner. Consumers additionally
    // join outputGeneration to ShellVisibilitySnapshot before using the order.
    QString uniqueOwner;
    quint64 outputGeneration = 0;
    QVector<CompositorOutputAuthorityEntry> outputs;

    friend bool operator==(const CompositorOutputAuthorityFrame &,
                           const CompositorOutputAuthorityFrame &) = default;
};

enum class CompositorOutputAuthorityDecodeError {
    None,
    PayloadTooLarge,
    InvalidOwner,
    MalformedPayload,
    Unavailable,
    UnsupportedSchema,
    InvalidGeneration,
    InvalidOutput,
};

struct CompositorOutputAuthorityDecodeResult final {
    std::optional<CompositorOutputAuthorityFrame> frame;
    CompositorOutputAuthorityDecodeError error =
        CompositorOutputAuthorityDecodeError::None;
    QString message;

    [[nodiscard]] bool ok() const noexcept
    {
        return frame.has_value()
            && error == CompositorOutputAuthorityDecodeError::None;
    }
};

// Decodes only the fields needed to route notification surfaces. Geometry and
// scale remain authoritative in the separately validated visibility snapshot;
// outputGeneration joins both public Compositor1 projections atomically.
class CompositorOutputAuthorityDecoder final {
public:
    [[nodiscard]] static CompositorOutputAuthorityDecodeResult decode(
        QByteArrayView payload, const QString &uniqueOwner);
};

} // namespace QindaQt::Shell
