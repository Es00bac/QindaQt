#!/bin/bash
# Temporary diagnostic driver for the private routing proof: runs the routing
# mode under the qq-private environment with frontend debug logging enabled.
set -uo pipefail
BIN="$QQ_BUILD_ROOT/tests/services/portal/qindaqt_portal_frontend_routing_tests"
export G_MESSAGES_DEBUG=all
echo "== routing mode"
"$BIN" routing
status=$?
echo "== routing exit $status"
exit $status
