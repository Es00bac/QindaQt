// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/chooser_types.h>
#include <QFile>
namespace QindaQt::Services::Portal {
namespace {
QJsonObject questionFrame(const AccessQuestion &q) {
    QJsonArray choices;
    for (const auto &choice : q.choices) {
        QJsonArray options;
        for (const auto &option : choice.options) options.append(QJsonObject{{"id", option.id}, {"label", option.label}});
        choices.append(QJsonObject{{"id", choice.id}, {"label", choice.label}, {"initial", choice.initial}, {"options", options}});
    }
    return {{"app", q.appId}, {"parent", q.parentWindow}, {"title", q.title}, {"accept", q.grantLabel}, {"modal", q.modal}, {"choices", choices}};
}
std::optional<AccessQuestion> questionFromFrame(const QJsonObject &frame) {
    if (frame.size() != 6 || !frame.value("modal").isBool() || !frame.value("choices").isArray()) return {};
    for (const auto *key : {"app", "parent", "title", "accept"}) if (!frame.value(QLatin1String(key)).isString()) return {};
    AccessChoices choices;
    for (const auto &value : frame.value("choices").toArray()) {
        const auto row = value.toObject();
        if (row.size() != 4 || !row.value("options").isArray()) return {};
        for (const auto *key : {"id", "label", "initial"}) if (!row.value(QLatin1String(key)).isString()) return {};
        AccessChoice choice{row.value("id").toString(), row.value("label").toString(), {}, row.value("initial").toString()};
        for (const auto &item : row.value("options").toArray()) {
            const auto option = item.toObject();
            if (option.size() != 2 || !option.value("id").isString() || !option.value("label").isString()) return {};
            choice.options.append({option.value("id").toString(), option.value("label").toString()});
        }
        choices.append(choice);
    }
    auto question = accessQuestion(frame.value("app").toString(), frame.value("parent").toString(), frame.value("title").toString(), {}, {},
        {{QStringLiteral("choices"), QVariant::fromValue(choices)}, {QStringLiteral("modal"), frame.value("modal").toBool()},
         {QStringLiteral("grant_label"), frame.value("accept").toString()}});
    if (question) question->denyLabel = QStringLiteral("Cancel");
    return question;
}
}
QJsonArray candidateFrame(const ApplicationCandidates &candidates) {
    QJsonArray result;
    for (const auto &candidate : candidates) result.append(QJsonObject{{"id", candidate.id}, {"label", candidate.label}});
    return result;
}
QJsonObject fileChooserFrame(const FileChooserRequest &r) {
    QJsonArray filters;
    for (const auto &filter : r.filters) {
        QJsonArray rules;
        for (const auto &rule : filter.rules) rules.append(QJsonObject{{"kind", static_cast<int>(rule.kind)}, {"pattern", rule.pattern}});
        filters.append(QJsonObject{{"label", filter.label}, {"rules", rules}});
    }
    return {{"type", "file"}, {"mode", static_cast<int>(r.mode)}, {"question", questionFrame(r.question)},
        {"multiple", r.multiple}, {"directory", r.directory}, {"folder", r.folder}, {"currentFile", r.currentFile},
        {"currentName", r.currentName}, {"files", QJsonArray::fromStringList(r.files)}, {"filters", filters}, {"currentFilter", r.currentFilter}};
}
std::optional<FileChooserRequest> fileChooserFromFrame(const QJsonObject &frame) {
    if (frame.size() != 11 || frame.value("type") != QJsonValue("file") || !frame.value("mode").isDouble()
        || !frame.value("multiple").isBool() || !frame.value("directory").isBool() || !frame.value("files").isArray()
        || !frame.value("filters").isArray() || !frame.value("currentFilter").isDouble()) return {};
    const int mode = frame.value("mode").toInt(-1); if (mode < 0 || mode > 2) return {};
    const auto question = questionFromFrame(frame.value("question").toObject()); if (!question) return {};
    QVariantMap options{{QStringLiteral("choices"), QVariant::fromValue(question->choices)}, {QStringLiteral("modal"), question->modal},
        {QStringLiteral("accept_label"), question->grantLabel}, {QStringLiteral("multiple"), frame.value("multiple").toBool()},
        {QStringLiteral("directory"), frame.value("directory").toBool()}};
    for (const auto *key : {"folder", "currentFile", "currentName"}) if (!frame.value(QLatin1String(key)).isString()) return {};
    const auto addPath = [&options, &frame](const char *key, const char *wire) {
        const auto value = frame.value(QLatin1String(key)).toString();
        if (!value.isEmpty()) options.insert(QLatin1String(wire), QFile::encodeName(value) + '\0');
    };
    addPath("folder", "current_folder"); addPath("currentFile", "current_file");
    if (!frame.value("currentName").toString().isEmpty()) options.insert(QStringLiteral("current_name"), frame.value("currentName").toString());
    FileNames names; for (const auto &value : frame.value("files").toArray()) {
        if (!value.isString()) return {}; names.append(QFile::encodeName(value.toString()) + '\0');
    }
    options.insert(QStringLiteral("files"), QVariant::fromValue(names));
    FileFilters filters;
    for (const auto &value : frame.value("filters").toArray()) {
        const auto row = value.toObject();
        if (row.size() != 2 || !row.value("label").isString() || !row.value("rules").isArray()) return {};
        FileFilter filter{row.value("label").toString(), {}};
        for (const auto &item : row.value("rules").toArray()) {
            const auto rule = item.toObject(); const int kind = rule.value("kind").toInt(-1);
            if (rule.size() != 2 || kind < 0 || kind > 1 || !rule.value("pattern").isString()) return {};
            filter.rules.append({static_cast<quint32>(kind), rule.value("pattern").toString()});
        }
        filters.append(filter);
    }
    options.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
    const int filter = frame.value("currentFilter").toInt(-2);
    if (filter < -1 || filter >= filters.size()) return {};
    if (filter >= 0) options.insert(QStringLiteral("current_filter"), QVariant::fromValue(filters[filter]));
    return fileChooserRequest(static_cast<FileChooserMode>(mode), question->appId, question->parentWindow, question->title, options);
}
QJsonObject appChooserFrame(const AppChooserRequest &r) {
    return {{"type", "app"}, {"question", questionFrame(r.question)}, {"contentType", r.contentType},
        {"uri", r.uri}, {"filename", r.filename}, {"lastChoice", r.lastChoice}, {"candidates", candidateFrame(r.candidates)}};
}
std::optional<AppChooserRequest> appChooserFromFrame(const QJsonObject &frame) {
    if (frame.size() != 7 || frame.value("type") != QJsonValue("app") || !frame.value("candidates").isArray()) return {};
    const auto question = questionFromFrame(frame.value("question").toObject()); if (!question) return {};
    for (const auto *key : {"contentType", "uri", "filename", "lastChoice"}) if (!frame.value(QLatin1String(key)).isString()) return {};
    // Helper names originate in the parent process's public catalog. Revalidate
    // IDs/options without a second ambient XDG scan across the process boundary.
    QindaQt::ApplicationCatalog::DirectoryScan catalog; QStringList ids;
    for (const auto &value : frame.value("candidates").toArray()) {
        const auto row = value.toObject(); if (row.size() != 2 || !row.value("id").isString() || !row.value("label").isString()) return {};
        const auto label = row.value("label").toString(); if (label.isEmpty() || label.size() > 512) return {};
        QindaQt::ApplicationCatalog::ScannedApplication app; app.entry.id = row.value("id").toString(); app.entry.name = label;
        ids.append(app.entry.id); catalog.applications.append(app);
    }
    return appChooserRequest(question->appId, question->parentWindow, ids,
        {{QStringLiteral("content_type"), frame.value("contentType").toString()}, {QStringLiteral("uri"), frame.value("uri").toString()},
         {QStringLiteral("filename"), frame.value("filename").toString()}, {QStringLiteral("last_choice"), frame.value("lastChoice").toString()},
         {QStringLiteral("modal"), question->modal}}, catalog);
}
} // namespace QindaQt::Services::Portal
