// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_upstream_suite.h"
#include "gain_qualification_evidence.h"
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QStringList>
#include <cmath>

namespace QindaQt::Audio::Test
{
namespace {
bool junitMatches(const QByteArray &bytes, const QMap<QString, QString> &observed,
                  const qint64 failures, const qint64 timeouts)
{
    QXmlStreamReader xml(bytes);
    bool rootSeen = false;
    bool inCase = false;
    QString current;
    QStringList flags;
    QSet<QString> seen;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::DTD
            || xml.tokenType() == QXmlStreamReader::EntityReference)
            return false;
        if (xml.isStartElement()) {
            const auto name = xml.name();
            if (!rootSeen) {
                if (name != QLatin1String("testsuites")
                    || xml.attributes().value(QStringLiteral("tests")) != QString::number(observed.size())
                    || xml.attributes().value(QStringLiteral("failures")) != QString::number(failures)
                    || xml.attributes().value(QStringLiteral("errors")) != QString::number(timeouts))
                    return false;
                rootSeen = true;
            }
            if (name == QLatin1String("testcase")) {
                if (inCase) return false;
                current = xml.attributes().value(QStringLiteral("name")).toString();
                if (!observed.contains(current) || seen.contains(current)) return false;
                inCase = true;
                flags.clear();
            } else if (inCase && (name == QLatin1String("failure") || name == QLatin1String("error")
                                 || name == QLatin1String("skipped"))) {
                flags.append(name.toString());
            }
        } else if (xml.isEndElement() && xml.name() == QLatin1String("testcase")) {
            const QString result = observed.value(current);
            const QStringList expected = result == QLatin1String("OK") ? QStringList{}
                : QStringList{result == QLatin1String("FAIL")
                    ? QStringLiteral("failure") : QStringLiteral("error")};
            if (!inCase || flags != expected) return false;
            seen.insert(current);
            inCase = false;
        }
    }
    return rootSeen && !xml.hasError() && !inCase && seen.size() == observed.size();
}
} // namespace
bool gainCompleteUpstreamSuite(const QMap<QString, QByteArray> &payloads,
                               const QString &root, const QJsonObject &counts,
                               const qint64 registryCount, const bool failed)
{
    const auto registryDocument = QJsonDocument::fromJson(payloads.value(QStringLiteral("registry")));
    if (!registryDocument.isArray() || registryDocument.array().size() != registryCount
        || registryCount <= 0 || registryCount > 256)
        return false;
    QMap<QString, QJsonArray> declared;
    for (const auto &value : registryDocument.array()) {
        const auto entry = value.toObject();
        const auto name = entry.value(QStringLiteral("name")).toString();
        const auto command = entry.value(QStringLiteral("cmd")).toArray();
        if (!value.isObject() || name.isEmpty() || name.size() > 256 || command.isEmpty()
            || command.size() > 64 || entry.value(QStringLiteral("suite")).toArray()
                != QJsonArray{QStringLiteral("pipewire")})
            return false;
        for (const auto &arg : command)
            if (!arg.isString() || arg.toString().isEmpty() || arg.toString().size() > 4096) return false;
        const auto identity = QStringLiteral("pipewire:") + name;
        if (declared.contains(identity)) return false;
        declared.insert(identity, command);
    }
    auto lines = payloads.value(QStringLiteral("json")).split('\n');
    if (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    if (lines.size() != registryCount) return false;
    QMap<QString, QString> observed;
    qint64 ok = 0, fail = 0, timeout = 0;
    for (const auto &line : lines) {
        const auto document = QJsonDocument::fromJson(line);
        const auto row = document.object();
        const auto name = row.value(QStringLiteral("name")).toString();
        const auto result = row.value(QStringLiteral("result")).toString();
        const auto duration = row.value(QStringLiteral("duration"));
        const auto code = row.value(QStringLiteral("returncode"));
        const auto isFail = row.value(QStringLiteral("is_fail"));
        const auto env = row.value(QStringLiteral("env")).toObject();
        if (!document.isObject() || !declared.contains(name) || observed.contains(name)
            || row.value(QStringLiteral("command")).toArray() != declared.value(name)
            || (result != QLatin1String("OK") && result != QLatin1String("FAIL")
                && result != QLatin1String("TIMEOUT"))
            || !gainInteger(code, -2147483648LL, 2147483647)
            || !duration.isDouble() || !std::isfinite(duration.toDouble())
            || duration.toDouble() < 0 || duration.toDouble() > 7200
            || !isFail.isBool() || isFail.toBool() != (result != QLatin1String("OK"))
            || (code.toInteger() == 0) != (result == QLatin1String("OK"))
            || env.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")).toString()
                != QStringLiteral("unix:path=") + root + QStringLiteral("/absent-session")
            || env.value(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS")).toString()
                != QStringLiteral("unix:path=") + root + QStringLiteral("/absent-system"))
            return false;
        const auto unsetValue = env.value(QStringLiteral("ENV_UNSET"));
        if (!unsetValue.isUndefined() && !unsetValue.isString()) return false;
        const auto unset = unsetValue.toString().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (unset.contains(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"))
            || unset.contains(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"))) return false;
        observed.insert(name, result);
        if (result == QLatin1String("OK")) ++ok;
        else if (result == QLatin1String("FAIL")) ++fail;
        else ++timeout;
    }
    if (counts.value(QStringLiteral("ok")).toInteger(-1) != ok
        || counts.value(QStringLiteral("fail")).toInteger(-1) != fail
        || counts.value(QStringLiteral("timeout")).toInteger(-1) != timeout
        || failed != (fail + timeout > 0))
        return false;
    const auto text = QString::fromUtf8(payloads.value(QStringLiteral("text")));
    if (text.count(QStringLiteral("test:")) < registryCount) return false;
    for (const auto &pair : {qMakePair(QStringLiteral("Ok"), ok),
                            qMakePair(QStringLiteral("Fail"), fail),
                            qMakePair(QStringLiteral("Timeout"), timeout)}) {
        const QRegularExpression expression(QStringLiteral("^") + pair.first
            + QStringLiteral(":\\s*(\\d+)\\s*$"), QRegularExpression::MultilineOption);
        auto matches = expression.globalMatch(text);
        if (!matches.hasNext()) {
            if (pair.second != 0) return false;
            continue;
        }
        const auto match = matches.next();
        if (match.captured(1).toLongLong() != pair.second || matches.hasNext()) return false;
    }
    return junitMatches(payloads.value(QStringLiteral("junit")), observed, fail, timeout);
}
} // namespace QindaQt::Audio::Test
