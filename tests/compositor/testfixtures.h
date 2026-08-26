// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/controltypes.h"
#include "qindaqt/compositor/sceneadapter.h"

#include "windowcontainer.h"

#include <QJsonArray>

namespace QindaQt::Compositor::Test {

class RecordingSceneAdapter;

class RecordingSceneTransaction final : public SceneTransaction
{
public:
    explicit RecordingSceneTransaction(RecordingSceneAdapter &adapter);
    [[nodiscard]] bool commit(QString *error) override;

private:
    RecordingSceneAdapter &m_adapter;
};

class RecordingSceneAdapter final : public SceneAdapter
{
public:
    bool prepareSucceeds = true;
    bool commitSucceeds = true;
    int prepareCalls = 0;
    int commitCalls = 0;
    QJsonObject before;
    QJsonObject after;

    [[nodiscard]] std::unique_ptr<SceneTransaction> prepareTransition(
        const Core::WindowContainer &beforeModel,
        const Core::WindowContainer &afterModel,
        QString *error) override
    {
        ++prepareCalls;
        before = beforeModel.toJson();
        after = afterModel.toJson();
        if (!prepareSucceeds) {
            if (error) {
                *error = QStringLiteral("fixture preparation failure");
            }
            return nullptr;
        }
        return std::make_unique<RecordingSceneTransaction>(*this);
    }
};

inline RecordingSceneTransaction::RecordingSceneTransaction(RecordingSceneAdapter &adapter)
    : m_adapter(adapter)
{
}

inline bool RecordingSceneTransaction::commit(QString *error)
{
    ++m_adapter.commitCalls;
    if (!m_adapter.commitSucceeds && error) {
        *error = QStringLiteral("fixture commit failure with rollback");
    }
    return m_adapter.commitSucceeds;
}

inline Core::WindowContainer seedContainer()
{
    Core::WindowContainer container(QStringLiteral("container-a"));
    QString error;
    const bool added = container.addPage(QStringLiteral("page-a"),
                                         QStringLiteral("leaf-a"),
                                         QStringLiteral("window-a"),
                                         &error);
    Q_ASSERT_X(added, "seedContainer", qPrintable(error));
    return container;
}

inline ControlRequest request(quint64 revision, QVector<QJsonObject> operations)
{
    return {{},
            QStringLiteral("transaction-a"),
            QStringLiteral("container-a"),
            revision,
            std::move(operations)};
}

inline QJsonObject requestJson(quint64 revision, const QJsonArray &operations)
{
    return {{QStringLiteral("protocol"),
             QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
            {QStringLiteral("transactionId"), QStringLiteral("transaction-a")},
            {QStringLiteral("containerId"), QStringLiteral("container-a")},
            {QStringLiteral("expectedRevision"), QString::number(revision)},
            {QStringLiteral("operations"), operations}};
}

} // namespace QindaQt::Compositor::Test
