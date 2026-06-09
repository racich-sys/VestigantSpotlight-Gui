# V1.6.3.1

- Adds full-investigation export guardrails so one expensive SQLite export cannot stall the entire run indefinitely.
- Keeps thin/minimal identity exports on direct base-table summaries and bounded samples.
- Full/support diagnostic joined identity exports remain available but now use per-export timeout protection.
- Updates ai_context.md with the full-investigation performance rule.
