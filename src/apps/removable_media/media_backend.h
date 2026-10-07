// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_types.h"
#include <QObject>
#include <qindaqt/services/removable_media_protocol/media_types.h>

namespace QindaQt::Apps::RemovableMedia {
struct BackendCompletion final {
    QString token;
    Operation operation = Operation::Mount;
    QindaQt::RemovableMedia::OperationStatus status = QindaQt::RemovableMedia::OperationStatus::Uncertain;
    QindaQt::RemovableMedia::RemovalMode removalMode = QindaQt::RemovableMedia::RemovalMode::None;
    QString message;
    QString mountPath;
};


// GUI-thread-only asynchronous seam. Consumers borrow the backend for their
// lifetime; errors are visible outcomes, never retries of a mutation. An
// attachment token becomes unusable as soon as its inventory entry disappears
// or the UDisks owner changes. No shell commands or privileged process runs.
class MediaBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual QVector<Volume> volumes() const = 0;
    [[nodiscard]] virtual quint64 authorityGeneration() const { return 0; }
    [[nodiscard]] virtual bool available() const = 0;
    [[nodiscard]] virtual QString diagnostic() const = 0;
    [[nodiscard]] virtual QStringList formatTypes() const = 0;
    [[nodiscard]] virtual bool busy() const { return false; }
    [[nodiscard]] virtual QString pendingDriveIdentity() const { return {}; }
    [[nodiscard]] virtual QindaQt::RemovableMedia::ProgressPhase phase() const {
        return QindaQt::RemovableMedia::ProgressPhase::Idle;
    }
    virtual void refresh() = 0;
    virtual void execute(const Request &request) = 0;
Q_SIGNALS:
    void changed();
    void operationCompleted(const BackendCompletion &result);
    void finished(const QString &token, bool success, const QString &message,
                  const QString &mountPath);
};
} // namespace QindaQt::Apps::RemovableMedia
