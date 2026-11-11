# Roadmap — dashboard bridge support

Owned by: the GNUS Dashboard's `.planning/REQUIREMENTS.md` dependency table.

## Phase D1 — expose what the node already has (this phase)

C API additions in `src/GeniusSDK.h` / `src/GeniusSDK.cpp`, each wrapping an
existing `GeniusNode` method. JSON out via rapidjson (already linked).

- [ ] `GeniusSDKGetMyTaskIds(limit, offset)` → JSON array of task ids
      (SDK-JOBID, SDK-STATUS data; node: `GetMyTaskIds`)
- [ ] `GeniusSDKGetTaskResult(task_id)` → JSON task result
      (SDK-RESULT; node: `GetTaskResult`)
- [ ] `GeniusSDKGetPayoutAddress()` → the node's dev payout address
      (SDK-PAYOUT; node: `DEV_CONFIG.Addr`)

## Phase D2 — earnings aggregation (planned)

`GeniusSDKGetEarningsHistory` derived from in/out transactions and task
results (SDK-EARNINGS). Needs the bridge's shape: {date, amount, jobId}.

## Phase D3 — cost engine (planned, needs node-side investigation)

SDK-CA (live cost returns 0) and SDK-COSTERR (distinguishable cost errors).
`GeniusNode::GetProcessCost` exists; why the live value is 0 is the open
question. Non-blocking cost/process = SDK-ASYNC, same investigation.
