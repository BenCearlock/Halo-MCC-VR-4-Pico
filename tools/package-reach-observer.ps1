[CmdletBinding()]
param(
    [string]$ModulePath =
        'N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\haloreach\haloreach.dll',
    [string]$HrekPath = 'N:\SteamLibrary\steamapps\common\HREK',
    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'

function Assert-NestedProofAndHookFieldsFalse {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ($Value -is [System.Management.Automation.PSCustomObject]) {
        foreach ($property in $Value.PSObject.Properties) {
            $propertyPath = "$Path.$($property.Name)"
            if ($property.Name -match '(?i)(proof|hook)') {
                if ($property.Value -isnot [bool] -or
                        $property.Value -ne $false) {
                    throw "$propertyPath must be the boolean false before packaging."
                }
            }
            if ($null -ne $property.Value) {
                Assert-NestedProofAndHookFieldsFalse `
                    -Value $property.Value -Path $propertyPath
            }
        }
        return
    }

    if ($Value -is [System.Collections.IDictionary]) {
        foreach ($key in $Value.Keys) {
            $propertyPath = "$Path.$key"
            if ([string]$key -match '(?i)(proof|hook)') {
                if ($Value[$key] -isnot [bool] -or
                        $Value[$key] -ne $false) {
                    throw "$propertyPath must be the boolean false before packaging."
                }
            }
            if ($null -ne $Value[$key]) {
                Assert-NestedProofAndHookFieldsFalse `
                    -Value $Value[$key] -Path $propertyPath
            }
        }
        return
    }

    if ($Value -is [System.Collections.IEnumerable] -and
            $Value -isnot [string]) {
        $index = 0
        foreach ($item in $Value) {
            if ($null -ne $item) {
                Assert-NestedProofAndHookFieldsFalse `
                    -Value $item -Path "$Path[$index]"
            }
            ++$index
        }
    }
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'out'))
$diagnosticRoot = [IO.Path]::GetFullPath(
    (Join-Path $outRoot 'diagnostics'))
$expectedOutRoot =
    $outRoot.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) +
    [IO.Path]::DirectorySeparatorChar

if (-not $ManifestPath) {
    $ManifestPath =
        Join-Path $repoRoot 'docs\REACH-EVIDENCE-MANIFEST.json'
}
$ManifestPath = [IO.Path]::GetFullPath($ManifestPath)
$ModulePath = [IO.Path]::GetFullPath($ModulePath)
$HrekPath = [IO.Path]::GetFullPath($HrekPath)

if (-not $diagnosticRoot.StartsWith(
        $expectedOutRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Diagnostic path escaped the repository out directory: $diagnosticRoot"
}

$observerSourcePath =
    Join-Path $repoRoot 'tools\reach_runtime_observer.cpp'
$runbookPath =
    Join-Path $repoRoot 'docs\REACH-RUNTIME-OBSERVER.md'
$licensePath = Join-Path $repoRoot 'LICENSE'
$preflightPath = Join-Path $repoRoot 'tools\reach-preflight.ps1'

foreach ($requiredPath in @(
        $observerSourcePath,
        $runbookPath,
        $licensePath,
        $preflightPath,
        $ManifestPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required observer package input is missing: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $status = @(
        & git -C $repoRoot status --porcelain=v1 --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect Git worktree state.'
    }
    if ($status.Count -ne 0) {
        throw ("Refusing to package a dirty worktree. Commit the diagnostic first:`n" +
            ($status -join "`n"))
    }

    $commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
        throw 'Could not resolve the observer source commit.'
    }

    $acceptedSource = '3a2a11bfc66b36e70f60282e91c9d5436f2e18d1'
    & git -C $repoRoot merge-base --is-ancestor $acceptedSource $commit
    if ($LASTEXITCODE -ne 0) {
        throw "Refusing to package: HEAD does not descend from accepted source $acceptedSource."
    }

    $evidence =
        Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
    if ($evidence.hook_eligible -ne $false) {
        throw 'Reach evidence manifest must remain hook_eligible=false.'
    }
    $runtimeObserverProperty =
        $evidence.PSObject.Properties['runtime_observer']
    if ($null -eq $runtimeObserverProperty -or
            $null -eq $runtimeObserverProperty.Value) {
        throw 'Reach evidence manifest is missing runtime_observer.'
    }
    $runtimeObserver = $runtimeObserverProperty.Value
    if ($runtimeObserver.status -cne 'IMPLEMENTED_UNRUN') {
        throw 'Reach runtime_observer.status must remain IMPLEMENTED_UNRUN.'
    }
    $observedResultsProperty =
        $runtimeObserver.PSObject.Properties['observed_runtime_results']
    if ($null -eq $observedResultsProperty -or
            $observedResultsProperty.Value -isnot [System.Array] -or
            $observedResultsProperty.Value.Count -ne 0) {
        throw 'Reach runtime_observer.observed_runtime_results must be an empty array.'
    }
    Assert-NestedProofAndHookFieldsFalse `
        -Value $runtimeObserver -Path 'runtime_observer'

    $playerViewProperty =
        $evidence.PSObject.Properties['player_view_transaction']
    if ($null -eq $playerViewProperty -or
            $null -eq $playerViewProperty.Value) {
        throw 'Reach evidence manifest is missing player_view_transaction.'
    }
    foreach ($field in @('proof_complete', 'hook_eligible')) {
        $fieldProperty =
            $playerViewProperty.Value.PSObject.Properties[$field]
        if ($null -eq $fieldProperty -or
                $fieldProperty.Value -isnot [bool] -or
                $fieldProperty.Value -ne $false) {
            throw "Reach player_view_transaction.$field must be the boolean false."
        }
    }

    $expectedModuleName = [string]$evidence.retail_module.name
    $expectedModuleHash =
        ([string]$evidence.retail_module.sha256).ToUpperInvariant()
    $expectedTimestamp = [string]$evidence.retail_module.pe_timestamp
    $expectedImageSize = [string]$evidence.retail_module.size_of_image
    if ($expectedModuleName -ine 'haloreach.dll' -or
            $expectedModuleHash -notmatch '^[0-9A-F]{64}$' -or
            $expectedTimestamp -notmatch '^0x[0-9A-Fa-f]{8}$' -or
            $expectedImageSize -notmatch '^0x[0-9A-Fa-f]{8}$') {
        throw 'Reach evidence manifest has an invalid retail module identity.'
    }
    $expectedHrekBuild = [string]$evidence.editing_kit.build
    $expectedCameraEvidence =
        $evidence.editing_kit.camera_evidence_binary

    $observerSource =
        [IO.File]::ReadAllText($observerSourcePath)
    $requiredIdentityLiterals = @(
        $expectedModuleName,
        $expectedModuleHash,
        $expectedTimestamp,
        $expectedImageSize)
    foreach ($literal in $requiredIdentityLiterals) {
        if ($observerSource.IndexOf(
                $literal,
                [StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "Observer source does not embed the evidence identity literal: $literal"
        }
    }

    $accessDeclaration =
        '(?s)\bconstexpr\s+DWORD\s+kProcessAccess\s*=\s*' +
        'PROCESS_QUERY_INFORMATION\s*\|\s*PROCESS_VM_READ\s*;'
    if ($observerSource -notmatch $accessDeclaration) {
        throw ('Observer process rights are not exactly ' +
            'PROCESS_QUERY_INFORMATION | PROCESS_VM_READ.')
    }
    $allOpenProcessCalls = [regex]::Matches(
        $observerSource,
        '(?m)^[^/"\r\n]*\bOpenProcess\s*\(')
    $readOnlyOpenProcessCalls = [regex]::Matches(
        $observerSource,
        '(?m)^[^/"\r\n]*\bOpenProcess\s*\(\s*kProcessAccess\s*,')
    if ($allOpenProcessCalls.Count -eq 0 -or
            $allOpenProcessCalls.Count -ne
                $readOnlyOpenProcessCalls.Count) {
        throw 'Every OpenProcess call must use the exact read-only kProcessAccess mask.'
    }

    $requiredRuntimeGuards = [ordered]@{
        'running-executable self-hash' =
            '(?s)CreateFileW\s*\(\s*reportedObserverPath\.c_str\(\)\s*,' +
            '\s*GENERIC_READ\s*,\s*FILE_SHARE_READ\s*,.*' +
            'HashOpenFile\s*\(\s*observerFile\s*,' +
            '\s*observerHash\s*\).*observerSHA256=%s.*' +
            'CloseHandle\s*\(\s*observerFile\s*\)'
        'normalized volume-guid handle resolution' =
            '(?s)FILE_NAME_NORMALIZED\s*\|\s*VOLUME_NAME_GUID.*' +
            'GetFinalPathNameByHandleW\s*\('
        'observer and process image canonicalization' =
            '(?s)CanonicalPathFromHandle\s*\(\s*observerFile\s*,' +
            '\s*observerPath\s*\).*' +
            'CanonicalPathForExisting\s*\(\s*processPath\s*,' +
            '\s*false\s*,\s*canonicalProcessPath\s*\)'
        'output parent canonicalization' =
            '(?s)ResolveCanonicalNewFilePath\s*\(\s*options\.outputPath\s*,' +
            '\s*canonicalOutput\s*,\s*outputLeaf\s*,' +
            '\s*outputParentGuard\s*\).*' +
            'options\.outputPath\s*=\s*std::move\s*\(\s*canonicalOutput\s*\)'
        'output parent held against rename' =
            '(?s)HANDLE\s+parentHandle\s*=\s*CreateFileW\s*\(\s*' +
            'parent\.c_str\(\)\s*,\s*FILE_READ_ATTRIBUTES\s*,' +
            '\s*FILE_SHARE_READ\s*\|\s*FILE_SHARE_WRITE\s*,' +
            '.*parentGuard\s*=\s*parentHandle'
        'alternate-data-stream output rejection' =
            'leaf\.find\s*\(\s*L'':''\s*\)\s*!=\s*std::wstring::npos'
        'observer path outside the MCC installation' =
            'if\s*\(\s*IsPathWithin\s*\(\s*observerPath\s*,' +
            '\s*mccInstallRoot\s*\)\s*\)'
        'log path outside the MCC installation' =
            'if\s*\(\s*IsPathWithin\s*\(\s*options\.outputPath\s*,' +
            '\s*mccInstallRoot\s*\)\s*\)'
        'existing-log refusal' =
            'GetFileAttributesW\s*\(\s*options\.outputPath\.c_str\(\)\s*\)' +
            '\s*!=\s*INVALID_FILE_ATTRIBUTES'
        'handle-relative single-writer log creation' =
            '(?s)InitializeObjectAttributes\s*\(\s*&attributes\s*,' +
            '\s*&name\s*,\s*OBJ_CASE_INSENSITIVE\s*,\s*directory\s*,' +
            '\s*nullptr\s*\).*NtCreateFile\s*\(\s*&file\s*,' +
            '\s*GENERIC_WRITE\s*\|\s*SYNCHRONIZE\s*,\s*&attributes\s*,' +
            '.*FILE_SHARE_READ\s*,\s*FILE_CREATE\s*,'
        'handle-relative final-component reparse refusal' =
            'FILE_NON_DIRECTORY_FILE\s*\|\s*' +
            'FILE_SYNCHRONOUS_IO_NONALERT\s*\|\s*FILE_OPEN_REPARSE_POINT'
        'held parent coupled to log open' =
            '(?s)report\.OpenRelative\s*\(\s*outputParentGuard\s*,' +
            '\s*outputLeaf\s*\).*CloseHandle\s*\(\s*outputParentGuard\s*\)'
    }
    foreach ($guard in $requiredRuntimeGuards.GetEnumerator()) {
        if ($observerSource -notmatch $guard.Value) {
            throw "Observer source is missing runtime guard: $($guard.Key)"
        }
    }

    $forbiddenApis = @(
        'WriteProcessMemory',
        'NtWriteVirtualMemory',
        'ZwWriteVirtualMemory',
        'VirtualAllocEx',
        'VirtualProtectEx',
        'VirtualFreeEx',
        'CreateRemoteThread',
        'CreateRemoteThreadEx',
        'NtCreateThreadEx',
        'RtlCreateUserThread',
        'QueueUserAPC',
        'SetWindowsHookEx',
        'SetWindowsHookExA',
        'SetWindowsHookExW',
        'DebugActiveProcess',
        'DebugActiveProcessStop',
        'WaitForDebugEvent',
        'ContinueDebugEvent',
        'SuspendThread',
        'ResumeThread',
        'SetThreadContext',
        'Wow64SetThreadContext',
        'CreateProcess',
        'CreateProcessA',
        'CreateProcessW',
        'ShellExecute',
        'ShellExecuteA',
        'ShellExecuteW',
        'ShellExecuteEx',
        'ShellExecuteExA',
        'ShellExecuteExW',
        'WinExec',
        'system',
        '_wsystem',
        '_popen',
        '_wpopen')
    foreach ($api in $forbiddenApis) {
        $apiPattern = '\b' + [regex]::Escape($api) + '\b'
        if ($observerSource -match $apiPattern) {
            throw "Observer source contains forbidden API: $api"
        }
    }

    & cmake --preset release
    if ($LASTEXITCODE -ne 0) {
        throw 'CMake configure failed for preset release.'
    }

    $cachePath =
        Join-Path $repoRoot 'out\build\release\CMakeCache.txt'
    $cache = [IO.File]::ReadAllText($cachePath)
    if ($cache -notmatch
            '(?m)^HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP:BOOL=ON\r?$' -or
            $cache -notmatch
            '(?m)^HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP:BOOL=OFF\r?$') {
        throw 'Release preset is not the cumulative ODST-ON, Reach-OFF configuration.'
    }

    & cmake --build --preset release --clean-first --target `
        reach_runtime_observer halomccvr_core_tests
    if ($LASTEXITCODE -ne 0) {
        throw 'Reach observer or core-test Release build failed.'
    }

    & ctest --preset release
    if ($LASTEXITCODE -ne 0) {
        throw 'Core tests failed for preset release.'
    }

    & $preflightPath -ModulePath $ModulePath -HrekPath $HrekPath `
        -ManifestPath $ManifestPath

    $finalCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
    $finalStatus = @(
        & git -C $repoRoot status --porcelain=v1 --untracked-files=normal)
    if ($LASTEXITCODE -ne 0 -or $finalCommit -ne $commit -or
            $finalStatus.Count -ne 0) {
        throw 'Repository state changed during validation; refusing to label the diagnostic.'
    }

    $observerBuildPath = Join-Path $repoRoot `
        'out\build\release\Release\reach-runtime-observer.exe'
    if (-not (Test-Path -LiteralPath $observerBuildPath -PathType Leaf)) {
        throw "Release observer executable is missing: $observerBuildPath"
    }

    $createdUtc = [DateTime]::UtcNow
    $packageId = '{0}-reach-runtime-observer-{1}' -f
        $commit.Substring(0, 7),
        $createdUtc.ToString("yyyyMMdd-HHmmssfff'Z'")
    $packageDir =
        [IO.Path]::GetFullPath((Join-Path $diagnosticRoot $packageId))
    $expectedDiagnosticRoot =
        $diagnosticRoot.TrimEnd(
            [IO.Path]::DirectorySeparatorChar,
            [IO.Path]::AltDirectorySeparatorChar) +
        [IO.Path]::DirectorySeparatorChar
    if (-not $packageDir.StartsWith(
            $expectedDiagnosticRoot,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Diagnostic package path escaped its staging root: $packageDir"
    }
    if (Test-Path -LiteralPath $packageDir) {
        throw "Refusing to reuse diagnostic directory: $packageDir"
    }
    [IO.Directory]::CreateDirectory($packageDir) | Out-Null

    $stagedObserver =
        Join-Path $packageDir 'reach-runtime-observer.exe'
    $stagedRunbook =
        Join-Path $packageDir 'REACH-RUNTIME-OBSERVER.md'
    $stagedLicense = Join-Path $packageDir 'LICENSE'
    [IO.File]::Copy($observerBuildPath, $stagedObserver, $false)
    [IO.File]::Copy($runbookPath, $stagedRunbook, $false)
    [IO.File]::Copy($licensePath, $stagedLicense, $false)

    $files = [ordered]@{}
    foreach ($stagedPath in @(
            $stagedObserver,
            $stagedRunbook,
            $stagedLicense)) {
        $item = Get-Item -LiteralPath $stagedPath
        $files[$item.Name] = [ordered]@{
            bytes = $item.Length
            sha256 =
                (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        }
    }

    $manifest = [ordered]@{
        schema_version = 1
        status = 'UNRUN'
        accepted = $false
        proof_complete = $false
        hook_eligible = $false
        package_id = $packageId
        created_utc = $createdUtc.ToString('o')
        source_commit = $commit
        accepted_source_ancestor = $acceptedSource
        base_release = 'MCC_VR_ALPHA_0.2.2'
        build = [ordered]@{
            configure_preset = 'release'
            target = 'reach_runtime_observer'
            tests = 'halomccvr_core_tests'
            offline_reach_preflight = 'passed'
        }
        expected_reach_module = [ordered]@{
            name = $expectedModuleName
            sha256 = $expectedModuleHash
            pe_timestamp = $expectedTimestamp
            size_of_image = $expectedImageSize
        }
        editing_kit_evidence = [ordered]@{
            build = $expectedHrekBuild
            camera_binary = [ordered]@{
                name = [string]$expectedCameraEvidence.name
                sha256 = [string]$expectedCameraEvidence.sha256
                pe_timestamp = [string]$expectedCameraEvidence.pe_timestamp
                size_of_image = [string]$expectedCameraEvidence.size_of_image
            }
        }
        evidence_state_gate = [ordered]@{
            runtime_observer_status = 'IMPLEMENTED_UNRUN'
            observed_runtime_results_count = 0
            observer_proof_and_hook_fields = $false
            player_view_transaction_proof_complete = $false
            player_view_transaction_hook_eligible = $false
        }
        process_access = [ordered]@{
            mask = '0x00000410'
            requested_rights = @(
                'PROCESS_QUERY_INFORMATION',
                'PROCESS_VM_READ')
            PROCESS_VM_WRITE = $false
            PROCESS_VM_OPERATION = $false
            PROCESS_CREATE_THREAD = $false
            PROCESS_SUSPEND_RESUME = $false
        }
        safety_contract = [ordered]@{
            standalone_external_observer = $true
            observer_executed_by_packager = $false
            packager_launches_mcc_process = $false
            packager_attaches_to_mcc_process = $false
            offline_preflight_read_opens_installed_evidence = $true
            offline_preflight_attaches_to_mcc_process = $false
            installs_to_mcc = $false
            writes_mcc_installation = $false
            writes_mcc_process_memory = $false
            changes_remote_page_protection = $false
            injects_code = $false
            installs_hooks = $false
            attaches_as_debugger = $false
            forbidden_api_source_scan = 'passed'
            observer_runtime_guard_source_scan = 'passed'
            build_and_dependency_output_scope = 'repository ignored out/'
            package_staging_scope = 'repository out/diagnostics only'
        }
        observer_runtime_guards = [ordered]@{
            running_executable_sha256_logged = $true
            canonical_path_api = 'GetFinalPathNameByHandleW'
            canonical_path_flags = 'FILE_NAME_NORMALIZED|VOLUME_NAME_GUID'
            canonicalizes_process_observer_install_and_output_parent = $true
            hashes_already_open_running_executable_handle = $true
            running_executable_handle_denies_write_share = $true
            creates_log_relative_to_held_parent_handle = $true
            output_parent_handle_held_without_delete_share_through_create = $true
            log_native_create_api = 'NtCreateFile'
            log_create_disposition = 'FILE_CREATE'
            log_create_options = @(
                'FILE_NON_DIRECTORY_FILE',
                'FILE_SYNCHRONOUS_IO_NONALERT',
                'FILE_OPEN_REPARSE_POINT')
            rejects_alternate_data_stream_leaf = $true
            refuses_executable_inside_mcc_installation = $true
            refuses_log_inside_mcc_installation = $true
            refuses_existing_log_path = $true
            log_share_mode = 'FILE_SHARE_READ'
            exclusive_log_writer = $true
        }
        files = $files
        note = ('Packaged but never run. Runtime observations from this exact ' +
            'executable are required before any Reach proof can advance.')
    }

    $manifestOutputPath =
        Join-Path $packageDir 'DIAGNOSTIC-MANIFEST.json'
    $json = $manifest | ConvertTo-Json -Depth 7
    [IO.File]::WriteAllText(
        $manifestOutputPath,
        $json + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))

    Write-Host "Created UNRUN Reach diagnostic: $packageDir"
    Write-Host "Source:   $commit"
    Write-Host "Observer: $($files['reach-runtime-observer.exe'].sha256)"
    Write-Host ('The packager did not launch or attach to an MCC process ' +
        'and did not write to the MCC installation.')
    Write-Host ('Offline preflight read-opened the configured installed ' +
        'haloreach.dll and HREK evidence.')
}
finally {
    Pop-Location
}
