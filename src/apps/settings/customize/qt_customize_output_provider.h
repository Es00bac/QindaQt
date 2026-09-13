// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_customize/customize_output_provider.h"

#include <QMetaObject>
#include <QVector>

class QGuiApplication;
class QScreen;

namespace QindaQt::Apps::SettingsCustomize {

class QtCustomizeOutputProvider final : public CustomizeOutputProvider {
public:
    explicit QtCustomizeOutputProvider(QGuiApplication &application,
                                       QObject *parent = nullptr);

    [[nodiscard]] CustomizeOutputSnapshot snapshot() const override;

private:
    void attachScreen(QScreen *screen);
    void advanceRevision();

    QGuiApplication &m_application;
    QVector<QMetaObject::Connection> m_screenConnections;
    quint64 m_revision = 1;
};

} // namespace QindaQt::Apps::SettingsCustomize
