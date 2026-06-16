# V1.6.35 AI Context Update

Current version: V1.6.35. The user uploaded native macOS Store-V2 component files. Inspection showed `store.db` and `.store.db` have zero in-header dictionary pointers, while adjacent `dbStr-*` map files contain property/category dictionaries. V1.6.35 changes external dbStr loading from iOS-path gated to component-driven for macOS Store-V2. Continue to validate using the same zipped macOS Spotlight thin test and GUI current-page export.

# V1.6.35 AI Context Update

Current version: V1.6.35. The immediate focus is macOS Store-V2 GUI path/name quality. Uploaded GUI page evidence showed most rows as `------NONAME------`; V1.6.35 adds enrichment promotion of native path probe candidates into artifact display/path fields with explicit provenance. Continue to avoid claiming file existence without active filesystem inventory.

# AI Context - 1.6.35

Current package: V1_6_35. V1.6.35 was created after reviewing a V1.6.32 macOS zipped Spotlight thin run. Evidence showed correct macOS Store-V2 persistence and complete success, but a no-op parent-inode path apply phase took several minutes despite `new_reconstructed_paths=0`. V1.6.35 skips that no-op UPDATE and logs explicit skipped status.

# AI Context - 1.6.35

Current package: V1_6_35. This version specifically fixes recurring build-blocking release-readiness failures by limiting fatal preflight to build-safety checks and making release-readiness advisory during build. Post-build CLI version checking now reads the expected version dynamically from `VERSION`. Continue to require uploaded build logs before claiming Windows/MSVC build success.

# AI Context - Vestigant Spotlight V1.6.35

Current baseline: V1.6.35.

V1.6.29.3 failed MSVC compile in `aff4_probe_worker.cpp` because OMAP vertical-cycle handling called `appendProbeNote`, which is not available in that context. V1.6.35 changes those calls to `aff4ApfsAppendProbeNote`. The build wrapper also now fails before version probing if the CLI executable is missing or if compiler/linker errors appear in the build log.

Next validation: build V1.6.35 on Windows/MSVC, upload `V1_6_35_build.log`, then run/upload the V1.6.35 iOS CoreSpotlight thin.

## V1.6.35 context

V1.6.35 was prepared after V1.6.29.4 build/thin validation. It implements the feasible follow-on review items: lock all checked-artifact set access paths, avoid synchronous review-thread joins from the UI refresh path, avoid unbounded export-thread vector accumulation by detaching registered workers, preserve embedded NUL/control bytes in CSV via length-aware export, and reject suspicious APFS NXSB block sizes before traversal/allocation use. Missing-from-FFS and CoreDuet guardrails remain unchanged.

## V1.6.35 context

V1.6.35 addresses the macOS folder Spotlight run where source intake correctly registered `E:/test second/Spotlight/.Spotlight-V100` as macOS/folder Spotlight and found Store-V2 candidates, but the native parse status incorrectly reported iOS CoreSpotlight compact persistence. Native persistence selection is now profile-aware. macOS profile maps to `MacOSStoreV2`, iOS profile maps to `IosCoreSpotlightCompact`, and Auto remains path-sensitive. Native parser progress is mirrored to root progress files.
