// SPDX-License-Identifier: GPL-3.0-or-later
#include "consent_controller.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <unistd.h>
using namespace QindaQt::Services::Portal;
ConsentController::ConsentController(AccessQuestion question) : m_question(std::move(question)) {
    for (const auto &choice : m_question.choices) m_values.append({choice.id,
        choice.initial.isEmpty() && !choice.options.isEmpty() ? choice.options.first().id : choice.initial});
}
QVariantList ConsentController::choices() const {
    QVariantList result;
    for (const auto &choice : m_question.choices) {
        QVariantList options; int index = 0;
        for (qsizetype i = 0; i < choice.options.size(); ++i) {
            options.append(QVariantMap{{QStringLiteral("id"), choice.options[i].id}, {QStringLiteral("label"), choice.options[i].label}});
            if (choice.options[i].id == choice.initial) index = static_cast<int>(i);
        }
        result.append(QVariantMap{{QStringLiteral("id"), choice.id}, {QStringLiteral("label"), choice.label},
            {QStringLiteral("options"), options}, {QStringLiteral("checked"), choice.initial == QStringLiteral("true")},
            {QStringLiteral("index"), index}});
    }
    return result;
}
void ConsentController::markReady() { m_ready = true; Q_EMIT readyChanged(); }
void ConsentController::choose(const QString &id, const QString &value) {
    for (auto &entry : m_values) if (entry.id == id) { entry.value = value; return; }
}
void ConsentController::grant() { if (m_ready && validChoiceValues(m_question, m_values)) finish(0); }
void ConsentController::deny() { finish(1); }
void ConsentController::fail() { finish(2); }
void ConsentController::finish(quint32 response) {
    if (m_finished) return;
    m_finished = true; QJsonArray choices;
    if (response == 0) for (const auto &value : m_values) choices.append(QJsonObject{{QStringLiteral("id"), value.id}, {QStringLiteral("value"), value.value}});
    const auto output = QJsonDocument(QJsonObject{{QStringLiteral("response"), static_cast<int>(response)},
        {QStringLiteral("choices"), choices}}).toJson(QJsonDocument::Compact);
    const auto written = write(STDOUT_FILENO, output.constData(), static_cast<size_t>(output.size()));
    QCoreApplication::exit(written == output.size() ? 0 : 2);
}
