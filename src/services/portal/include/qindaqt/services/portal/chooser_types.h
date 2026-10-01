// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/application_catalog/application_directory_scan.h>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
namespace QindaQt::Services::Portal {
struct FilterRule { quint32 kind = 0; QString pattern; friend bool operator==(const FilterRule &, const FilterRule &) = default; };
using FilterRules = QList<FilterRule>;
struct FileFilter { QString label; FilterRules rules; friend bool operator==(const FileFilter &, const FileFilter &) = default; };
using FileFilters = QList<FileFilter>;
using FileNames = QList<QByteArray>;
QDBusArgument &operator<<(QDBusArgument &, const FilterRule &);
const QDBusArgument &operator>>(const QDBusArgument &, FilterRule &);
QDBusArgument &operator<<(QDBusArgument &, const FileFilter &);
const QDBusArgument &operator>>(const QDBusArgument &, FileFilter &);
void registerChooserTypes();
enum class FileChooserMode { Open, Save, SaveMany };
struct FileChooserRequest {
    FileChooserMode mode = FileChooserMode::Open;
    AccessQuestion question;
    bool multiple = false, directory = false;
    QString folder, currentFile, currentName;
    QStringList files;
    FileFilters filters;
    int currentFilter = -1;
};
struct ApplicationCandidate { QString id, label; };
using ApplicationCandidates = QList<ApplicationCandidate>;
struct AppChooserRequest {
    AccessQuestion question;
    QString contentType, uri, filename, lastChoice;
    ApplicationCandidates candidates;
};
// Pure bounded wire validation. Filesystem paths are literal native names,
// never shell text. Parsing does not read files, choose or launch applications.
std::optional<FileChooserRequest> fileChooserRequest(FileChooserMode, const QString &app,
    const QString &parent, const QString &title, const QVariantMap &);
std::optional<AppChooserRequest> appChooserRequest(const QString &app, const QString &parent,
    const QStringList &choices, const QVariantMap &, const QindaQt::ApplicationCatalog::DirectoryScan &);
std::optional<ApplicationCandidates> applicationCandidates(const QStringList &,
    const QindaQt::ApplicationCatalog::DirectoryScan &);
QJsonObject fileChooserFrame(const FileChooserRequest &);
QJsonObject appChooserFrame(const AppChooserRequest &);
QJsonArray candidateFrame(const ApplicationCandidates &);
std::optional<FileChooserRequest> fileChooserFromFrame(const QJsonObject &);
std::optional<AppChooserRequest> appChooserFromFrame(const QJsonObject &);
// Validate helper output against the exact offered request. URI validation is
// syntactic; the GUI owns explicit existing-file and overwrite confirmation.
std::optional<QVariantMap> fileChooserResults(const FileChooserRequest &, const QJsonObject &);
std::optional<QVariantMap> appChooserResults(const AppChooserRequest &, const QJsonObject &);
} // namespace QindaQt::Services::Portal
Q_DECLARE_METATYPE(QindaQt::Services::Portal::FilterRule)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::FilterRules)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::FileFilter)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::FileFilters)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::FileNames)
