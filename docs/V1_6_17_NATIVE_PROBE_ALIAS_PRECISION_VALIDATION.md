# V1.6.17 Native Probe Alias Precision Validation

## Basis

The V1.6.12 macOS AFF4 thin reached `complete_aff4_apfs_staged_storev2_validation_probe` and validated the parent-inode path metric split. The remaining issue observed in the V1.6.12 field inventory sample was overbroad derived semantic aliasing from raw Store-V2 probe strings.

## Problem fixed

V1.6.12 could emit `__native_probe_email_candidate_##` rows that preserved a whole HTML, JavaScript, or text probe when the probe merely contained an at-sign. This made the semantic alias appear stronger than the underlying evidence supported. URL and file-path candidate extraction could also overrun into an adjacent concatenated `http://`, `https://`, or `file://` scheme.

## V1.6.17 behavior

- Raw `__native_core_probe_string_##` values remain evidence-preserving rows.
- `__native_probe_email_candidate_##` now stores only a validated email substring.
- Email validation requires a single at-sign, a dotted domain, and an alphabetic final domain label.
- URL and file-path extraction stops when a second adjacent URL/file scheme starts.
- Non-email code comments, annotation tags, and IDs such as `@thread.v2` are not promoted to email aliases.

## Validation required on live thin

After the V1.6.17 macOS AFF4 thin:
- Confirm `aff4_apfs_staged_storev2_field_inventory_sample.csv` still contains semantic URL/path/email aliases where present.
- Confirm `__native_probe_email_candidate_##` sample values are actual email substrings, not whole source snippets.
- Confirm `raw_date_candidates` remains focused on actual date fields such as `Last_Updated`.
