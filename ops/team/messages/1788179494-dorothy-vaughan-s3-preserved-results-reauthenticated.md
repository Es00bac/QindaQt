# S3 preserved result roots remain byte-exact while executable lane is withheld

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T06:31:34-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: preserved evidence reauthenticated; new executable gates not started

Read-only revalidation confirms all four preserved result roots remain present,
sentinel-authenticated, and byte-exact:

| Row/result | Outcome | Result JSON | Evidence or decisive diagnostic | Capture |
| --- | --- | --- | --- | --- |
| WUXGA `d2e82f3d14d4787ce286caecd35ebdea` | success, return 0, not timed out | `f0f3c27bb4684e620998e14eac40e77638ff9b08648e057d1862cb8785251604` | `50f09adf54612c52c207ef99e2c9ede9b76d7fdba1f01095360bc4be8fcf3cc8` | `5b89909603a3b7c9bfe8cf9c0c9301b3e3087ee54d5c9293a95c7b57d5f9087d` |
| 1440p/125 `dd22c6143ad53529a76639190ae4a6f5` | success, return 0, not timed out | `e8a60abc489ebfb6d665fe7050de82d0764b75f87ac85ce56d27ffedb99df12a` | `831b43819ddf20cc05e0d557b6a5c81b00e6d59d3159e3c3301153862aa30a43` | `ed3d912351050a948f1b54555e0660ec7468f92a42be5b55cb8148d0dc9c20dc` |
| 1080p/150 `1b30bfe610d96520125119ffaefdaf8b` | success, return 0, not timed out | `dddf8f30dca9e6cc0a6d0ef8dca7cba1fbb1481b82a3b5b7568e24ddca9c1d5d` | `808c5f15534417933d4e33fd5e88c70b1b08ac27cca03135d7c19a1aa7ab2e70` | `c1bfffc20261f53e77cd91f94975cff02dc30d732ab004d953721e531700d0d4` |
| dual diagnostic `a547453211a4477f93363522c7583d4b` | expected failure, return 1, not timed out | `4be077de8f0d40a43f25434afa96cccd808b5319a2a24ba5e18359ab43497dd6` | interaction failure `3170cb494cc966d2b92ae14508534bb186282172bab3c71a2d97f6bec709c199` | none before repaired route |

The run IDs embedded in each `result.json` match the directory, the successful
rows retain their complete evidence and PNG artifacts, and the dual diagnostic
retains the exact post-injection compositor/shell inventory that isolated the
stale WL-0 product route. This is provenance evidence only. Root still owns the
serialized Portal manager gates; no S3 build, CTest, package mutation, bus, or
nested compositor was started.
