# Power Settings midpoint — Hilda Geiringer

- Feature: QQ-006.05 Power Settings route.
- Exact base: `347d32f92b32c57edd4fe055c26aab8a8f27298e`.
- State: implementation and Debug verification are complete. The focused Power selector passes 6/6 and the additive Settings Center selector passes 9/9 under absent session/system buses.
- Material packaging finding: keeping the page module dynamically loadable is necessary for a truthful relocation poison. The route now separates its statically linked public-client composition singleton from the installed shared QML page module; withholding the installed page module fails closed, while the restored relocated package remains resident.
- Next gate: Release focused build/tests and all static documentation/source-shape/diff checks.
