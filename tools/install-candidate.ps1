[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CandidateDir,

    [string]$GameDir =
        'N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\Halo_MCC_VR'
)

# Installs one already-packaged cumulative candidate. The package manifest is
# the authority: no build tree, loose DLL, old backup, or release artifact may
# enter this path. This script never launches MCC and never changes the shared
# configuration.

$ErrorActionPreference = 'Stop'

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-FileIdentity(
    [string]$Path,
    [object]$Evidence,
    [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label is missing: $Path"
    }
    $item = Get-Item -LiteralPath $Path
    if ([int64]$Evidence.bytes -ne $item.Length) {
        throw "$Label length mismatch: expected $($Evidence.bytes), got $($item.Length)."
    }
    $expected = ([string]$Evidence.sha256).ToUpperInvariant()
    if ($expected -notmatch '^[0-9A-F]{64}$') {
        throw "$Label manifest SHA-256 is invalid."
    }
    $actual = Get-Sha256 $Path
    if ($actual -cne $expected) {
        throw "$Label SHA-256 mismatch: expected $expected, got $actual."
    }
    return $actual
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidateRoot = [IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'out\candidates'))
$candidatePrefix = $candidateRoot.TrimEnd(
    [IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
$candidatePath = [IO.Path]::GetFullPath($CandidateDir)
if (-not $candidatePath.StartsWith(
        $candidatePrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Candidate escaped the repository candidate directory: $candidatePath"
}
if (-not (Test-Path -LiteralPath $candidatePath -PathType Container)) {
    throw "Candidate directory does not exist: $candidatePath"
}

$manifestPath = Join-Path $candidatePath 'CANDIDATE-MANIFEST.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Candidate manifest is missing: $manifestPath"
}
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$packageId = [IO.Path]::GetFileName(
    $candidatePath.TrimEnd([IO.Path]::DirectorySeparatorChar))
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') {
    throw 'Could not resolve current source commit for deployment.'
}
$repoStatus = @(& git -C $repoRoot status --porcelain=v1 --untracked-files=normal)
if ($LASTEXITCODE -ne 0 -or $repoStatus.Count -ne 0) {
    throw 'Repository is dirty; refusing automatic deployment.'
}
if ([int]$manifest.schema_version -ne 6 -or
        [string]$manifest.status -cne 'UNTESTED_LOCAL_CANDIDATE' -or
        $manifest.accepted -ne $false -or
        [string]$manifest.package_id -cne $packageId -or
        [string]$manifest.source_commit -notmatch '^[0-9a-f]{40}$' -or
        [string]$manifest.source_commit -cne $head -or
        -not $packageId.StartsWith(
            $head.Substring(0, 7) + '-',
            [StringComparison]::Ordinal) -or
        $manifest.embedded_build_identity.source_commit -cne
            $manifest.source_commit -or
        $manifest.embedded_build_identity.odst -ne $true -or
        $manifest.embedded_build_identity.reach -ne $true -or
        $manifest.embedded_build_identity.reach_render -ne $true -or
        $manifest.deployment_policy.automatic_after_package -ne $true -or
        [string]$manifest.deployment_policy.installer -cne
            'tools/install-candidate.ps1' -or
        $manifest.deployment_policy.launches_mcc -ne $false -or
        $manifest.deployment_policy.changes_config -ne $false -or
        $manifest.reach_fp_h3_odst_transaction_parity_gate -ne $true -or
        $manifest.reach_fp_nested_camera_workspace -ne $true -or
        $manifest.reach_fp_world_projection_execution_status -ne $true -or
        $manifest.reach_forced_floating_hands -ne $true -or
        $manifest.reach_runtime_hooks_enabled -ne $true) {
    throw 'Candidate manifest identity or cumulative-title contract is invalid.'
}

$candidateDll = Join-Path $candidatePath 'halo3xr.dll'
$candidateLauncher = Join-Path $candidatePath 'halo3xr_launcher.exe'
$dllHash = Assert-FileIdentity `
    $candidateDll $manifest.files.'halo3xr.dll' 'Candidate DLL'
$launcherHash = Assert-FileIdentity `
    $candidateLauncher $manifest.files.'halo3xr_launcher.exe' 'Candidate launcher'

$gamePath = [IO.Path]::GetFullPath($GameDir)
if (-not (Test-Path -LiteralPath $gamePath -PathType Container)) {
    throw "Dedicated Halo_MCC_VR directory does not exist: $gamePath"
}
$installedDll = Join-Path $gamePath 'halo3xr.dll'
$installedLauncher = Join-Path $gamePath 'halo3xr_launcher.exe'
if (-not (Test-Path -LiteralPath $installedDll -PathType Leaf) -or
        -not (Test-Path -LiteralPath $installedLauncher -PathType Leaf)) {
    throw 'Existing dedicated mod DLL/launcher pair is incomplete; refusing automatic install.'
}

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -in @('MCC-Win64-Shipping', 'halo3xr_launcher')
})
if ($running.Count -ne 0) {
    $owners = ($running | ForEach-Object {
        '{0}:{1}' -f $_.ProcessName, $_.Id
    }) -join ', '
    throw "MCC or its launcher is running ($owners); automatic install made no changes."
}

