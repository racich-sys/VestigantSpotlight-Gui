# Vestigant Spotlight Version History

Current version: 0.9.44

## V0_9_44 - Fresh ZIP FFS inventory recovery and native parser efficiency

Reviewed V0_9_43 build/thin outputs. Build succeeded and Stage B fresh-ZIP reached `complete_success`, but fresh-ZIP FFS inventory was empty (`files=0 app_databases=0 raw_records=0`) even though ZIP entry probe rows and CoreSpotlight stores existed. V0_9_44 fixes the 7-Zip raw-listing handoff/parsing path and adds fallback decoding for UTF-16 raw listings created by older PowerShell redirection behavior. It also adds zero-copy bounded metadata item parsing and text cleanup/probe improvements.

The maintained full history is `docs/CONSOLIDATED_VERSION_HISTORY.md`.
