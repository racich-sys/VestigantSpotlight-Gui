# V1.6.40.1.1 Current Note

Current baseline is V1.6.40.1.1. Use `docs/START_CONTINUATION_CHAT.md` for handoff and `docs/IOS_INVESTIGATION_VALIDATION_WORKFLOW.md` for iOS validation. Guardrail retirement is tracked in `docs/GUARDRAIL_RETIREMENT_PLAN.md`.

# Quick Start - V1.6.40.1.1

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_40_1.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_40_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_40_1.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_40_1\scripts\Build-V1_6_40_1.ps1 -CleanExtract
```
