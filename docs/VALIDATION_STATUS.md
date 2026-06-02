# Validation Status

Current version: 0.9.34

V0_9_34 requires Windows/MSVC validation after packaging.  Local validation performed during packaging includes source-tree cleanup inventory, syntax/static checks where available, raw-string risk checks, ZIP integrity checks, and Linux build/self-test attempt where feasible.

Expected Windows validation:

1. Build with `build_windows_msvc.bat`.
2. Confirm CLI version reports `Vestigant Spotlight v0.9.34`.
3. Run `VestigantSpotlightTests.exe`.
4. Run the standard iOS reuse-cache script and upload the thin bundle.
