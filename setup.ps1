param([Parameter(Mandatory=$true)][string]$EngineRoot,[string]$AssetsDirectory='',
      [ValidateSet('baseline','nvidia')][string]$Graphics='baseline',[switch]$SkipAssets,
      [string]$Python='python')
$ErrorActionPreference='Stop'
$setupArgs=@((Join-Path $PSScriptRoot 'Tools/setup.py'),'--engine',$EngineRoot,'--graphics',$Graphics)
if($AssetsDirectory){$setupArgs+=@('--assets-dir',$AssetsDirectory)}
if($SkipAssets){$setupArgs+='--skip-assets'}
& $Python @setupArgs
if($LASTEXITCODE -ne 0){throw 'Project setup failed; no build was started.'}
