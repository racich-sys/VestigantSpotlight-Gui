# Source Docs/Scripts Review — V1_1_10_1

Reviewed current documentation and PowerShell wrappers after the user's command-block correction request.

## Result

- Current build command block now matches the requested pattern: set location, hash ZIP, remove prior T:\ extraction, expand ZIP to T:\, and run the versioned build wrapper.
- Current AFF4/APFS thin command now uses the versioned `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut` wrapper.
- New-chat continuation guide now includes the same command blocks.
- No ambiguous files were removed.
