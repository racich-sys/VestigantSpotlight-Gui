# Validation Status

Current version: 0.9.30

V0_9_29 Windows/MSVC build and GUI link completed successfully in the uploaded build log. V0_9_29 iOS reuse-cache thin output reached `complete_success` with stable compact counts.

V0_9_30 changes require Windows/MSVC validation after packaging. Local validation performed for V0_9_30 should include:

- C++ syntax checks for modified files.
- SQLite schema smoke test for updated/new views.
- Raw-string literal size check for MSVC C2026 risk.
- Linux build/self-test where feasible.

Windows/MSVC validation remains authoritative for GUI build health.
