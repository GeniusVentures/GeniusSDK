# GeniusSDK

C wrapper over the SuperGenius node for embedders (GeniusWallet via FFI, Unity
games via the Unity wrapper). House rules: Ponytail lazy-senior-dev + the GNUS
C++ standards in AGENTS.md.

## Why this planning exists

The GNUS Dashboard (gnus-dashboard-v1) waits on SDK capabilities its UI can't
deliver today. The dependency list lives in the dashboard's
`.planning/REQUIREMENTS.md` ("Dependencies (not dashboard work)"): SDK-CA,
SDK-COSTERR, SDK-JOBID, SDK-STATUS, SDK-RESULT, SDK-EARNINGS, SDK-ASYNC,
SDK-PAYOUT. Investigation 2026-11-10 showed the node already exposes most of
what these need (`GeniusNode::GetMyTaskIds`, `GetTaskResult`,
`GetBalance(address, token)`, `DEV_CONFIG.Addr`) — the C API just doesn't
expose it. The wrapper items are plumbing; the cost-engine items are real.
