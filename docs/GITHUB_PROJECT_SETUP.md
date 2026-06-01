# GitHub Project Setup

## Goal

Use GitHub for source control, issue/roadmap tracking, and automated build validation without storing forensic evidence in the repository.

## Repository

Recommended repository name:

```text
VestigantSpotlight
```

Use a private repository unless the code is intentionally public.

## Stable local repo

```text
T:\VestigantSpotlight
```

## GitHub Project board

Create a GitHub Project named:

```text
Vestigant Spotlight Roadmap
```

Recommended fields:

- Status: Backlog, Ready, In Progress, Blocked, Needs Windows Validation, Needs GUI Validation, Done
- Area: iOS, CoreSpotlight, App Databases, GUI, AFF4/APFS, Build, Docs, Validation
- Version: V0_8_99, V0_8_99, V0_8_99, Future
- Priority: High, Medium, Low

Recommended labels:

```text
ios
corespotlight
app-db
missing-from-ffs
gui
aff4-apfs
build-failure
msvc
docs
validation
codex
```

## Evidence handling rule

Never upload or commit evidence or generated case outputs.
