param([Parameter(Mandatory=$true)][string]$EngineRoot,
      [ValidateSet('Editor','Shipping')][string]$Target='Editor',
      [string]$OutputRoot='',[int]$ParallelActions=4)
$ErrorActionPreference='Stop'
$repoRoot=$PSScriptRoot
$projectFile=Join-Path $repoRoot 'Project/EndlessWorld.uproject'
$engineVersion=Get-Content -Raw -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') | ConvertFrom-Json
if($engineVersion.MajorVersion -ne 5 -or $engineVersion.MinorVersion -ne 8 -or $engineVersion.PatchVersion -ne 2){throw 'Unreal Engine 5.8.2 required.'}
if(-not $OutputRoot){$OutputRoot=Join-Path $repoRoot ('Local/'+$Target+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
if(Test-Path -LiteralPath $OutputRoot){throw "Preserving existing output: $OutputRoot"}
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$OutputRoot=(Resolve-Path -LiteralPath $OutputRoot).Path
$buildLog=Join-Path $OutputRoot 'build.log'
$priorDDC=[Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $repoRoot 'Local/DDC'),'Process')
    if($Target -eq 'Editor'){
        & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') EndlessWorldEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoUBA "-MaxParallelActions=$ParallelActions" *> $buildLog
    }else{
        $arguments=@('BuildCookRun',"-project=$projectFile",'-noP4','-platform=Win64','-clientconfig=Shipping',
            '-build','-cook','-stage','-pak','-iostore','-compressed','-prereqs','-archive',
            "-CookOutputDir=$(Join-Path $OutputRoot 'Cook/Windows')", "-stagingdirectory=$(Join-Path $OutputRoot 'Stage')",
            "-archivedirectory=$(Join-Path $OutputRoot 'Archive')", "-UbtArgs=-gather -MaxParallelActions=$ParallelActions -NoUBA",
            '-nodebuginfo','-utf8output','-ddc=NoZenLocalFallback',
            '-AdditionalCookerOptions=-nosound -EWSilentAudit -SharedDataCachePath=None -asyncassetcompilationmaxconcurrency=2 -asyncstaticmeshcompilationmaxconcurrency=1')
        & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat') @arguments *> $buildLog
    }
    if($LASTEXITCODE -ne 0){throw "Build failed. Inspect $buildLog"}
    if($Target -eq 'Editor'){
        $prepareLog=Join-Path $OutputRoot 'prepare-content.log'
        & (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $projectFile -run=pythonscript "-script=$(Join-Path $repoRoot 'Tools/Art/ensure_cloud_materials.py')" -unattended -nosplash -nosound -nullrhi -nop4 -stdout -UTF8Output "-abslog=$prepareLog" *> (Join-Path $OutputRoot 'prepare-content-console.log')
        if($LASTEXITCODE -ne 0){throw "Content preparation failed. Inspect $prepareLog"}
        if(Select-String -LiteralPath $prepareLog,(Join-Path $OutputRoot 'prepare-content-console.log') -Pattern 'Failed to compile Material|doesn.t have a valid ShaderMap|Shadermap pointer is null|LogMaterial: Error' -Quiet){throw 'Generated material compilation failed.'}
    }
    if(Select-String -LiteralPath $buildLog -Pattern 'Failed to compile Material|doesn.t have a valid ShaderMap|Shadermap pointer is null' -Quiet){throw 'Material compilation failed.'}
    if($Target -eq 'Shipping'){
        $archiveRoot=Join-Path $OutputRoot 'Archive'
        Copy-Item -LiteralPath (Join-Path $repoRoot 'Licenses') -Destination (Join-Path $archiveRoot 'Licenses') -Recurse
        foreach($name in @('LICENSE','ASSET-LICENSE','NOTICE','GAMEPLAY-VIDEO-PERMISSION.md','THIRD-PARTY-NOTICES.md','PLAY.cmd')){
            Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination $archiveRoot
        }
        Copy-Item -LiteralPath (Join-Path $repoRoot 'Docs/PLAY-JA.md') -Destination (Join-Path $archiveRoot 'README-JA.md')
        Copy-Item -LiteralPath (Join-Path $repoRoot 'Docs/PLAY-EN.md') -Destination (Join-Path $archiveRoot 'README-EN.md')
        Copy-Item -LiteralPath (Join-Path $repoRoot 'Docs/END-USER-TERMS.md') -Destination (Join-Path $archiveRoot 'END-USER-TERMS.md')
        Copy-Item -LiteralPath (Join-Path $repoRoot 'ThirdPartySources') -Destination (Join-Path $archiveRoot 'ThirdPartySources') -Recurse
    }
    Write-Output "Build succeeded: $OutputRoot"
}finally{[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$priorDDC,'Process')}
