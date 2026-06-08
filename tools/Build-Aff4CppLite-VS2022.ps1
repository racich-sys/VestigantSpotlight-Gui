param(
    [string]$SourceZip = "D:\Downloads\aff4-cpp-lite-master.zip",
    [string]$SourceRoot = "T:\aff4-cpp-lite-master",
    [string]$OutputRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
    [string]$LogPath = "Q:\SpotlightCase\aff4_cpp_lite_build_log.txt",
    [string]$WindowsTargetPlatformVersion = "",
    [string]$PlatformToolset = "v143",
    [string]$NuGetExe = "",
    [switch]$NoRetarget,
    [switch]$SkipNuGetRestore,
    [switch]$NoNuGetDownload,
    [switch]$BuildEntireSolution
)

$ErrorActionPreference = "Stop"

function Write-LogLine {
    param([Parameter(Mandatory=$true)][AllowEmptyString()][string]$Message)

    # Do not write log messages to the PowerShell success output pipeline.
    # Path-returning helpers in this script are assigned to variables; success-pipeline
    # logging can contaminate those variables and cause PowerShell to execute a
    # malformed command string.
    Write-Host $Message
    if ($script:LogPath) {
        $parent = Split-Path -Parent $script:LogPath
        if ($parent) {
            New-Item -ItemType Directory -Force -Path $parent | Out-Null
        }
        Add-Content -LiteralPath $script:LogPath -Value $Message
    }
}

function Write-LogObject {
    param([Parameter(Mandatory=$true)]$Object)

    $text = ($Object | Out-String).TrimEnd()
    if ($text.Length -gt 0) {
        foreach ($line in ($text -split "`r?`n")) {
            Write-LogLine $line
        }
    }
}

function Assert-CleanPathString {
    param(
        [Parameter(Mandatory=$true)]$PathValue,
        [Parameter(Mandatory=$true)][string]$Description,
        [string]$RequiredLeafName = "",
        [switch]$MustExist
    )

    if ($null -eq $PathValue) {
        throw "$Description path is null."
    }

    if ($PathValue -is [array]) {
        $joined = ($PathValue | ForEach-Object { [string]$_ }) -join "<NL>"
        throw "$Description path is malformed because multiple success-output values were returned: $joined"
    }

    $path = [string]$PathValue
    if ([string]::IsNullOrWhiteSpace($path)) {
        throw "$Description path is blank."
    }

    if ($path.Contains("`n") -or $path.Contains("`r")) {
        $shown = $path.Replace("`r", "<CR>").Replace("`n", "<LF>")
        throw "$Description path is malformed because it contains a newline/carriage return: $shown"
    }

    if ($RequiredLeafName) {
        $leaf = Split-Path -Leaf $path
        if ($leaf -ine $RequiredLeafName) {
            throw "$Description path must end in $RequiredLeafName. Malformed value: $path"
        }
    }

    if ($MustExist -and !(Test-Path -LiteralPath $path)) {
        throw "$Description path does not exist: $path"
    }

    return $path
}

function Assert-CleanNuGetPath {
    param([Parameter(Mandatory=$true)]$PathValue)
    return (Assert-CleanPathString -PathValue $PathValue -Description "NuGet executable" -RequiredLeafName "nuget.exe" -MustExist)
}

function Assert-CleanMSBuildPath {
    param([Parameter(Mandatory=$true)]$PathValue)
    return (Assert-CleanPathString -PathValue $PathValue -Description "MSBuild executable" -RequiredLeafName "MSBuild.exe" -MustExist)
}

function Invoke-LoggedExternalCommand {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$Arguments,
        [Parameter(Mandatory=$true)][string]$Description
    )

    $cleanFilePath = Assert-CleanPathString -PathValue $FilePath -Description $Description -MustExist
    Write-LogLine "$Description command: $cleanFilePath $($Arguments -join ' ')"

    $output = & $cleanFilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE

    if ($null -ne $output) {
        foreach ($line in $output) {
            Write-LogLine ([string]$line)
        }
    }

    if ($exitCode -ne 0) {
        throw "$Description failed with exit code $exitCode. See log: $LogPath"
    }
}

