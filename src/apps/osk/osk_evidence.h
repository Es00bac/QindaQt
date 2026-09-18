// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::Osk {

class OskKeyboardModel;

// Optional evidence document for the nested harness (`QINDAQT_OSK_EVIDENCE_FILE`):
// the keyboard's visibility, panel size and placed keys in its own logical
// coordinates, rewritten atomically on every change. Off in production.
class OskEvidence final {
public:
    explicit OskEvidence(QString path = {});

    [[nodiscard]] bool enabled() const { return !m_path.isEmpty(); }
    void setVisible(bool visible);
    void setActivated(bool activated);
    void recordPress(const QString &kind, const QString &text);
    void write(const OskKeyboardModel &model);

private:
    QString m_path;
    bool m_visible = false;
    bool m_activated = false;
    QVariantList m_presses;
};

} // namespace QindaQt::Apps::Osk
