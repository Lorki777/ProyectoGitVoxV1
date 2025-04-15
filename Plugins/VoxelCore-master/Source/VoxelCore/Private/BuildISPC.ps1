$ErrorActionPreference = "Stop"

$ISPC = "ISPC_PATH"
$Headers = ISPC_HEADERS
$ISPCArgs = ISPC_ARGS
$Includes = ISPC_INCLUDES

$CreatedNew = $false
New-Object System.Threading.Mutex($true, "Global\VoxelISPCMutex", [ref]$CreatedNew)

if (-not $CreatedNew) {
    Write-Output "Another instance of this script is already running. Exiting."
    exit 0
}

$vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$InstallationPath = & $vswherePath -latest -property installationPath
$LIB = "`"$(Get-ChildItem -Path "$InstallationPath\VC\Tools\MSVC" -Recurse -Filter "lib.exe" | Select-Object -First 1 -ExpandProperty FullName)`" /OUT:"

if ($ISPCArgs.ToLower().Contains("android")) {
    $LIB = "`"$($env:NDK_ROOT)\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-ar.exe`" rcs "
}


$global:Commands = @()

function ShouldCompile {
    param (
        [string]$Target,
        [string[]]$Prerequisites
    )

    if (!(Test-Path -Path $Target)) {
        Write-Host "$Target does not exist";
        return $true;
    }

    $TargetTime = (Get-Item $Target).LastWriteTime

    foreach ($Prerequisite in $Prerequisites) {
        $PrerequisiteTime = (Get-Item $Prerequisite).LastWriteTime

        if ($PrerequisiteTime -gt $TargetTime) {
            Write-Host "$Target is older than $Prerequisite";
            return $true;
        }
    }

    Write-Host "$Target is up to date";
    return $false;
}

function Build {
    param (
        [string]$SourceFile,
        [string]$ObjectFile,
        [string]$GeneratedHeader
    )

    $Prerequisites = $Headers + $SourceFile

    if (ShouldCompile -Target $GeneratedHeader -Prerequisites $Prerequisites) {
        $global:Commands += "& `"$ISPC`" `"$SourceFile`" -h `"$GeneratedHeader`" -DVOXEL_DEBUG=0 --werror $Includes $ISPCArgs"
    }

    if (ShouldCompile -Target $ObjectFile -Prerequisites $Prerequisites) {
        $global:Commands += "& `"$ISPC`" `"$SourceFile`" -o `"$ObjectFile`" -O3 -DVOXEL_DEBUG=0 --werror $Includes $ISPCArgs"
    }
}

function BuildLibrary {
    param (
        [string]$TargetFile,
        [string[]]$ObjectFiles
    )

    if (ShouldCompile -Target $TargetFile -Prerequisites $ObjectFiles) {
        Remove-Item -Path $TargetFile
        $QuotedObjectFiles = $ObjectFiles | ForEach-Object { "'$_'" }
        $global:Commands += "& $LIB`"$TargetFile`" $($QuotedObjectFiles -join " ")"
    }
}

function FlushCommands {
    $Jobs = foreach ($Command in $global:Commands) {
        Write-Host "Executing $Command";

        $Job = Start-Job -Name $Command -ScriptBlock {
            param ($Command)
            $Output = Invoke-Expression $Command
            $ExitCode = $LastExitCode

            [PSCustomObject]@{
                Command  = $Command
                Output   = $Output
                ExitCode = $ExitCode
            }
        } -ArgumentList $Command

        [PSCustomObject]@{
            Command = $Command
            Job     = $Job
        }
    }

    $Jobs | Wait-Job

    $ExitCode = 0;

    foreach ($Job in $Jobs) {
        try {
            $Result = Receive-Job $Job.Job
        }
        catch {
            Write-Host "Error: Command failed:"
            Write-Host "   Error: $($_.Exception.Message)"
            Write-Host "   Error: Command:  $($Job.Command)"
            $ExitCode = 1;
            continue;
        }

        Write-Host $Result.Output;

        if ($Result.ExitCode -ne 0) {
            Write-Host "Error: $($Result.Command) failed with exit code $($Result.ExitCode)";
            $ExitCode = 1;
        }

        Remove-Job $Job.Job
    }

    $global:Commands = @()

    if ($ExitCode -ne 0) {
        exit $ExitCode;
    }
}
