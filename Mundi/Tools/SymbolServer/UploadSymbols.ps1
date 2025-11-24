# Symbol Server Upload Script
# Automatically processed during build
# Usage: .\UploadSymbols.ps1 -OutDir "C:\Path\To\Binaries\Debug" -Configuration "Debug"

param(
    [Parameter(Mandatory=$true)]
    [string]$OutDir,

    [Parameter(Mandatory=$true)]
    [string]$Configuration
)

# Normalize path (remove trailing dot, backslash)
$OutDir = $OutDir.TrimEnd('.').TrimEnd('\')

# Script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Load config file
$ConfigPath = Join-Path $ScriptDir "symbol-server.config"
if (-not (Test-Path $ConfigPath)) {
    Write-Host "[SymbolServer] Config file not found - skipping upload (keeping local PDB)"
    exit 0
}

# Parse config file
$Config = @{}
Get-Content $ConfigPath | ForEach-Object {
    if ($_ -match '^\s*([^#][^=]+)=(.*)$') {
        $Config[$matches[1].Trim()] = $matches[2].Trim()
    }
}

$ServerIP = $Config['ServerIP']
$ServerUser = $Config['ServerUser']
$SymbolsPath = $Config['SymbolsPath']
$TempPath = "/tmp/symbols"

if (-not $ServerIP -or -not $ServerUser -or -not $SymbolsPath) {
    Write-Host "[SymbolServer] Config error - ServerIP, ServerUser, SymbolsPath missing"
    exit 1
}

# SSH key path
$KeyPath = Join-Path $ScriptDir "symbol-upload.key"

# Check key file - skip upload if not found
if (-not (Test-Path $KeyPath)) {
    Write-Host "[SymbolServer] SSH key not found - skipping upload (keeping local PDB)"
    exit 0
}

# SSH/SCP common options (timeout in seconds)
$SshOptions = "-o ConnectTimeout=10 -o ServerAliveInterval=5 -o ServerAliveCountMax=2 -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL"

# Check for uncommitted changes - skip upload if dirty
$ProjectRoot = (Get-Item $ScriptDir).Parent.Parent.FullName
$GitStatus = git -C $ProjectRoot status --porcelain 2>&1
if ($GitStatus) {
    Write-Host "[SymbolServer] Uncommitted changes detected - skipping upload (keeping local PDB)"
    Write-Host "[SymbolServer] Commit your changes first to upload symbols"
    exit 0
}

# Search for PDB files
$PdbFiles = Get-ChildItem -Path $OutDir -Filter "*.pdb" -File -ErrorAction SilentlyContinue

if ($PdbFiles.Count -eq 0) {
    Write-Host "[SymbolServer] No PDB files found: $OutDir"
    exit 0
}

Write-Host "[SymbolServer] Found $($PdbFiles.Count) PDB files"

# Search for binary files (EXE, DLL)
$BinaryFiles = Get-ChildItem -Path "$OutDir\*" -Include "*.exe","*.dll" -File -ErrorAction SilentlyContinue
Write-Host "[SymbolServer] Found $($BinaryFiles.Count) binary files (EXE/DLL)"

# Test server connection first
$TestResult = ssh -i "$KeyPath" $SshOptions.Split(' ') ${ServerUser}@${ServerIP} "echo ok" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[SymbolServer] Server connection failed - skipping upload (keeping local PDB)"
    Write-Host "[SymbolServer] Error: $TestResult"
    exit 0
}

# Create temp directory on server
ssh -i "$KeyPath" $SshOptions.Split(' ') ${ServerUser}@${ServerIP} "mkdir -p $TempPath" 2>$null

$UploadedCount = 0
$FailedCount = 0

# Upload PDB files
foreach ($Pdb in $PdbFiles) {
    $PdbName = $Pdb.Name
    $PdbPath = $Pdb.FullName

    Write-Host "[SymbolServer] Uploading PDB: $PdbName"

    # Upload PDB to server
    $ScpResult = scp -i "$KeyPath" $SshOptions.Split(' ') -q "$PdbPath" "${ServerUser}@${ServerIP}:${TempPath}/" 2>&1

    if ($LASTEXITCODE -eq 0) {
        # Index with symstore on server
        $SshResult = ssh -i "$KeyPath" $SshOptions.Split(' ') ${ServerUser}@${ServerIP} "/usr/local/bin/symstore -s $SymbolsPath ${TempPath}/${PdbName} && rm ${TempPath}/${PdbName}" 2>&1

        if ($LASTEXITCODE -eq 0) {
            Write-Host "[SymbolServer] Indexed: $PdbName"

            # Delete local PDB
            Remove-Item -Path $PdbPath -Force
            Write-Host "[SymbolServer] Deleted local: $PdbName"

            $UploadedCount++
        } else {
            Write-Host "[SymbolServer] Indexing failed: $PdbName"
            Write-Host $SshResult
            $FailedCount++
        }
    } else {
        Write-Host "[SymbolServer] Upload failed: $PdbName"
        Write-Host $ScpResult
        $FailedCount++
    }
}

# Upload binary files (EXE, DLL) - do NOT delete local copies
$BinaryUploadedCount = 0
$BinaryFailedCount = 0

foreach ($Binary in $BinaryFiles) {
    $BinaryName = $Binary.Name
    $BinaryPath = $Binary.FullName

    Write-Host "[SymbolServer] Uploading binary: $BinaryName"

    # Upload binary to server
    $ScpResult = scp -i "$KeyPath" $SshOptions.Split(' ') -q "$BinaryPath" "${ServerUser}@${ServerIP}:${TempPath}/" 2>&1

    if ($LASTEXITCODE -eq 0) {
        # Index with symstore on server
        $SshResult = ssh -i "$KeyPath" $SshOptions.Split(' ') ${ServerUser}@${ServerIP} "/usr/local/bin/symstore -s $SymbolsPath ${TempPath}/${BinaryName} && rm ${TempPath}/${BinaryName}" 2>&1

        if ($LASTEXITCODE -eq 0) {
            Write-Host "[SymbolServer] Indexed: $BinaryName"
            # NOTE: Do NOT delete local binary - needed for execution
            $BinaryUploadedCount++
        } else {
            Write-Host "[SymbolServer] Indexing failed: $BinaryName"
            Write-Host $SshResult
            $BinaryFailedCount++
        }
    } else {
        Write-Host "[SymbolServer] Upload failed: $BinaryName"
        Write-Host $ScpResult
        $BinaryFailedCount++
    }
}

# Clean up server temp directory
ssh -i "$KeyPath" $SshOptions.Split(' ') ${ServerUser}@${ServerIP} "rmdir $TempPath 2>/dev/null" 2>$null

# Delete sourcelink.json (already embedded in PDB)
$SourcelinkPath = Join-Path $OutDir "sourcelink.json"
if (Test-Path $SourcelinkPath) {
    Remove-Item -Path $SourcelinkPath -Force
    Write-Host "[SymbolServer] Deleted: sourcelink.json"
}

Write-Host "[SymbolServer] Complete: PDB $UploadedCount uploaded/$FailedCount failed, Binary $BinaryUploadedCount uploaded/$BinaryFailedCount failed"

$TotalFailed = $FailedCount + $BinaryFailedCount
if ($TotalFailed -gt 0) {
    exit 1
}

exit 0
