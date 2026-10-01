// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_types.h"
#include <QObject>

namespace QindaQt::Apps::RemovableMedia {

// GUI-thread-only asynchronous seam. Consumers borrow the backend for their
// lifetime; errors are visible outcomes, never retries of a mutation. An
// attachment token becomes unusable as soon as its inventory entry disappears
// or the UDisks owner changes. No shell commands or privileged process runs.
class MediaBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual QVector<Volume> volumes() const = 0;
    [[nodiscard]] virtual bool available() const = 0;
    [[nodiscard]] virtual QString diagnostic() const = 0;
    [[nodiscard]] virtual QStringList formatTypes() const = 0;
    virtual void refresh() = 0;
    virtual void execute(const Request &request) = 0;
Q_SIGNALS:
    void changed();
    void finished(const QString &token, bool success, const QString &message,
                  const QString &mountPath);
};
} // namespace QindaQt::Apps::RemovableMedia
