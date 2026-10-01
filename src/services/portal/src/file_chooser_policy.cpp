// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/chooser_types.h>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSet>
#include <QUrl>
namespace QindaQt::Services::Portal {
namespace {
bool text(const QString &s, qsizetype maximum = 4096) {
    if (s.size() > maximum) return false;
    for (const auto c : s) if (c.isNull() || c.category() == QChar::Other_Control) return false;
    return true;
}
template<class T> std::optional<T> typed(const QVariant &v, const QString &signature) {
    if (v.metaType() == QMetaType::fromType<T>()) return v.value<T>();
    if (v.metaType() != QMetaType::fromType<QDBusArgument>()) return {};
    const auto argument = v.value<QDBusArgument>();
    return argument.currentSignature() == signature ? std::optional(qdbus_cast<T>(argument)) : std::nullopt;
}
std::optional<QString> path(const QByteArray &bytes, bool basename = false) {
    if (bytes.isEmpty() || bytes.size() > 4096 || bytes.back() != '\0' || bytes.left(bytes.size()-1).contains('\0')) return {};
    const auto name = QFile::decodeName(bytes.left(bytes.size()-1));
    if (!text(name) || name.isEmpty() || QFile::encodeName(name) != bytes.left(bytes.size()-1)) return {};
    if (basename ? (name.contains('/') || name == QStringLiteral(".") || name == QStringLiteral("..")) : !QDir::isAbsolutePath(name)) return {};
    return name;
}
bool validFilter(const FileFilter &filter) {
    if (filter.label.isEmpty() || !text(filter.label, 512) || filter.rules.isEmpty() || filter.rules.size() > 32) return false;
    for (const auto &rule : filter.rules) if (rule.kind > 1 || rule.pattern.isEmpty() || !text(rule.pattern, 512)) return false;
    return true;
}
}
std::optional<FileChooserRequest> fileChooserRequest(FileChooserMode mode, const QString &app,
    const QString &parent, const QString &title, const QVariantMap &options) {
    auto normalized = options;
    if (options.contains(QStringLiteral("choices"))) {
        auto choices = typed<AccessChoices>(options.value(QStringLiteral("choices")), QStringLiteral("a(ssa(ss)s)"));
        if (!choices) return {};
        for (auto &choice : *choices) {
            if (choice.label.isEmpty()) return {};
            for (const auto &option : choice.options) if (option.label.isEmpty()) return {};
            // FileChooser permits an empty initial Boolean choice; choose false
            // without weakening the separate Access question contract.
            if (choice.options.isEmpty() && choice.initial.isEmpty()) choice.initial = QStringLiteral("false");
        }
        normalized.insert(QStringLiteral("choices"), QVariant::fromValue(*choices));
    }
    const auto question = accessQuestion(app, parent, title, {}, {}, normalized);
    if (!question) return {};
    FileChooserRequest r; r.mode = mode; r.question = *question;
    r.question.grantLabel = mode == FileChooserMode::Open ? QStringLiteral("Open") : QStringLiteral("Save");
    r.question.denyLabel = QStringLiteral("Cancel");
    if (options.contains(QStringLiteral("accept_label"))) {
        const auto label = options.value(QStringLiteral("accept_label"));
        if (label.metaType() != QMetaType::fromType<QString>() || !text(label.toString(), 512) || label.toString().isEmpty()) return {};
        r.question.grantLabel = label.toString();
    }
    for (const auto *key : {"multiple", "directory"}) if (options.contains(QLatin1String(key))
        && options.value(QLatin1String(key)).metaType() != QMetaType::fromType<bool>()) return {};
    r.multiple = mode == FileChooserMode::Open && options.value(QStringLiteral("multiple"), false).toBool();
    r.directory = mode == FileChooserMode::SaveMany || (mode == FileChooserMode::Open && options.value(QStringLiteral("directory"), false).toBool());
    for (const auto *key : {"current_folder", "current_file"}) if (options.contains(QLatin1String(key))) {
        const auto value = options.value(QLatin1String(key));
        if (value.metaType() != QMetaType::fromType<QByteArray>()) return {};
        const auto name = path(value.toByteArray()); if (!name) return {};
        if (QLatin1String(key) == QStringLiteral("current_folder")) r.folder = *name; else r.currentFile = *name;
    }
    if (options.contains(QStringLiteral("current_name"))) {
        const auto name = options.value(QStringLiteral("current_name"));
        if (name.metaType() != QMetaType::fromType<QString>()) return {};
        const auto decoded = path(QFile::encodeName(name.toString()) + '\0', true); if (!decoded) return {};
        r.currentName = *decoded;
    }
    if (options.contains(QStringLiteral("filters"))) {
        const auto filters = typed<FileFilters>(options.value(QStringLiteral("filters")), QStringLiteral("a(sa(us))"));
        if (!filters || filters->size() > 32) return {};
        r.filters = *filters;
        qsizetype total = 0;
        for (const auto &filter : r.filters) {
            if (!validFilter(filter)) return {};
            total += filter.label.size(); for (const auto &rule : filter.rules) total += rule.pattern.size();
            if (total > 32768) return {};
        }
    }
    if (options.contains(QStringLiteral("current_filter"))) {
        const auto filter = typed<FileFilter>(options.value(QStringLiteral("current_filter")), QStringLiteral("(sa(us))"));
        if (!filter || !validFilter(*filter)) return {};
        if (r.filters.isEmpty()) r.filters.append(*filter);
        r.currentFilter = static_cast<int>(r.filters.indexOf(*filter));
        if (r.currentFilter < 0) return {};
    } else if (!r.filters.isEmpty()) r.currentFilter = 0;
    if (mode == FileChooserMode::SaveMany) {
        const auto names = typed<FileNames>(options.value(QStringLiteral("files")), QStringLiteral("aay"));
        if (!names || names->isEmpty() || names->size() > 128) return {};
        qsizetype total = 0;
        for (const auto &bytes : *names) {
            const auto name = path(bytes, true); if (!name || (total += bytes.size()) > 32768) return {}; r.files.append(*name);
        }
    }
    return r;
}
std::optional<QVariantMap> fileChooserResults(const FileChooserRequest &r, const QJsonObject &object) {
    if (!object.value(QStringLiteral("uris")).isArray() || !object.value(QStringLiteral("choices")).isArray()
        || !object.value(QStringLiteral("filter")).isDouble() || object.size() != 3) return {};
    const auto uris = object.value(QStringLiteral("uris")).toArray();
    const auto count = r.mode == FileChooserMode::SaveMany ? r.files.size() : (r.multiple ? 128 : 1);
    if (uris.isEmpty() || uris.size() > count || (r.mode == FileChooserMode::SaveMany && uris.size() != count)) return {};
    QStringList paths; QSet<QString> seen;
    for (const auto &value : uris) {
        if (!value.isString() || !text(value.toString(), 16384)) return {};
        const QUrl uri(value.toString(), QUrl::StrictMode);
        if (!uri.isValid() || !uri.isLocalFile() || !uri.host().isEmpty() || uri.hasQuery() || uri.hasFragment()
            || !QDir::isAbsolutePath(uri.toLocalFile()) || uri.toString(QUrl::FullyEncoded) != value.toString()
            || QDir::cleanPath(uri.toLocalFile()) != uri.toLocalFile() || seen.contains(value.toString())) return {};
        seen.insert(value.toString()); paths.append(value.toString());
    }
    ChoiceValues values;
    for (const auto &value : object.value(QStringLiteral("choices")).toArray()) {
        const auto item = value.toObject();
        if (item.size() != 2 || !item.value(QStringLiteral("id")).isString() || !item.value(QStringLiteral("value")).isString()) return {};
        values.append({item.value(QStringLiteral("id")).toString(), item.value(QStringLiteral("value")).toString()});
    }
    if (!validChoiceValues(r.question, values)) return {};
    const int index = object.value(QStringLiteral("filter")).toInt(-2);
    if (index < -1 || index >= r.filters.size() || (r.filters.isEmpty() ? index != -1 : index < 0)) return {};
    QVariantMap result{{QStringLiteral("uris"), paths}, {QStringLiteral("choices"), QVariant::fromValue(values)}};
    if (index >= 0) result.insert(QStringLiteral("current_filter"), QVariant::fromValue(r.filters[index]));
    if (r.mode == FileChooserMode::Open) result.insert(QStringLiteral("writable"), false);
    return result;
}
} // namespace QindaQt::Services::Portal