function Resolve-MSBuild {
    $candidates = @()
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
        if ($install) {
            $candidates += (Join-Path $install "MSBuild\Current\Bin\MSBuild.exe")
            $candidates += (Join-Path $install "MSBuild\15.0\Bin\MSBuild.exe")
        }
    }

    $candidates += @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Assert-CleanMSBuildPath -PathValue (Resolve-Path -LiteralPath $candidate).Path)
        }
    }

    throw "MSBuild.exe not found. Install Visual Studio Build Tools/Desktop C++ workload or run from a Visual Studio Developer PowerShell."
}

function Resolve-NuGet {
    param(
        [string]$RequestedNuGetExe,
        [Parameter(Mandatory=$true)][string]$OutputRootForTools,
        [switch]$NoDownload
    )

    $candidates = @()
    if (![string]::IsNullOrWhiteSpace($RequestedNuGetExe)) { $candidates += $RequestedNuGetExe }
    if (![string]::IsNullOrWhiteSpace($env:NUGET_EXE)) { $candidates += $env:NUGET_EXE }

    $cmd = Get-Command nuget.exe -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { $candidates += $cmd.Source }

    $candidates += @(
        (Join-Path $PSScriptRoot "nuget.exe"),
        (Join-Path $OutputRootForTools "_tools\nuget.exe"),
        "C:\Program Files (x86)\NuGet\nuget.exe"
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace([string]$candidate)) { continue }
        if (Test-Path -LiteralPath $candidate) {
            return (Assert-CleanNuGetPath -PathValue (Resolve-Path -LiteralPath $candidate).Path)
        }
    }

    if ($NoDownload) {
        throw "nuget.exe not found and -NoNuGetDownload was specified. Pass -NuGetExe <path> or place nuget.exe beside this script."
    }

    $toolDir = Join-Path $OutputRootForTools "_tools"
    New-Item -ItemType Directory -Force -Path $toolDir | Out-Null

    $downloadPath = Join-Path $toolDir "nuget.exe"
    $downloadPath = Assert-CleanPathString -PathValue $downloadPath -Description "NuGet download target" -RequiredLeafName "nuget.exe"

    Write-LogLine "nuget.exe not found locally. Downloading NuGet command line client to: $downloadPath"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" -OutFile $downloadPath -UseBasicParsing

    return (Assert-CleanNuGetPath -PathValue $downloadPath)
}

function Resolve-WindowsSDKVersion {
    $sdkRootCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Include"),
        "C:\Program Files (x86)\Windows Kits\10\Include"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique

    foreach ($sdkRoot in $sdkRootCandidates) {
        $versions = Get-ChildItem -LiteralPath $sdkRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -match '^10\.\d+\.\d+\.\d+$' -and
                (Test-Path -LiteralPath (Join-Path $_.FullName "um")) -and
                (Test-Path -LiteralPath (Join-Path $_.FullName "shared"))
            } |
            Sort-Object Name -Descending

        if ($versions -and $versions.Count -gt 0) {
            return $versions[0].Name
        }
    }

    throw "No Windows 10/11 SDK include folder was found under C:\Program Files (x86)\Windows Kits\10\Include. Install a Windows 10/11 SDK or pass -NoRetarget and install Windows SDK 8.1."
}

function Retarget-Aff4CppLiteProjects {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$SdkVersion,
        [Parameter(Mandatory=$true)][string]$Toolset
    )

    $projectFiles = Get-ChildItem -LiteralPath (Join-Path $Root "win32") -Recurse -Include "*.vcxproj","*.props" -File -ErrorAction Stop
    $changed = 0

    foreach ($file in $projectFiles) {
        $original = Get-Content -LiteralPath $file.FullName -Raw
        $updated = $original

        if ($updated -match '<WindowsTargetPlatformVersion>.*?</WindowsTargetPlatformVersion>') {
            $updated = [regex]::Replace($updated, '<WindowsTargetPlatformVersion>.*?</WindowsTargetPlatformVersion>', "<WindowsTargetPlatformVersion>$SdkVersion</WindowsTargetPlatformVersion>")
        } else {
            $updated = $updated -replace '(<PropertyGroup\s+Label="Configuration"[^>]*>\s*)', "`$1`r`n    <WindowsTargetPlatformVersion>$SdkVersion</WindowsTargetPlatformVersion>`r`n"
        }

        if ($Toolset) {
            if ($updated -match '<PlatformToolset>.*?</PlatformToolset>') {
                $updated = [regex]::Replace($updated, '<PlatformToolset>.*?</PlatformToolset>', "<PlatformToolset>$Toolset</PlatformToolset>")
            }
        }

        if ($updated -ne $original) {
            Set-Content -LiteralPath $file.FullName -Value $updated -Encoding UTF8
            $changed++
        }
    }

    Write-LogLine "Retargeted aff4-cpp-lite projects: files_changed=$changed sdk=$SdkVersion toolset=$Toolset"
}

