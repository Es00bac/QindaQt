// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/chooser_types.h>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
namespace QindaQt::Services::Portal {
std::optional<ApplicationCandidates> applicationCandidates(const QStringList &choices,
    const QindaQt::ApplicationCatalog::DirectoryScan &catalog) {
    if (choices.size() > 128) return {};
    ApplicationCandidates candidates; QSet<QString> seen;
    static const QRegularExpression id(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,254}$"));
    for (const auto &choice : choices) {
        if (!id.match(choice).hasMatch() || choice.endsWith(QStringLiteral(".desktop")) || seen.contains(choice)) return {};
        seen.insert(choice);
        // Missing/uninstalled entries are never substituted or launched; a
        // later UpdateChoices can introduce newly catalogued candidates.
        if (const auto *application = catalog.application(choice)) {
            const auto &label = application->entry.name;
            if (label.isEmpty() || label.size() > 512) return {};
            for (const auto c : label) if (c.isNull() || c.category() == QChar::Other_Control) return {};
            candidates.append({choice, label});
        }
    }
    return candidates;
}
std::optional<AppChooserRequest> appChooserRequest(const QString &app, const QString &parent,
    const QStringList &choices, const QVariantMap &options, const QindaQt::ApplicationCatalog::DirectoryScan &catalog) {
    auto question = accessQuestion(app, parent, QStringLiteral("Choose an application"), {}, {}, options);
    auto candidates = applicationCandidates(choices, catalog);
    if (!question || !candidates) return {};
    AppChooserRequest r; r.question = *question; r.candidates = *candidates;
    for (const auto *key : {"last_choice", "content_type", "uri", "filename", "activation_token"}) {
        const auto value = options.value(QLatin1String(key), QString{});
        if (value.metaType() != QMetaType::fromType<QString>() || value.toString().size() > 4096) return {};
        for (const auto c : value.toString()) if (c.isNull() || c.category() == QChar::Other_Control) return {};
    }
    r.lastChoice = options.value(QStringLiteral("last_choice")).toString();
    r.contentType = options.value(QStringLiteral("content_type")).toString();
    r.uri = options.value(QStringLiteral("uri")).toString(); r.filename = options.value(QStringLiteral("filename")).toString();
    if (!r.uri.isEmpty()) {
        const QUrl uri(r.uri, QUrl::StrictMode);
        if (!uri.isValid() || uri.isRelative() || uri.toString(QUrl::FullyEncoded) != r.uri) return {};
    }
    if (r.filename.contains('/') || r.filename == QStringLiteral(".") || r.filename == QStringLiteral("..")) return {};
    return r;
}
std::optional<QVariantMap> appChooserResults(const AppChooserRequest &r, const QJsonObject &object) {
    if (object.size() != 1 || !object.value(QStringLiteral("choice")).isString()) return {};
    const auto choice = object.value(QStringLiteral("choice")).toString();
    for (const auto &candidate : r.candidates) if (candidate.id == choice) return QVariantMap{{QStringLiteral("choice"), choice}};
    return {};
}
} // namespace QindaQt::Services::Portal
