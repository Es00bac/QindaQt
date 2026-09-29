// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include "qwayland-ext-data-control-v1.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSocketNotifier>

#include <memory>
#include <vector>

namespace QindaQt::Services::ClipboardWayland {

class DataControlSource final : public QObject,
                                public QtWayland::ext_data_control_source_v1 {
public:
    DataControlSource(::ext_data_control_source_v1 *object,
                      ClipboardModel::ClipboardValue value, QObject *parent);
    ~DataControlSource() override;

protected:
    void ext_data_control_source_v1_send(const QString &mediaType, int32_t fd) override;
    void ext_data_control_source_v1_cancelled() override { deleteLater(); }

private:
    struct PendingWrite;
    void pump(PendingWrite *pending);
    void finish(PendingWrite *pending);
    ClipboardModel::ClipboardValue m_value;
    std::vector<std::unique_ptr<PendingWrite>> m_writes;
};

// True while any published source is still offered. A source the
// compositor cancelled deletes itself, so its QPointer reads null (ADR-0289).
[[nodiscard]] bool anySourceLive(const std::vector<QPointer<DataControlSource>> &sources) noexcept;

class DataControlManager final : public QtWayland::ext_data_control_manager_v1 {
public:
    ~DataControlManager() override
    {
        if (object() != nullptr) {
            destroy();
        }
    }
};

} // namespace QindaQt::Services::ClipboardWayland
