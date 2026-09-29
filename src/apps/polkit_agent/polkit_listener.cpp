// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_listener.h"

#include <PolkitQt1/Agent/Session>
#include <PolkitQt1/Subject>

#include <pwd.h>
#include <unistd.h>

#include <array>

namespace QindaQt::Apps::PolkitAgent {
namespace {

QString detailValue(const PolkitQt1::Details &details, const QString &key)
{
    return details.lookup(key);
}

// AGENT-NOTE: "polkit.subject-pid" is polkitd's documented detail key for the
// pid of the process the authentication is on behalf of (the same key
// polkit-kde-agent and polkit-gnome-authentication-agent read to show "which
// program is asking"). Absence resolves to pid <= 0, which
// PolkitRequesterResolver treats as honestly unresolvable.
qint64 subjectPid(const PolkitQt1::Details &details)
{
    bool ok = false;
    const qint64 pid = detailValue(details, QStringLiteral("polkit.subject-pid")).toLongLong(&ok);
    return ok ? pid : -1;
}

QList<AgentIdentity> translateIdentities(const PolkitQt1::Identity::List &identities)
{
    QList<AgentIdentity> result;
    result.reserve(identities.size());
    const qint64 currentUid = static_cast<qint64>(::getuid());
    for (PolkitQt1::Identity identity : identities) {
        PolkitQt1::UnixUserIdentity userIdentity = identity.toUnixUserIdentity();
        if (!userIdentity.isValid()) {
            // AGENT-NOTE: polkit can in principle list a group identity; this
            // agent only ever offers user identities to authenticate as,
            // matching every other first-party polkit agent.
            continue;
        }
        const uid_t uid = userIdentity.uid();
        struct passwd pwbuf {};
        struct passwd *pw = nullptr;
        std::array<char, 4096> buffer{};
        QString login;
        QString fullName;
        if (::getpwuid_r(uid, &pwbuf, buffer.data(), buffer.size(), &pw) == 0 && pw != nullptr) {
            login = QString::fromLocal8Bit(pw->pw_name);
            fullName = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0);
        }
        AgentIdentity agentIdentity;
        agentIdentity.displayLabel =
            formatUserIdentityLabel(static_cast<qint64>(uid), login, fullName);
        agentIdentity.token = identity.toString();
        agentIdentity.isCurrentUser = static_cast<qint64>(uid) == currentUid;
        result.append(agentIdentity);
    }
    return result;
}

// The only concrete AuthenticationAttempt: a real, single-use
// PolkitQt1::Agent::Session. A fresh instance is created for every retry, as
// polkit-qt requires.
class PolkitSessionAttempt final : public AuthenticationAttempt {
public:
    PolkitSessionAttempt(const PolkitQt1::Identity &identity, const QString &cookie,
                         PolkitQt1::Agent::AsyncResult *result)
        : m_session(identity, cookie, result)
    {
        connect(&m_session, &PolkitQt1::Agent::Session::request, this,
               [this](const QString &text, bool echo) { Q_EMIT request(text, echo); });
        connect(&m_session, &PolkitQt1::Agent::Session::showInfo, this,
               [this](const QString &text) { Q_EMIT showInfo(text); });
        connect(&m_session, &PolkitQt1::Agent::Session::showError, this,
               [this](const QString &text) { Q_EMIT showError(text); });
        connect(&m_session, &PolkitQt1::Agent::Session::completed, this,
               [this](bool gained) { Q_EMIT completed(gained); });
        m_session.initiate();
    }

    void respond(const QString &response) override { m_session.setResponse(response); }
    void cancel() override { m_session.cancel(); }

private:
    PolkitQt1::Agent::Session m_session;
};

} // namespace

PolkitListener::PolkitListener(PolkitRequesterResolver &requesterResolver,
                                         QObject *parent)
    : PolkitQt1::Agent::Listener(parent)
    , m_requesterResolver(requesterResolver)
    , m_queue([this](const AuthenticationRequest &request) {
          const QString cookie = request.cookie;
          return std::make_unique<PolkitAttemptController>(
              request.identities, selectPreferredIdentityIndex(request.identities),
              [this, cookie](const QString &identityToken) -> std::unique_ptr<AuthenticationAttempt> {
                  return std::make_unique<PolkitSessionAttempt>(
                      PolkitQt1::Identity::fromString(identityToken), cookie,
                      m_pendingResults.value(cookie));
              });
      })
{
    connect(&m_queue, &PolkitRequestQueue::requestActivated, this,
           [this](const AuthenticationRequest &request, PolkitAttemptController *controller) {
               const QString cookie = request.cookie;
               connect(controller, &PolkitAttemptController::completed, this,
                      [this, cookie](bool) {
                          if (PolkitQt1::Agent::AsyncResult *result = m_pendingResults.take(cookie)) {
                              result->setCompleted();
                          }
                      });
           });
}

PolkitListener::~PolkitListener() = default;

void PolkitListener::initiateAuthentication(
    const QString &actionId, const QString &message, const QString &iconName,
    const PolkitQt1::Details &details, const QString &cookie,
    const PolkitQt1::Identity::List &identities, PolkitQt1::Agent::AsyncResult *result)
{
    AuthenticationRequest request;
    request.actionId = actionId;
    request.message = message;
    request.iconName = iconName;
    // AGENT-NOTE: "polkit.action_vendor" is this agent's best-effort read of
    // the action's vendor string; unlike polkit.subject-pid it has not been
    // verified against a live polkitd (this agent is never registered
    // against the real daemon by test or build automation -- see the "Live
    // check" caveat in ADR-0290 and docs/wiki/apps/polkit-agent.md). An
    // absent or wrong key just leaves the Details disclosure's vendor line
    // blank, which the dialog already treats as honest absence.
    request.vendorName = detailValue(details, QStringLiteral("polkit.action_vendor"));
    request.cookie = cookie;
    request.identities = translateIdentities(identities);
    request.requester = m_requesterResolver.resolve(subjectPid(details));

    m_pendingResults.insert(cookie, result);
    m_queue.enqueue(std::move(request));
}

bool PolkitListener::initiateAuthenticationFinish()
{
    // polkit-qt's own header marks this "TODO: is this method really
    // required?" and no first-party agent this codebase could reference
    // does anything but accept; there is nothing more for this agent to
    // finish once initiateAuthentication has enqueued the request.
    return true;
}

void PolkitListener::cancelAuthentication()
{
    m_queue.cancelActive();
}

} // namespace QindaQt::Apps::PolkitAgent
