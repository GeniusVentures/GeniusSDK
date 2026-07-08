# Phase 1 Discussion Log

**Phase:** 1 — Init Interface Modernization
**Date:** 2026-07-03

## Areas Discussed

### Config wiring
- **Question:** Should the SDK call WriteNetworkConfig/WriteSgnsConfig helpers before GeniusNode::New(), or leave it to the engine?
- **Options presented:** Engine handles it (Recommended) | SDK calls helpers first | Let me explain
- **Decision:** User provides config files. They are NOT generated internally and do NOT use SuperGenius helpers. SDK is a thin adapter.

### Impl structure
- **Question:** Should all 3 init variants flow through a single SDKInitHelper template, or separate code paths?
- **Options presented:** Single template (Recommended) | Separate paths | Let me explain
- **Decision:** User wants simpler than template. Follow-up clarified: inline per function.

### Impl approach (follow-up)
- **Question:** Inline, no helper — each init function does ParseDevConfig + GeniusNode::New directly?
- **Options presented:** Inline, no helper (Recommended) | Keep template | Let me explain
- **Decision:** Inline, no helper. ~5 lines per function, zero indirection.

### Error handling
- **Question:** When dev_config JSON string is null or empty, what should happen?
- **Options presented:** Return null immediately (Recommended) | Fallback to file | Let me explain
- **Decision:** Return null immediately with SPDLOG_ERROR. No fallback.

## Locked (carried from project setup)

- 3 init functions: Init, InitWithKey, InitWithMnemonic
- base_path + dev_config JSON string as params
- Remove InitWithKeyAndDevConfig and InitMinimal
- Lambda capture-by-value for key/mnemonic
- No SDK auto-write of JSON configs; callers ship own config files
- Return contract preserved (static ret_val pattern stays for now)

## Deferred Ideas

None raised during discussion.

---

*Discussion log — human reference only. Not consumed by downstream agents.*