function Restore-Aff4CppLiteNuGetPackages {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$Solution,
        [Parameter(Mandatory=$true)][string]$OutputRootForTools,
        [string]$RequestedNuGetExe,
        [switch]$NoDownload
    )

    $nuget = Resolve-NuGet -RequestedNuGetExe $RequestedNuGetExe -OutputRootForTools $OutputRootForTools -NoDownload:$NoDownload
    $nuget = Assert-CleanNuGetPath -PathValue $nuget

    $packagesDir = Join-Path (Join-Path $Root "win32") "packages"
    New-Item -ItemType Directory -Force -Path $packagesDir | Out-Null

    Write-LogLine "Using NuGet: $nuget"
    Write-LogLine "NuGet packages directory: $packagesDir"

    $sources = "https://api.nuget.org/v3/index.json"
    $solutionRestoreArgs = @(
        "restore", $Solution,
        "-PackagesDirectory", $packagesDir,
        "-NonInteractive",
        "-Verbosity", "normal",
        "-Source", $sources
    )

    Write-LogLine "NuGet restore args: $($solutionRestoreArgs -join ' ')"
    Invoke-LoggedExternalCommand -FilePath $nuget -Arguments $solutionRestoreArgs -Description "NuGet restore"

    $packageConfigs = Get-ChildItem -LiteralPath (Join-Path $Root "win32") -Recurse -Filter "packages.config" -File -ErrorAction SilentlyContinue
    foreach ($config in $packageConfigs) {
        Write-LogLine "NuGet install packages.config: $($config.FullName)"
        $installArgs = @(
            "install", $config.FullName,
            "-OutputDirectory", $packagesDir,
            "-NonInteractive",
            "-Verbosity", "normal",
            "-Source", $sources
        )
        Invoke-LoggedExternalCommand -FilePath $nuget -Arguments $installArgs -Description "NuGet install packages.config"
    }

    $required = @(
        (Join-Path $packagesDir "lz4.1.3.2.3\build\native\lz4.targets")
    )
    foreach ($item in $required) {
        if (!(Test-Path -LiteralPath $item)) {
            throw "NuGet restore did not produce required file: $item"
        }
    }

    Write-LogLine "NuGet restore check passed: lz4 package target file present."
}

if (!(Test-Path -LiteralPath $SourceRoot)) {
    if (!(Test-Path -LiteralPath $SourceZip)) {
        throw "Source ZIP not found: $SourceZip"
    }
    Expand-Archive -LiteralPath $SourceZip -DestinationPath (Split-Path -Parent $SourceRoot) -Force
}

