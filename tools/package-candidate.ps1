[CmdletBinding()]
param(
    [switch]$ReachPrivate
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidateRoot = [IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'out\candidates'))
$expectedCandidateRoot = [IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'out')) + [IO.Path]::DirectorySeparatorChar
$packagePreset = if ($ReachPrivate) { 'release-reach-private' } else { 'release' }
$packageBuildDir = if ($ReachPrivate) {
    'out\build\release-reach-private'
} else {
    'out\build\release'
}
$validationPresets = if ($ReachPrivate) {
    @('release-reach-private')
} else {
    @('release')
}
$reachEnabled = [bool]$ReachPrivate

if (-not $candidateRoot.StartsWith(
        $expectedCandidateRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Candidate path escaped the repository out directory: $candidateRoot"
}

Push-Location $repoRoot
try {
    $status = @(& git -C $repoRoot status --porcelain=v1 --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect Git worktree state.'
    }
    if ($status.Count -ne 0) {
        throw ("Refusing to package a dirty worktree. Commit the candidate first:`n" +
            ($status -join "`n"))
    }

    $commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
        throw 'Could not resolve the candidate source commit.'
    }

    $acceptedSource = '3a2a11bfc66b36e70f60282e91c9d5436f2e18d1'
    & git -C $repoRoot merge-base --is-ancestor $acceptedSource $commit
    if ($LASTEXITCODE -ne 0) {
        throw "Refusing to package: HEAD does not descend from accepted source $acceptedSource."
    }

    foreach ($preset in $validationPresets) {
        & cmake --preset $preset
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed for preset $preset."
        }

        $presetBuildDir = if ($preset -eq 'release-reach-private') {
            'out\build\release-reach-private'
        } else {
            'out\build\release'
        }
        $cachePath = Join-Path $repoRoot "$presetBuildDir\CMakeCache.txt"
        $cache = [IO.File]::ReadAllText($cachePath)
        if ($cache -notmatch
                '(?m)^HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP:BOOL=ON\r?$') {
            throw "Refusing to package: ODST is not ON in preset $preset."
        }
        $expectedReach = if ($preset -eq 'release-reach-private') {
            'ON'
        } else {
            'OFF'
        }
        $reachPattern =
            "(?m)^HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP:BOOL=$expectedReach\r?$"
        if ($cache -notmatch $reachPattern) {
            throw "Refusing to package: Reach is not $expectedReach in preset $preset."
        }

        & cmake --build --preset $preset --clean-first
        if ($LASTEXITCODE -ne 0) {
            throw "Release build failed for preset $preset."
        }

        & ctest --preset $preset
        if ($LASTEXITCODE -ne 0) {
            throw "Core tests failed for preset $preset."
        }
    }

    $finalCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
    $finalStatus =
        @(& git -C $repoRoot status --porcelain=v1 --untracked-files=normal)
    if ($LASTEXITCODE -ne 0 -or $finalCommit -ne $commit -or
            $finalStatus.Count -ne 0) {
        throw 'Repository state changed during build/test; refusing to label the artifacts.'
    }

    $createdUtc = [DateTime]::UtcNow
    $packageKind = if ($ReachPrivate) { 'reach-private' } else { 'release' }
    $packageId = '{0}-{1}-{2}' -f $commit.Substring(0, 7), $packageKind,
        $createdUtc.ToString("yyyyMMdd-HHmmssfff'Z'")
    $packageDir = Join-Path $candidateRoot $packageId
    if (Test-Path -LiteralPath $packageDir) {
        throw "Refusing to reuse candidate directory: $packageDir"
    }

    & cmake --install $packageBuildDir --config Release `
        --prefix $packageDir --component dist
    if ($LASTEXITCODE -ne 0) {
        throw 'Candidate staging failed.'
    }

    $dllPath = Join-Path $packageDir 'halo3xr.dll'
    $launcherPath = Join-Path $packageDir 'halo3xr_launcher.exe'
    foreach ($requiredPath in @(
            $dllPath,
            $launcherPath,
            (Join-Path $packageDir 'LICENSE'),
            (Join-Path $packageDir 'MANUAL-README.txt'))) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Candidate package is missing: $requiredPath"
        }
    }

    $dll = Get-Item -LiteralPath $dllPath
    $launcher = Get-Item -LiteralPath $launcherPath
    $dllHash = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash
    $launcherHash =
        (Get-FileHash -LiteralPath $launcherPath -Algorithm SHA256).Hash

    $manifest = [ordered]@{
        schema_version = 2
        status = 'UNTESTED_LOCAL_CANDIDATE'
        accepted = $false
        package_id = $packageId
        created_utc = $createdUtc.ToString('o')
        source_commit = $commit
        package_preset = $packagePreset
        embedded_build_identity = [ordered]@{
            source_commit = $commit
            HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP = $true
            HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP = $reachEnabled
        }
        reach_controller_input_enabled = $reachEnabled
        reach_runtime_hooks_enabled = $false
        base_release = 'MCC_VR_ALPHA_0.2.2'
        files = [ordered]@{
            'halo3xr.dll' = [ordered]@{
                bytes = $dll.Length
                sha256 = $dllHash
            }
            'halo3xr_launcher.exe' = [ordered]@{
                bytes = $launcher.Length
                sha256 = $launcherHash
            }
        }
        note = 'Not accepted until this exact DLL hash passes the required headset tests.'
    }

    $manifestPath = Join-Path $packageDir 'CANDIDATE-MANIFEST.json'
    $json = $manifest | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText(
        $manifestPath,
        $json + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))

    Write-Host "Created untested candidate: $packageDir"
    Write-Host "Source:   $commit"
    Write-Host "DLL:      $dllHash"
    Write-Host "Launcher: $launcherHash"
}
finally {
    Pop-Location
}
