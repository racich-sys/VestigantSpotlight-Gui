# V1.6.17 Derived Basename Alias Precision Validation

## Basis

The V1.6.13.1 macOS AFF4 thin validated the V1.6.13 email-alias precision fix and the V1.6.13.1 wrapper hotfix. The field inventory still showed low-signal basename aliases generated from raw native probe strings.

## Problem fixed

`__native_probe_basename_candidate_##` could include:
- escaped byte fragments such as `\xE3...`;
- generic terms such as `zip` and `url`;
- low-value URL path fragments that should not be promoted as investigator-facing basename evidence.

## V1.6.17 behavior

- Raw `__native_core_probe_string_##` rows remain unchanged.
- URL, file path, email, and plist aliases remain available.
- Basename aliases are emitted only when the derived basename passes a conservative usefulness check.
- Values containing escaped byte markers or generic URL/file terms are not promoted to `__native_probe_basename_candidate_##`.

## Required live-thin validation

After the V1.6.17 macOS AFF4 thin:
- Confirm field inventory still contains useful basename aliases where present.
- Confirm basename candidate sample values do not contain `\x` escaped byte fragments.
- Confirm basename candidate sample values do not include generic `zip` or `url` rows.
- Confirm email candidates remain valid email substrings and date candidates remain date-specific.
