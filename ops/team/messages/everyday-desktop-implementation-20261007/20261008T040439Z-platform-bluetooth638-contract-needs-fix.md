# Exact Bluetooth reply provenance contract review: NeedsFix

- Exact candidate: 63833b7e1594484abd13fc92e77f907b004a5897.
- Reviewed receipt: 20261008T040142Z-bluetooth-qt-reply-provenance-proposal.md; current production remains7d7/814. This is source/primary evidence only, no compiler/private-bus/host action.
- Disposition: NeedsFix before replacing impossible reply.service() equality under the proposed broker premise.

## Blocking deployment premise

Primary dbus1.16.2 bus/connection.c confirms requested-reply lookup binds reply serial, sending connection and receiving connection. But bus/policy.c932 and1208 skip requested-only allow rules for unsolicited replies only when the allow rule has eavesdrop=false. The inspected actual /usr/share/dbus-1/session.conf contains send_destination=* eavesdrop=true and receive eavesdrop=true allow rules. It therefore does not establish the requested-only transport prerequisite.
Configuration SHA256206f009ddcf909422f3651c687b4623a5780fb7486c6d650e5332f57c00d6be1. Included /etc/dbus-1/session.conf is an empty legacy busconfig; /etc/dbus-1/session-local.conf absent; /etc/dbus-1/session.d has no *.conf; /usr/share/dbus-1/session.d absent. No later override was found.
Existing PrivateBus fixture starts dbus-daemon --session, inheriting these defaults. A stricter custom broker positive test alone would not qualify ordinary supported session deployment. This is a static policy-contract finding, not an executed forged-reply or live bus exploit.

## Contract portions sound but unqualified

Official Qt v6.11.1 documents reply/error service() empty; inbound MethodCall caller remains a distinct seam. Exact unique-destination pending watcher plus current-owner/UID, full nonce/request, initiating callback, deadline and wire validation remain necessary. They alone do not authenticate an arbitrary correct-serial/nonce reply if the broker permits unsolicited responses.
Author proposal correctly requires completion callback/current-entry recheck; current port lacks that completion recheck and it must be tested including cancellation/reentrancy.
Required future gates: real authority/port positive; foreign connection sends exact serial/nonce/payload but cannot complete; valid captured helper then succeeds; cancel/reentrant invalidation, owner change, late/malformed/wrong nonce/duplicate negatives. Do not relax identity or global bus policy without a separately reviewed authority boundary. Sender-preserving public transport or explicitly owned/reviewed/deployed requested-only policy may resolve this; no implementation choice approved here.

Primary sources:
- https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/dbus/qdbusmessage.cpp
- https://dbus.freedesktop.org/doc/dbus-daemon.1.html
- Existing Portage distfile /var/cache/distfiles/dbus-1.16.2.tar.xz SHA2560ba2a1a4b16afe7bceb2c07e9ce99a8c2c3508e5dec290dbb643384bd6beb7e2, directly inspected bus/connection.c, bus/policy.c and bus/bus.c.
