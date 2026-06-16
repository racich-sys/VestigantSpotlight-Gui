# V1.6.35 current handoff note

V1.6.35 follows successful V1.6.35 build/thin validation and implements code-review hardening for APFS/AFF4/bplist/iOS app DB/GUI path handling. See `docs/V1_6_35_CODE_REVIEW_VALIDATION_HARDENING.md`. Missing-from-FFS and CoreDuet interpretation guardrails remain in place.

# Guardrail Retirement Plan - V1.6.35

V1.6.35 does not remove the lead-only Missing-from-FFS guardrail. It improves validation fidelity by carrying lookup-source provenance into the reference and candidate outputs. Further guardrail retirement requires a thin/support run where all active-comparison validation checks pass.

# Guardrail Retirement Plan - V1.6.35

The next removable guardrail is limited to validation gating, not interpretation. Missing-from-FFS candidate generation is enabled for iOS FFS reference lookup, but the deletion-proof guardrail remains. V1.6.35 adds validation checks to determine whether reference-candidate and interactionC outputs are stable enough to widen support/full profiles.

Do not remove:

- lead-only language for Missing-from-FFS candidates;
- CoreDuet contextual-evidence wording;
- suppression of generic numeric identity-to-phone promotion;
- AFF4/APFS active comparison guardrails until image-backed validation exists.

# Guardrail Retirement Plan - V1.6.35

V1.6.35 does not retire additional interpretation guardrails. It adds validation checks required before further guardrail retirement.

Still retained:

- No fuzzy active filesystem matching.
- No deletion conclusion from missing Spotlight paths.
- AFF4/APFS image-backed inode/parent comparison remains pending.
- Direct `--evidence-root` comparison remains pending.
