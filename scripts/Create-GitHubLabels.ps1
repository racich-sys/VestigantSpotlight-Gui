Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Repo = "racich-sys/VestigantSpotlight"
$Labels = @(
  @{Name="ios"; Color="0E8A16"; Description="iOS investigation workflow"},
  @{Name="corespotlight"; Color="1D76DB"; Description="CoreSpotlight parser/decoder"},
  @{Name="app-db"; Color="5319E7"; Description="iOS local app database parsing/correlation"},
  @{Name="missing-from-ffs"; Color="FBCA04"; Description="Spotlight to FFS/app DB residency correlation"},
  @{Name="gui"; Color="C2E0C6"; Description="GUI views and workflow"},
  @{Name="aff4-apfs"; Color="D93F0B"; Description="AFF4/APFS image support"},
  @{Name="build-failure"; Color="B60205"; Description="Build or linker failure"},
  @{Name="msvc"; Color="E99695"; Description="Windows/MSVC build"},
  @{Name="docs"; Color="0075CA"; Description="Documentation/release notes"},
  @{Name="validation"; Color="7057FF"; Description="Validation upload or test review"},
  @{Name="codex"; Color="BFDADC"; Description="Codex-assisted branch/task"}
)
foreach ($Label in $Labels) { gh label create $Label.Name --repo $Repo --color $Label.Color --description $Label.Description --force }
