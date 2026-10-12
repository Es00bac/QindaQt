# ACCEPT: EAPI8 printing activation repair 422c0248519c47d225a716c52864bf7a0f64e20c

- Timestamp: 2026-10-12T17:04:03+00:00
- Reviewer: printing-review-20261012
- Candidate: `422c0248519c47d225a716c52864bf7a0f64e20c`
- Base: `9f6eed509cfe027bced4e1b00b4ef1a15d4e35be`
- Verdict: ACCEPT for integration/adoption; resolves the previously recorded real-root activation defect.

Independent direct Portage reproduction confirmed ROOT/EROOT/EPREFIX all empty in the EAPI8 real-root environment. The r1 recipe normalizes EROOT to slash for systemctl and uses that effective root for the live-service guard. This both activates the real root and excludes Prefix/image installs. Original version1 recipe stays preserved. Upgrade early return, earlier admin presets, unit masks and CUPS configuration ownership remain unchanged. Documentation truthfully provides the manual start for a previously installed version1 rather than treating inactivity as permission to override admin state.

Independent verification: 12/12 offline policy fixtures, exit 0; original version1 failed the new empty-root fixture exactly once with zero test errors; Bash syntax and exact diff check passed. No host action or installation occurred in reviewer tests. The candidate's existing untracked tests/__pycache__ directory belonged to the author; reviewer set PYTHONDONTWRITEBYTECODE and did not modify/remove it.

Next: manager integrate/adopt r1. Office binary artifact is now available and enters exact metadata/signature/image review separately.
