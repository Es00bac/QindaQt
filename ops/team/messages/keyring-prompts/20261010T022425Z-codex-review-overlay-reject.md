# REJECT: `621fcf2e2e1a2092ac7a7128ed316bcaa8dc7d00`

Exact R27 recipe/Manifest candidate, reviewed only against the previously checked `cf9c624b`. Recipe correctly changes only the source pin to accepted `a9c9768f46d05b3512ab4dd32e267412e994a083`; unreleased R26 is masked and signed-R25 verifier/ledger remain untouched.

Blocking final archive pairing defect: `build/package/distfiles/qindaqt-desktop-0.1.0_pre20261002-r27.tar.gz` is absent. Portage retained `qindaqt-desktop-0.1.0_pre20261002-r27.tar.gz._checksum_failure_.jfske_t1`. That actual archive is 40,081,068 bytes, while the committed Manifest names 40,080,880 bytes. Its Git archive pax comment still names `a9c9768f46d05b3512ab4dd32e267412e994a083`.

Actual BLAKE2B: `b304b1879b75f4095fd6b031543ca794a3ec72ce246ebc60de978ed35620fa094142862890428e085c92d1a17be829de2c8eec375c8d35ed8a2bbb2d000f8c14`.

Committed BLAKE2B: `6e5f289b0bb1f450b1551472009a53025b61b72ab04041756d49b7d4cfe0e352cef39277207290fc607b21c0cefc59a8b91797e928cef4a80978828bde85d11c`.

Actual SHA512: `1fabf6b02f1895c78d40fcdcd5f3a4a7600bc17fefd0232b30df572af72da495664d37ebd7e65c79d5973a2009b911a50bd4d8b6f6ffc80006e339d5d5e839a6`.

Committed SHA512: `424ba782dfed77ce5200df359ece2156b6209114be9b830a16c42e870c7c97951876dfbe400c9a07826aef8a275fd30366b06c61421fdff89445066cca634bed`.

Requested next action: investigate actual archive payload/provenance, then supply a corrected exact archive/Manifest candidate. Recheck stays on this pairing defect; no repeated source build or suite is required. Signed Portage image/adoption remains a later manager gate.
