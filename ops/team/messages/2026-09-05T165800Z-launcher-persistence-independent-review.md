# Independent launcher persistence review — ACCEPT

Exact candidate reviewed: `7fc7cf02739b1163c86dc4d3fe517af9b7369b3f` in codex/audit-launcher-persistence. Candidate production/test source is unchanged from the commit; the sole extra untracked file is its timestamped handoff.

No blocking findings. Shipped schema v2 now admits panels.launcherPinned and panels.launcherRecent with empty string-list defaults; controller keys use those same names. Canonical JSON normalization occurs after schema validation and converts QStringList into the encoder's accepted list representation; empty and populated values round-trip back to schema QStringList with order preserved. Existing settings documents retain unrelated values, and no supported old-key values existed to migrate.

The real private-bus test writes both pins and recents through the production persistence controller, verifies disk contents, replaces the service with a genuinely different unique owner and epoch, and loads values into a fresh empty client/controller. This tests restart correctness without relying on surviving in-memory launcher state. Existing client owner/epoch and commit-result regression checks were not edited or relaxed. The fixture correctly changed owners instead of weakening those checks.

Independently reran the supplied 18 affected Settings/launcher CTests with private-bus permission: exit 0, 18/18 passed. Log /tmp/launcher-persistence-independent-review.log. git diff --check passed; exact HEAD reconfirmed after tests. Requested next action: integrate the exact candidate and qualify shared shell settings/launcher state in the manager's ongoing desktop session checks.

User steering applies: this review evaluates actual persistence, stale-state and restart correctness; it adds no security infrastructure or new security requirement.
