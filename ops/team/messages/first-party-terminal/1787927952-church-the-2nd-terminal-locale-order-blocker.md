# Church the 2nd — Terminal S0 material finding: locale precedence still depends on envp order

- Time: 2026-08-28T08:35:12-06:00
- Owner: Church the 2nd
- Addressees: Micah Stone; Program Manager
- Exact candidate: `2386e7464bcebe17dd074299ac20f1739a5bf8b1`
- Severity: P2; exact repair descendant and rereview required

The candidate still does not implement its documented libc precedence
contract. In `src/apps/terminal/session/terminal_launch_policy.cpp:207-224`,
`effectiveAuthority` is assigned only while it is `None`, so the first
locale-named entry encountered in the arbitrary input list wins. The inner
loop's `LC_ALL, LC_CTYPE, LANG` order does not repair that: it only identifies
the current entry. Libc precedence is based on variable name, not envp order.

Concrete pure-policy reproduction: call `childEnvironment()` with
`{"LANG=en_US.UTF-8", "LC_ALL=C"}`. The loop first records `Lang` as a UTF-8
authority, never upgrades it when `LC_ALL` appears, and performs no locale
replacement at `terminal_launch_policy.cpp:246-257`. The returned envp retains
`LC_ALL=C`; libc therefore selects non-UTF-8 despite the API/wiki guarantee.
The same failure exists for `LANG` before hostile `LC_CTYPE`, and `LC_CTYPE`
before hostile `LC_ALL`.

The test table at `tests/apps/terminal/tst_launch_policy.cpp:231-314` puts the
governing authority first in every multi-variable case, so it is non-vacuous
for value replacement but blind to entry-order independence. Repair should
collect first occurrences per locale variable, then choose the highest present
variable after scanning (or explicitly promote authority when a higher-priority
variable appears), and add reversed/permuted-order tests proving the effective
locale. Duplicate first-envp semantics for each individual variable must stay
deterministic.

I am continuing the rest of the source/test/docs rereview. No product/Git edit,
compiler, PTY, GUI, session, or host-state interaction occurred.
