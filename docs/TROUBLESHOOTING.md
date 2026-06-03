# Troubleshooting

Current version: 0.9.46

## Build fails before CLI/GUI link

Upload the build log.  Recent MSVC-specific failure classes included oversized raw string literals (`C2026`) and GUI SQL helper scope mistakes.  V0_9_37 keeps raw-string static checks in validation and continues consolidating SQL/view ownership.

## Run stalls or stops writing

Collect state before rerunning:

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_46\scripts\Collect-V0_9_46-DBBloat-State.ps1" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_46_ReusedCache" `
  -OutZip "D:\Downloads\Upload_State_V0_9_37_NoWrites_Stopped_Check.zip" `
  -StopVestigant
```

## Even-looking counts

Check `exports/parser_limits_and_suppression_summary.csv`.  Native record count should be unlimited unless a max-record option is explicitly set.  Compact key/value and date counts are expected in normal iOS investigator mode.
