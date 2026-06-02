# Vestigant Spotlight Help

Current version: 0.9.34

Use `docs/CONSOLIDATED_USER_MANUAL.md` as the primary help/manual.  Use `docs/CONSOLIDATED_VERSION_HISTORY.md` for version history and `docs/PROJECT_ROADMAP_AND_CONTINUATION.md` for active roadmap/continuation notes.

## Current iOS CoreSpotlight review start path

1. Open the completed case in the GUI.
2. Go to the iOS Investigation tab.
3. Start with `iOS - Investigator Overview`.
4. Review `iOS - Case Quality Dashboard` for parser limits, diagnostics, and missing/correlation status.
5. Review direct messages and thread summaries before opening broader text/timeline samples.
6. Use parser diagnostics and the limits/suppression report to understand what was decoded, compacted, suppressed, or deferred to diagnostics mode.

## Current package layout

V0_9_34 starts a clean-source packaging pass.  Obsolete per-version root notes, old V0_7 test-command fragments, and older version-specific PowerShell wrappers were removed from the production ZIP.  Historical and roadmap information is consolidated under `docs/`.

## Standard build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_34.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_34" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_34.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_34\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_34_build.log"
& "T:\VestigantSpotlightInv_V0_9_34\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_34\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_34\build-msvc\selftest_out"
```
