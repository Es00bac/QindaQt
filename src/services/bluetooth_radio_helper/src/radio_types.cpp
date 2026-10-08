// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/bluetooth_radio_helper/radio_types.h>
#include <QtCore/QRegularExpression>
#include <QtDBus/QDBusMetaType>
#include <limits>
#include <time.h>

namespace QindaQt::BluetoothRadio {
namespace {
bool nonceValid(const QString &nonce) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}\\z"));
    return pattern.match(nonce).hasMatch();
}
bool ownerValid(const QString &owner) {
    static const QRegularExpression pattern(QStringLiteral("^:[0-9]+\\.[0-9]+\\z"));
    return owner.size() <= 255 && pattern.match(owner).hasMatch();
}
}
bool validRequest(const Request &request) {
    static const QRegularExpression path(QStringLiteral("^/org/bluez/hci(0|[1-9][0-9]{0,4})\\z"));
    static const QRegularExpression address(QStringLiteral("^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}\\z"));
    if (!nonceValid(request.nonce) || !ownerValid(request.bluezOwner)
        || !ownerValid(request.initiatingCaller) || !path.match(request.adapterPath).hasMatch()
        || !address.match(request.adapterAddress).hasMatch() || !request.deadlineBoottimeMs)
        return false;
    bool valid = false;
    const uint index = request.adapterPath.mid(14).toUInt(&valid);
    return valid && index <= 65535;
}
bool validResult(const Result &result) {
    if (!result.wireValid || !nonceValid(result.nonce)) return false;
    const auto &reason = result.reasonCode;
    switch (result.disposition) {
    case Disposition::VerifiedUnblocked:
        return reason == QLatin1String("radio-unblocked");
    case Disposition::NoWriteUnavailable:
        return reason == QLatin1String("radio-helper-unavailable")
            || reason == QLatin1String("radio-observation-unavailable");
    case Disposition::Refused:
        return reason == QLatin1String("radio-hardware-blocked")
            || reason == QLatin1String("radio-software-blocked")
            || reason == QLatin1String("radio-not-authorized")
            || reason == QLatin1String("radio-stale-target")
            || reason == QLatin1String("radio-busy")
            || reason == QLatin1String("radio-request-rejected");
    case Disposition::Uncertain:
        return reason == QLatin1String("radio-change-uncertain");
    }
    return false;
}
quint64 boottimeMilliseconds() {
    timespec value{};
    if (clock_gettime(CLOCK_BOOTTIME, &value) != 0 || value.tv_sec < 0) return 0;
    const auto seconds = static_cast<quint64>(value.tv_sec);
    if (seconds > (std::numeric_limits<quint64>::max() - 999) / 1000) return 0;
    return seconds * 1000 + static_cast<quint64>(value.tv_nsec / 1000000);
}
void registerDBusTypes() {
    qDBusRegisterMetaType<Request>();
    qDBusRegisterMetaType<Result>();
}
QDBusArgument &operator<<(QDBusArgument &argument, const Request &value) {
    argument.beginStructure();
    argument << value.nonce << value.bluezOwner << value.adapterPath
             << value.adapterAddress << value.initiatingCaller << value.deadlineBoottimeMs;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, Request &value) {
    argument.beginStructure();
    argument >> value.nonce >> value.bluezOwner >> value.adapterPath
             >> value.adapterAddress >> value.initiatingCaller >> value.deadlineBoottimeMs;
    argument.endStructure();
    return argument;
}
QDBusArgument &operator<<(QDBusArgument &argument, const Result &value) {
    argument.beginStructure();
    argument << value.nonce << static_cast<quint32>(value.disposition) << value.reasonCode;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, Result &value) {
    quint32 disposition = 0;
    argument.beginStructure();
    argument >> value.nonce >> disposition >> value.reasonCode;
    argument.endStructure();
    value.disposition = static_cast<Disposition>(disposition);
    value.wireValid = disposition <= static_cast<quint32>(Disposition::Uncertain);
    return argument;
}
} // namespace QindaQt::BluetoothRadio
