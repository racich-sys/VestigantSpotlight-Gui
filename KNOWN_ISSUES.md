# V1.6.35 Known Issues Update

- Windows/MSVC build for V1.6.35 is not verified until `V1_6_35_build.log` is uploaded.
- Full macOS Store-V2 structured value decoding remains under validation; V1.6.35 improves dictionary/category loading but does not claim complete parity with Apple's Spotlight internals.

# V1.6.35 Known Issues Update

- Windows/MSVC build for V1.6.35 is not verified until `V1_6_35_build.log` is uploaded.
- macOS Store-V2 native parser remains in safe core-probe mode; structured property dictionary decoding remains experimental.
- Active filesystem comparison for ZIP/folder Spotlight sources remains NOT_CHECKED unless a validated filesystem inventory is provided.

# Known Issues - 1.6.35

- Windows/MSVC build validation is pending until `V1_6_35_build.log` is uploaded.
- macOS profile still creates some legacy iOS-named export files with zero rows; this is a UI/export hygiene issue and should be separated from parser correctness.
- The external thin wrapper may report a diagnostic blank exit code even when the CLI writes `complete_success`; rely on run_status/last_stage until the wrapper is revised.

# Known Issues - 1.6.35

- Windows/MSVC build validation is pending until the user uploads `V1_6_35_build.log`.
- Release-readiness is now advisory during build; review warnings should still be addressed before final release packaging, but they should not block compilation.

# Known Issues - V1.6.35

- V1.6.35 Windows/MSVC build is not verified until `V1_6_35_build.log` is uploaded and reviewed.
- Missing-from-FFS candidates remain investigative leads only, not deletion proof.
- CoreDuet `interactionC.db` rows remain contextual evidence only.
- AFF4/APFS image-backed active filesystem comparison remains pending.

## V1.6.35 Known limitations

- GUI source changes are statically checked in this package but require Windows/MSVC build validation from `V1_6_35_build.log`.
- Detached stale review-query workers rely on the existing request-sequence/progress-handler cancellation pattern; a future thread-pool design may be cleaner.
- AFF4/APFS image-backed active filesystem comparison remains pending.

## V1.6.35 Known limitations

- V1.6.35 Windows/MSVC build is not verified until `V1_6_35_build.log` is uploaded.
- The macOS folder Spotlight run that triggered this fix stopped during native parse setup in V1.6.30; V1.6.35 should be rerun against the same folder to confirm whether parse completion is restored or whether a deeper native Store-V2 parser issue remains.
