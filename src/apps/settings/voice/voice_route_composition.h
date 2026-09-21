// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsVoice {

class VoiceRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)

public:
    explicit VoiceRouteComposition(QObject *parent = nullptr);
    ~VoiceRouteComposition() override;
    [[nodiscard]] QObject *model() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsVoice
