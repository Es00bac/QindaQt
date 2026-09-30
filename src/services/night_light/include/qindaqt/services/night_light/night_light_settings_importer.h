// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <memory>
#include <QVariantMap>

namespace QindaQt::Services::SettingsClient { class SettingsClient; }
namespace QindaQt::Services::NightLight {
class NightLightConfigPort;

class NightLightSettingsImporter final : public QObject {
    Q_OBJECT
public:
    enum class Status { Idle, Importing, Imported, InvalidLegacy, SaveFailed, RetryRequired };
    Q_ENUM(Status)

    NightLightSettingsImporter(SettingsClient::SettingsClient &settings,
                                NightLightConfigPort &legacyReader,
                                QObject *parent = nullptr);
    ~NightLightSettingsImporter() override;
    void start();
    Q_INVOKABLE void retry();
    [[nodiscard]] Status status() const noexcept;
    [[nodiscard]] QString message() const;

Q_SIGNALS:
    void stateChanged();

private:
    void attempt(const QVariantMap &values, const QVariantMap &sourceLayers);
    void writeNext();
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::NightLight
