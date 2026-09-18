// SPDX-License-Identifier: GPL-3.0-or-later
#include "osk_evidence.h"

#include "osk_keyboard_model.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariantMap>

namespace QindaQt::Apps::Osk {

OskEvidence::OskEvidence(QString path) : m_path(std::move(path)) {}

void OskEvidence::setVisible(bool visible)
{
    m_visible = visible;
}

void OskEvidence::setActivated(bool activated)
{
    m_activated = activated;
}

void OskEvidence::recordPress(const QString &kind, const QString &text)
{
    m_presses.append(QVariantMap{{QStringLiteral("kind"), kind}, {QStringLiteral("text"), text}});
}

void OskEvidence::write(const OskKeyboardModel &model)
{
    if (!enabled()) {
        return;
    }
    QVariantList keys;
    const auto rows = model.placedRows();
    for (const QList<PlacedKey> &row : rows) {
        for (const PlacedKey &key : row) {
            keys.append(QVariantMap{
                {QStringLiteral("kind"), keyKindName(key.kind)},
                {QStringLiteral("label"), key.label},
                {QStringLiteral("x"), key.rect.x()},
                {QStringLiteral("y"), key.rect.y()},
                {QStringLiteral("width"), key.rect.width()},
                {QStringLiteral("height"), key.rect.height()},
            });
        }
    }
    const QVariantMap document{
        {QStringLiteral("visible"), m_visible},
        {QStringLiteral("activated"), m_activated},
        {QStringLiteral("layout"), model.layoutName()},
        {QStringLiteral("shifted"), model.shifted()},
        {QStringLiteral("symbolsPage"), model.symbolsPage()},
        {QStringLiteral("panelSize"), QVariantList{model.metrics().width, model.panelHeight()}},
        {QStringLiteral("keys"), keys},
        {QStringLiteral("presses"), m_presses},
    };
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(document)).toJson());
    file.commit();
}

} // namespace QindaQt::Apps::Osk
