// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/window_decoration_controller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

constexpr auto DecorationGroup = "org.kde.kdecoration2";
constexpr auto QindaQtLibrary = "org.qindaqt";
constexpr auto AuroraeLibrary = "org.kde.kwin.aurorae.v2";
constexpr auto AuroraePrefix = "__aurorae__svg__";

bool isAuroraeLibrary(const QString &library)
{
    return library == QLatin1String("org.kde.kwin.aurorae")
        || library == QLatin1String(AuroraeLibrary);
}

QString decorationId(const QString &library, const QString &theme)
{
    if (isAuroraeLibrary(library)
        && theme.startsWith(QLatin1String(AuroraePrefix))) {
        return QStringLiteral("aurorae:")
            + theme.sliced(QLatin1StringView(AuroraePrefix).size());
    }
    return QStringLiteral("native:") + library;
}

QString themeDisplayName(const QDir &directory)
{
    const QString desktopPath = directory.filePath(QStringLiteral("metadata.desktop"));
    if (QFileInfo::exists(desktopPath)) {
        QSettings metadata(desktopPath, QSettings::IniFormat);
        metadata.beginGroup(QStringLiteral("Desktop Entry"));
        const QString name = metadata.value(QStringLiteral("Name")).toString().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }
    const QString jsonPath = directory.filePath(QStringLiteral("metadata.json"));
    QFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::ReadOnly)) {
        const QJsonObject plugin = QJsonDocument::fromJson(jsonFile.readAll())
                                       .object().value(QStringLiteral("KPlugin"))
                                       .toObject();
        const QString name = plugin.value(QStringLiteral("Name")).toString().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }
    return directory.dirName();
}

bool nativePluginAvailable(const QString &library)
{
    for (const QString &root : QCoreApplication::libraryPaths()) {
        const QDir directory(QDir(root).filePath(
            QStringLiteral("org.kde.kdecoration3")));
        if (QFileInfo::exists(directory.filePath(library + QStringLiteral(".so")))) {
            return true;
        }
    }
    return false;
}

} // namespace

WindowDecorationController::WindowDecorationController(
    QString configPath, QStringList auroraeRoots,
    DecorationReconfigureRequest reconfigure, QObject *parent)
    : QObject(parent)
    , m_configPath(std::move(configPath))
    , m_reconfigure(std::move(reconfigure))
{
    loadCatalog(auroraeRoots);
    readConfigured();
}

void WindowDecorationController::loadCatalog(const QStringList &auroraeRoots)
{
    m_decorations = {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("native:org.qindaqt")},
                    {QStringLiteral("name"), QStringLiteral("QindaQt")},
                    {QStringLiteral("kind"), QStringLiteral("native")},
                    {QStringLiteral("library"), QLatin1String(QindaQtLibrary)},
                    {QStringLiteral("theme"), QString{}}},
    };
    if (nativePluginAvailable(QStringLiteral("org.kde.breeze"))) {
        m_decorations.append(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("native:org.kde.breeze")},
            {QStringLiteral("name"), QStringLiteral("Breeze")},
            {QStringLiteral("kind"), QStringLiteral("native")},
            {QStringLiteral("library"), QStringLiteral("org.kde.breeze")},
            {QStringLiteral("theme"), QString{}}});
    }

    QSet<QString> seenIds;
    seenIds.insert(QStringLiteral("native:org.qindaqt"));
    if (m_decorations.size() > 1) {
        seenIds.insert(QStringLiteral("native:org.kde.breeze"));
    }
    QVariantList aurorae;
    for (const QString &root : auroraeRoots) {
        const QDir rootDirectory(root);
        for (const QString &folder : rootDirectory.entryList(
                 QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (folder.size() > 128 || folder.contains(QLatin1Char('/'))
                || folder.contains(QLatin1String(".."))) {
                continue;
            }
            const QDir themeDirectory(rootDirectory.filePath(folder));
            if (!QFileInfo::exists(themeDirectory.filePath(
                    QStringLiteral("decoration.svg")))) {
                continue;
            }
            const QString id = QStringLiteral("aurorae:") + folder;
            if (seenIds.contains(id)) {
                continue;
            }
            seenIds.insert(id);
            aurorae.append(QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), themeDisplayName(themeDirectory)},
                {QStringLiteral("kind"), QStringLiteral("aurorae")},
                {QStringLiteral("library"), QLatin1String(AuroraeLibrary)},
                {QStringLiteral("theme"), QLatin1String(AuroraePrefix) + folder}});
        }
    }
    std::sort(aurorae.begin(), aurorae.end(), [](const QVariant &left,
                                                  const QVariant &right) {
        return left.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(
                   right.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    m_decorations.append(aurorae);
}

QVariantMap WindowDecorationController::entry(const QString &id) const
{
    for (const QVariant &value : m_decorations) {
        const QVariantMap candidate = value.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == id) {
            return candidate;
        }
    }
    return {};
}

void WindowDecorationController::readConfigured()
{
    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.beginGroup(QLatin1String(DecorationGroup));
    const QString library = settings.value(
        QStringLiteral("library"), QLatin1String(QindaQtLibrary)).toString();
    const QString theme = settings.value(QStringLiteral("theme")).toString();
    settings.endGroup();

    const QString id = decorationId(library, theme);
    if (entry(id).isEmpty()) {
        m_decorations.prepend(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), theme.isEmpty() ? library : theme},
            {QStringLiteral("kind"), QStringLiteral("unavailable")},
            {QStringLiteral("library"), library},
            {QStringLiteral("theme"), theme},
            {QStringLiteral("available"), false}});
    }
    m_configuredId = id;
    m_selectedId = id;
    m_statusText = QStringLiteral("Current window decoration: %1")
                       .arg(configuredName());
    m_errorText.clear();
}

