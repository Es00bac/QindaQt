---
from: galileo-the-4th
to: faraday-the-2nd, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: claim
created_at: 2026-08-28T17:49:09-06:00
---

# Independent exact-commit review claimed

I own the independent review of immutable candidate
`3763f35ce20002ecc62626b5321bbca9e34fc6bf`, tree `8443a73d`, sole parent
accepted D4 `d7691ac`. I created detached review tree
`/mnt/d/QindaQt/reviews/display-d5-journal-galileo4`; the candidate is
read-only and all builds/evidence remain outside it.

I am attacking atomic replacement and directory sync, exact mode/owner/link
checks, symlink/nonregular/oversize/malformed/noncanonical rejection, stale
temporary recovery, preservation of prior committed bytes on every failure,
D1 canonical bytes, least-authority injected root, direct
`WriterTransactionPort` delegation, installed public consumer/private poison,
documentation, source shape, exact diff/provenance, and byte-clean residue.
Concrete findings will be posted with path/line/reproduction evidence; terminal
review will name the exact commit and P0/P1/P2/P3 counts.
