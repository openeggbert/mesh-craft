# Bug: AI Assistant produces truncated / invalid XML for complex prompts

> **FIXED.** Archived here 2026-07-19 (moved from the repo root, where it
> had continued to read as an open bug report long after the fix landed).
> All three fixes below are implemented and live:
> - Fix A (raise `max_tokens`): `AiAssistant::maxTokens` defaults to
>   32000, user-adjustable 4096–64000 via a slider in the AI panel
>   (`MeshCraftApplication_UiAi.cpp`).
> - Fix B (detect `stop_reason`): `AiAssistant::wasTruncated()` checks
>   `stop_reason == "max_tokens"`; the UI shows a dedicated message
>   naming the configured `maxTokens` value instead of a raw XML parse
>   error.
> - Fix C (expose `max_tokens` in the UI): the slider mentioned above.
>
> The rest of this document is kept verbatim for historical context only
> — do not treat it as describing current behavior.

## Symptom

When the user sends an ambitious prompt (e.g. "generate a house with garden, fence,
trees, sidewalk, multiple floors"), the AI Assistant panel shows:

```
Validation error: Failed to load XML: /tmp/mc_ai_resp_1.mc3.xml:
Error=XML_ERROR_PARSING ErrorID=15 (0xf) Line number=265
Line content:    <objects>
```

The generated XML file is cut off mid-document — tags like `</objects>` and `</mc3>`
are simply missing at the end.  tinyxml2 reports the error at the `<objects>` opening
tag (line 265 in the example) because that is where the unclosed structure started,
not at the actual truncation point.

## Root cause

### 1. `max_tokens` is capped at 8 192  ← primary cause

**File:** `src/MeshCraft/AiAssistant.cpp`, line 164

```cpp
"\"max_tokens\":8192,"
```

`claude-sonnet-4-6` (and the broader Claude 4.x family) supports up to **64 000**
output tokens when the `output-128k-2025-02-19` beta header is sent.  The current
hard-coded value of 8 192 is the *default* cap, not the model's actual maximum.

A medium-complexity scene XML easily exceeds 8 192 tokens.  The model is generating
valid XML right up to the moment the token budget runs out; it simply never gets to
write the closing tags.

### 2. `stop_reason` from the API is never checked

**File:** `src/MeshCraft/AiAssistant.cpp`, function `extractFirstTextValue()` (line 51)

The Anthropic Messages API always includes a `stop_reason` field in its JSON response:

| `stop_reason`  | meaning                                    |
|----------------|--------------------------------------------|
| `"end_turn"`   | model finished normally                    |
| `"max_tokens"` | response was cut off by the token limit    |

The current parser extracts only the `"text"` value and ignores everything else.
There is therefore no way for the application to distinguish a complete response from
a truncated one before handing it to tinyxml2.

### 3. `repairXml()` cannot rescue a truncated document

**File:** `src/MeshCraft/MeshCraftApplication_UiAi.cpp`, function `repairXml()` (line 92)

`repairXml()` handles one specific AI mistake: an opening tag whose `>` is missing
before the next element.  A document that ends abruptly without closing tags is a
fundamentally different problem that no character-level scan can fix.

## How to fix

### Fix A — raise `max_tokens` (essential)

In `src/MeshCraft/AiAssistant.cpp`, change:

```cpp
// before
"\"max_tokens\":8192,"
```

```cpp
// after
"\"max_tokens\":32000,"   // or up to 64000 for very large scenes
```

> **Note:** `claude-sonnet-4-6` natively supports up to **64 000** output tokens.
> The `output-128k-2025-02-19` beta header is **not required** — it is built into all
> Claude 4.x models and has no effect if sent.  Simply raising the number is enough.

### Fix B — detect `stop_reason` and show a clear error (important UX fix)

Extend `extractFirstTextValue()` (or add a sibling helper) to also extract
`stop_reason` from the response JSON.  When `stop_reason == "max_tokens"`, throw
(or return) a descriptive error *before* attempting to parse the XML:

```cpp
// Pseudocode
std::string stopReason = extractStopReason(res->body);
if (stopReason == "max_tokens")
    throw std::runtime_error(
        "AI response was cut off by the token limit (max_tokens). "
        "The generated scene was too large. "
        "Try a simpler prompt, use the 'Selection only' scope, or increase max_tokens.");
```

This turns the confusing `XML_ERROR_PARSING` message into an actionable explanation.

### Fix C — expose `max_tokens` in the UI (optional but useful)

Add a numeric input field to the AI Assistant panel so the user can tune the limit
without recompiling.  A reasonable range is 4 096 – 64 000.

## Files to change

| File | Change needed |
|------|--------------|
| `src/MeshCraft/AiAssistant.cpp` | Raise `max_tokens`; add `output-128k-2025-02-19` beta header; parse and return `stop_reason` alongside the text |
| `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Check `stop_reason` after `sendAsync` completes; show friendly error when it is `"max_tokens"` |
| `src/MeshCraft/AiAssistant.hpp` | Expose `stop_reason` (or a `wasTruncated()` bool) on the `AiAssistant` class |

## Additional bug found during analysis

In `src/MeshCraft/MeshCraftApplication_UiAi.cpp`, the `def_zidle` definition
(generated by the AI in the example session) contained:

```xml
<box name="Noha_pp_l" size="0.05 0.46 0.05" position="-0.20 0.23 " .../>
```

The `position` attribute has only two numbers instead of the required three (`vec3Type`).
This is a separate AI hallucination / schema-violation issue, not related to truncation,
but it shows that schema validation errors should also produce a clear diagnostic rather
than a raw tinyxml2 message.
