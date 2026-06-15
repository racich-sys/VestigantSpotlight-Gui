# Known Issues - V1.6.28

- V1.6.28 Windows/MSVC build is not verified until `V1_6_28_build.log` is uploaded.
- Self-test execution is not confirmed unless the log shows the test executable actually ran.
- Missing-from-FFS candidates remain investigative leads only, not deletion proof.
- AFF4/APFS image-backed active comparison remains pending.

# Known Issues - V1.6.28

- Windows/MSVC build must be validated from `V1_6_28_build.log`; this package has only static/local syntax validation in this environment.
- Self-test execution is not confirmed unless the build log shows `VestigantSpotlightTests.exe` actually ran and exited successfully.
- iOS Missing-from-FFS candidates are lead-only. They are not deletion proof.
- AFF4/APFS image-backed inode/parent active comparison remains pending.

# V1.6.28 Known Issues and Validation Limits

- Windows/MSVC build is not verified until `V1_6_28_build.log` is uploaded and reviewed.
- Self-test execution is not confirmed unless the build log shows actual test output.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.
- Exact artifact-path comparison can remain zero when Spotlight artifact rows lack direct comparable iOS paths; V1.6.28 validates recovered-path reference candidates separately.
- AFF4/APFS image-backed inode/parent comparison remains pending.