$Solution = Join-Path $SourceRoot "win32\libaff4.sln"
if (!(Test-Path -LiteralPath $Solution)) {
    throw "aff4-cpp-lite solution not found: $Solution"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
if (Test-Path -LiteralPath $LogPath) {
    Remove-Item -LiteralPath $LogPath -Force
}

$MSBuild = Resolve-MSBuild
$MSBuild = Assert-CleanMSBuildPath -PathValue $MSBuild

Write-LogLine "Using MSBuild: $MSBuild"
Write-LogLine "Solution: $Solution"

if (!$SkipNuGetRestore) {
    Restore-Aff4CppLiteNuGetPackages -Root $SourceRoot -Solution $Solution -OutputRootForTools $OutputRoot -RequestedNuGetExe $NuGetExe -NoDownload:$NoNuGetDownload
} else {
    Write-LogLine "NuGet restore skipped by -SkipNuGetRestore. Build may fail if win32\packages is missing lz4/OpenSSL/zlib native packages."
}

$SdkForBuild = $WindowsTargetPlatformVersion
if (!$NoRetarget) {
    if (!$SdkForBuild) {
        $SdkForBuild = Resolve-WindowsSDKVersion
    }
    Write-LogLine "Retarget requested: sdk=$SdkForBuild toolset=$PlatformToolset"
    Retarget-Aff4CppLiteProjects -Root $SourceRoot -SdkVersion $SdkForBuild -Toolset $PlatformToolset
} else {
    Write-LogLine "Retarget skipped by -NoRetarget. Legacy projects may require Windows SDK 8.1 and v140 tools."
}

$LibAff4Project = Join-Path $SourceRoot "win32\libaff4\libaff4.vcxproj"
if (!(Test-Path -LiteralPath $LibAff4Project)) {
    throw "libaff4 project not found: $LibAff4Project"
}

$CommonAff4OutDir = Join-Path (Join-Path (Join-Path $SourceRoot "win32") "x64") "Release"
if (!$CommonAff4OutDir.EndsWith("\")) {
    $CommonAff4OutDir += "\"
}
$CommonAff4IntBase = Join-Path (Join-Path (Join-Path $SourceRoot "win32") "x64") "obj"
New-Item -ItemType Directory -Force -Path $CommonAff4OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $CommonAff4IntBase | Out-Null
Write-LogLine "AFF4 common output directory: $CommonAff4OutDir"

function Invoke-Aff4MsBuildProject {
    param(
        [Parameter(Mandatory=$true)][string]$ProjectPath,
        [Parameter(Mandatory=$true)][string]$TargetName
    )

    if (!(Test-Path -LiteralPath $ProjectPath)) {
        throw "Required AFF4 build project not found: $ProjectPath"
    }

    $projectArgs = @(
        $ProjectPath,
        "/m",
        "/verbosity:minimal",
        "/t:Rebuild",
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/p:BuildProjectReferences=true",
        "/p:OutDir=$CommonAff4OutDir",
        "/p:IntDir=$CommonAff4IntBase\$TargetName\",
        "/restore:false"
    )

    if (!$NoRetarget -and $SdkForBuild) {
        $projectArgs += "/p:WindowsTargetPlatformVersion=$SdkForBuild"
    }
    if (!$NoRetarget -and $PlatformToolset) {
        $projectArgs += "/p:PlatformToolset=$PlatformToolset"
    }

    Write-LogLine "MSBuild project target: $TargetName"
    Write-LogLine "MSBuild args: $($projectArgs -join ' ')"
    Invoke-LoggedExternalCommand -FilePath $MSBuild -Arguments $projectArgs -Description "MSBuild $TargetName"
}

if ($BuildEntireSolution) {
    Write-LogLine "Build scope: full solution. This may build examples/tests that require OpenSSL/zlib include paths."

    $msbuildArgs = @(
        $Solution,
        "/m",
        "/verbosity:minimal",
        "/t:Rebuild",
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/restore:false"
    )

    if (!$NoRetarget -and $SdkForBuild) {
        $msbuildArgs += "/p:WindowsTargetPlatformVersion=$SdkForBuild"
    }
    if (!$NoRetarget -and $PlatformToolset) {
        $msbuildArgs += "/p:PlatformToolset=$PlatformToolset"
    }

    Write-LogLine "MSBuild args: $($msbuildArgs -join ' ')"
    Invoke-LoggedExternalCommand -FilePath $MSBuild -Arguments $msbuildArgs -Description "MSBuild full solution"
} else {
    Write-LogLine "Build scope: required AFF4 reader library dependency chain only."

    $RequiredProjects = @(
        @{ Name = "zlib";   Path = (Join-Path $SourceRoot "win32\zlib\zlib.vcxproj") },
        @{ Name = "snappy"; Path = (Join-Path $SourceRoot "win32\snappy\snappy.vcxproj") },
        @{ Name = "raptor2"; Path = (Join-Path $SourceRoot "win32\raptor2\raptor2.vcxproj") },
        @{ Name = "libaff4"; Path = $LibAff4Project }
    )

    foreach ($project in $RequiredProjects) {
        Invoke-Aff4MsBuildProject -ProjectPath $project.Path -TargetName $project.Name
    }
}

$Found = Get-ChildItem $SourceRoot -Recurse -Include "libaff4.dll","libaff4.lib","zlib1.dll","zlib1.lib","snappy.dll","snappy.lib","raptor2.dll","raptor2.lib","*.pdb" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "x64|Release" }

$Dest = Join-Path $OutputRoot "x64\Release"
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
foreach ($item in $Found) {
    Copy-Item -LiteralPath $item.FullName -Destination (Join-Path $Dest $item.Name) -Force
}

$IncludeDest = Join-Path $OutputRoot "include"
New-Item -ItemType Directory -Force -Path $IncludeDest | Out-Null

$HeaderRoots = @(
    (Join-Path $SourceRoot "src"),
    (Join-Path $SourceRoot "include")
) | Where-Object { Test-Path -LiteralPath $_ }

foreach ($headerRoot in $HeaderRoots) {
    Get-ChildItem -LiteralPath $headerRoot -Recurse -Include "*.h","*.hpp","*.hh" -File -ErrorAction SilentlyContinue | ForEach-Object {
        $relative = $_.FullName.Substring($headerRoot.Length).TrimStart('\','/')
        $destPath = Join-Path $IncludeDest $relative
        $destParent = Split-Path -Parent $destPath
        if ($destParent) { New-Item -ItemType Directory -Force -Path $destParent | Out-Null }
        Copy-Item -LiteralPath $_.FullName -Destination $destPath -Force
    }
}

$Manifest = Join-Path $OutputRoot "reader_tools_manifest.csv"
$manifestRows = Get-ChildItem $OutputRoot -Recurse -File | Sort-Object FullName
$manifestLines = New-Object System.Collections.Generic.List[string]
$manifestLines.Add("relative_path,full_path,length,last_write_utc,sha256,role")
foreach ($item in $manifestRows) {
    $relative = $item.FullName.Substring($OutputRoot.Length).TrimStart('\','/')
    $role = "OTHER"
    switch -Regex ($item.Name) {
        '^libaff4\.dll$' { $role = "AFF4_CPP_LITE_LIBRARY_DLL"; break }
        '^libaff4\.lib$' { $role = "AFF4_CPP_LITE_LIBRARY_IMPORT_LIB"; break }
        '^zlib1\.(dll|lib)$' { $role = "AFF4_CPP_LITE_DEPENDENCY_ZLIB"; break }
        '^snappy\.(dll|lib)$' { $role = "AFF4_CPP_LITE_DEPENDENCY_SNAPPY"; break }
        '^raptor2\.(dll|lib)$' { $role = "AFF4_CPP_LITE_DEPENDENCY_RAPTOR2"; break }
        '\.(h|hpp|hh)$' { $role = "AFF4_CPP_LITE_HEADER"; break }
        '\.pdb$' { $role = "DEBUG_SYMBOL"; break }
    }
    $hash = ""
    try { $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash } catch { $hash = "HASH_ERROR: $($_.Exception.Message)" }
    $manifestLines.Add(('"{0}","{1}",{2},"{3}","{4}","{5}"' -f ($relative -replace '"','""'), ($item.FullName -replace '"','""'), $item.Length, $item.LastWriteTimeUtc.ToString("o"), $hash, $role))
}
Set-Content -LiteralPath $Manifest -Value $manifestLines -Encoding UTF8
Write-LogLine "AFF4 CPP Lite reader tools manifest written: $Manifest"

$Inventory = Join-Path (Split-Path -Parent $LogPath) "aff4_cpp_lite_outputs.txt"
$inventoryRows = Get-ChildItem $OutputRoot -Recurse -File | Select-Object FullName, Length
Write-LogObject $inventoryRows
$inventoryRows | Format-Table -AutoSize | Out-String | Set-Content -LiteralPath $Inventory

Write-Host "Build log: $LogPath"
Write-Host "Output inventory: $Inventory"
Write-Host "Reader tools manifest: $Manifest"
Write-Host "Reader tools folder for Vestigant: $OutputRoot"
