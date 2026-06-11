# Vestigant Spotlight Current User Note — V1.6.6.6

Current source package: `VestigantSpotlightInv_V1_6_6_6.zip`.

Use `BUILD_INSTRUCTIONS.md` or `docs/QUICK_START.md` for the current build and thin-test commands.

Key current release note:

- V1.6.6.6 reviewed the uploaded V1.6.6.5 iOS thin result and verified queued forensic-directive claims against source before modifying code.
- The V1.6.6.5 thin run reached `complete_success`; no slow or incomplete exports were reported above the thin-performance threshold.
- APFS guided traversal cycle detection, bounded bplist/NSKeyedArchiver recovery, and `tel:` / `mailto:` identity fallback were already present in the V1.6.6.5 baseline.
- V1.6.6.6 adds the Spotlight/native-DB communication mismatch view to the GUI bootstrap schema and prioritizes that view in the iOS GUI sort order.

Detailed note: `docs/V1_6_6_6_THIN_AND_FORENSIC_DIRECTIVE_REVIEW.md`.