$priorDllHash = Get-Sha256 $installedDll
$priorLauncherHash = Get-Sha256 $installedLauncher
if ($priorDllHash -ceq $dllHash -and
        $priorLauncherHash -ceq $launcherHash) {
    Write-Host "Candidate already installed: $packageId"
    Write-Host "Installed source:   $($manifest.source_commit)"
    Write-Host "Installed DLL:      $dllHash"
    Write-Host "Installed launcher: $launcherHash"
    return
}

$backupRoot = [IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'out\deploy-backups'))
$expectedBackupPrefix = [IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'out')) + [IO.Path]::DirectorySeparatorChar
if (-not $backupRoot.StartsWith(
        $expectedBackupPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Backup root escaped the repository out directory: $backupRoot"
}
$createdUtc = [DateTime]::UtcNow
$backupId = '{0}-before-{1}-{2}' -f `
    $priorDllHash.Substring(0, 7).ToLowerInvariant(),
    $manifest.source_commit.Substring(0, 7),
    $createdUtc.ToString("yyyyMMdd-HHmmssfff'Z'")
$backupDir = Join-Path $backupRoot $backupId
if (Test-Path -LiteralPath $backupDir) {
    throw "Refusing to reuse deployment backup directory: $backupDir"
}
New-Item -ItemType Directory -Path $backupDir | Out-Null

$configPath = Join-Path $gamePath 'halomccvr.cfg'
$logPath = Join-Path $gamePath 'halo3xr.log'
$configHashBefore = $null
Copy-Item -LiteralPath $installedDll -Destination `
    (Join-Path $backupDir 'halo3xr.dll')
Copy-Item -LiteralPath $installedLauncher -Destination `
    (Join-Path $backupDir 'halo3xr_launcher.exe')
if (Test-Path -LiteralPath $configPath -PathType Leaf) {
    $configHashBefore = Get-Sha256 $configPath
    Copy-Item -LiteralPath $configPath -Destination `
        (Join-Path $backupDir 'halomccvr.cfg')
}
if (Test-Path -LiteralPath $logPath -PathType Leaf) {
    Copy-Item -LiteralPath $logPath -Destination `
        (Join-Path $backupDir 'halo3xr.log')
}

if ((Get-Sha256 (Join-Path $backupDir 'halo3xr.dll')) -cne $priorDllHash -or
        (Get-Sha256 (Join-Path $backupDir 'halo3xr_launcher.exe')) -cne
            $priorLauncherHash) {
    throw 'Deployment backup verification failed; automatic install made no changes.'
}

$stagedDll = Join-Path $gamePath ("halo3xr.dll.$packageId.pending")
$stagedLauncher = Join-Path $gamePath `
    ("halo3xr_launcher.exe.$packageId.pending")
if ((Test-Path -LiteralPath $stagedDll) -or
        (Test-Path -LiteralPath $stagedLauncher)) {
    throw 'A candidate staging file already exists; refusing to overwrite it.'
}

try {
    Copy-Item -LiteralPath $candidateDll -Destination $stagedDll
    Copy-Item -LiteralPath $candidateLauncher -Destination $stagedLauncher
    if ((Get-Sha256 $stagedDll) -cne $dllHash -or
            (Get-Sha256 $stagedLauncher) -cne $launcherHash) {
        throw 'Staged candidate hash verification failed; installed files remain unchanged.'
    }

    Copy-Item -LiteralPath $stagedDll -Destination $installedDll -Force
    Copy-Item -LiteralPath $stagedLauncher -Destination $installedLauncher -Force
}
finally {
    if (Test-Path -LiteralPath $stagedDll -PathType Leaf) {
        Remove-Item -LiteralPath $stagedDll -Force
    }
    if (Test-Path -LiteralPath $stagedLauncher -PathType Leaf) {
        Remove-Item -LiteralPath $stagedLauncher -Force
    }
}

$installedDllHash = Get-Sha256 $installedDll
$installedLauncherHash = Get-Sha256 $installedLauncher
if ($installedDllHash -cne $dllHash -or
        $installedLauncherHash -cne $launcherHash) {
    throw "Post-install hash mismatch: DLL=$installedDllHash launcher=$installedLauncherHash"
}
if ($null -ne $configHashBefore -and
        (Get-Sha256 $configPath) -cne $configHashBefore) {
    throw 'Shared configuration changed during deployment.'
}

$deployment = [ordered]@{
    schema_version = 1
    deployed_utc = $createdUtc.ToString('o')
    package_id = $packageId
    source_commit = [string]$manifest.source_commit
    game_dir = $gamePath
    previous = [ordered]@{
        halo3xr_dll_sha256 = $priorDllHash
        halo3xr_launcher_sha256 = $priorLauncherHash
    }
    installed = [ordered]@{
        halo3xr_dll_sha256 = $installedDllHash
        halo3xr_launcher_sha256 = $installedLauncherHash
        config_sha256 = $configHashBefore
    }
    launched = $false
}
$deploymentPath = Join-Path $backupDir 'DEPLOYMENT-MANIFEST.json'
[IO.File]::WriteAllText(
    $deploymentPath,
    ($deployment | ConvertTo-Json -Depth 5) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

Write-Host "Automatically installed candidate: $packageId"
Write-Host "Installed source:   $($manifest.source_commit)"
Write-Host "Installed DLL:      $installedDllHash"
Write-Host "Installed launcher: $installedLauncherHash"
Write-Host "Preserved previous: $backupDir"
Write-Host 'MCC was not launched and halomccvr.cfg was not changed.'