QString WindowDecorationController::configuredName() const
{
    const QVariantMap configured = entry(m_configuredId);
    return configured.isEmpty()
        ? m_configuredId : configured.value(QStringLiteral("name")).toString();
}

bool WindowDecorationController::selectedUsesQindaQt() const
{
    return entry(m_selectedId).value(QStringLiteral("library")).toString()
        == QLatin1String(QindaQtLibrary);
}

bool WindowDecorationController::applyAvailable() const
{
    const QVariantMap selected = entry(m_selectedId);
    return !selected.isEmpty()
        && selected.value(QStringLiteral("available"), true).toBool()
        && m_selectedId != m_configuredId;
}

void WindowDecorationController::setError(QString error)
{
    m_errorText = std::move(error).left(512);
    Q_EMIT stateChanged();
}

bool WindowDecorationController::selectDecoration(const QString &id)
{
    const QVariantMap selected = entry(id);
    if (selected.isEmpty()
        || !selected.value(QStringLiteral("available"), true).toBool()) {
        setError(QStringLiteral("The selected window decoration is unavailable"));
        return false;
    }
    if (m_selectedId == id && m_errorText.isEmpty()) {
        return true;
    }
    m_selectedId = id;
    m_errorText.clear();
    Q_EMIT stateChanged();
    return true;
}

bool WindowDecorationController::applySelection()
{
    if (!applyAvailable()) {
        return false;
    }
    const QVariantMap selected = entry(m_selectedId);
    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.beginGroup(QLatin1String(DecorationGroup));
    settings.setValue(QStringLiteral("library"),
                      selected.value(QStringLiteral("library")));
    const QString theme = selected.value(QStringLiteral("theme")).toString();
    if (theme.isEmpty()) {
        settings.remove(QStringLiteral("theme"));
    } else {
        settings.setValue(QStringLiteral("theme"), theme);
    }
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(QStringLiteral("The window decoration could not be saved"));
        return false;
    }

    QString reconfigureError;
    if (!m_reconfigure || !m_reconfigure(&reconfigureError)) {
        setError(reconfigureError.isEmpty()
                     ? QStringLiteral("The decoration was saved, but KWin could not reload it")
                     : reconfigureError);
        return false;
    }
    m_configuredId = m_selectedId;
    m_statusText = QStringLiteral("Applied window decoration: %1")
                       .arg(configuredName());
    m_errorText.clear();
    Q_EMIT stateChanged();
    return true;
}

void WindowDecorationController::refresh()
{
    readConfigured();
    Q_EMIT stateChanged();
}

QString windowDecorationConfigPath()
{
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("kwinrc"));
}

QStringList windowDecorationThemeRoots()
{
    QStringList roots;
    for (const QString &dataRoot : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
        const QString root = QDir(dataRoot).filePath(
            QStringLiteral("aurorae/themes"));
        if (QFileInfo(root).isDir() && !roots.contains(root)) {
            roots.append(root);
        }
    }
    return roots;
}

bool requestKWinDecorationReconfigure(QString *error)
{
    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(
        request, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        return true;
    }
    if (error != nullptr) {
        const QString detail = reply.errorMessage().isEmpty()
            ? QStringLiteral("no valid reply was received")
            : reply.errorMessage().left(256);
        *error = QStringLiteral("The decoration was saved, but KWin could not reload it: %1")
                     .arg(detail);
    }
    return false;
}

} // namespace QindaQt::Apps::SettingsAppearance
