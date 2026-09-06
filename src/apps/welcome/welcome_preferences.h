// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace QindaQt::Apps::Welcome {

class WelcomePreferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool showAtNextLaunch READ showAtNextLaunch WRITE setShowAtNextLaunch NOTIFY showAtNextLaunchChanged)
public:
    explicit WelcomePreferences(QObject *parent = nullptr);
    [[nodiscard]] bool showAtNextLaunch() const;
    void setShowAtNextLaunch(bool enabled);
Q_SIGNALS:
    void showAtNextLaunchChanged();
private:
    bool m_showAtNextLaunch = true;
};
}
