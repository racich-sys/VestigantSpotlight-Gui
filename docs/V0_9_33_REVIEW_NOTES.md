# V0_9_33 Review Notes

Inputs reviewed:

- V0_9_32 Windows build log.
- V0_9_32 iOS reuse-cache thin upload.
- Separate user instruction requesting a detailed roadmap, testing-source transition timeline, and AFF4/APFS work plan.

Findings:

- Windows/MSVC build completed successfully for GUI, CLI, and self-test binaries.
- The iOS reuse-cache run reached `complete_success`.
- Stable compact-mode counts continued: 344,445 raw records, 982,230 compact raw key/value rows, and 336,037 compact date candidates.
- FFS full inventory and app DB parsed records remained suppressed in normal mode.
- Reuse-cache imported 1,592,440 slim FFS lookup rows from the known cache.
- The largest review exports remain the row-level message/body/direct-message samples. These are bounded samples, but future versions should add smaller investigative dashboards and GUI-first filtering so users do not need to open the largest CSVs first.

Decision:

V0_9_33 focuses on documentation/roadmap consolidation and next-step planning rather than risky parser changes, because V0_9_32 is stable and the user specifically requested a forward roadmap and testing timeline. The next feature version should resume iOS investigator-view improvements and should consider shrinking/targeting oversized sample exports.
