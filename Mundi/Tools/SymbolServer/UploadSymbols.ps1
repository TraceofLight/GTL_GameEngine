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

# Search for PDB files
$PdbFiles = Get-ChildItem -Path $OutDir -Filter "*.pdb" -File -ErrorAction SilentlyContinue

if ($PdbFiles.Count -eq 0) {
    Write-Host "[SymbolServer] No PDB files found: $OutDir"
    exit 0
}

Write-Host "[SymbolServer] Found $($PdbFiles.Count) PDB files"

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

foreach ($Pdb in $PdbFiles) {
    $PdbName = $Pdb.Name
    $PdbPath = $Pdb.FullName

    Write-Host "[SymbolServer] Uploading: $PdbName"

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

# Clean up server temp directory
ssh -i "$KeyPath" $SshOptions.Split(' ') ${ServerUser}@${ServerIP} "rmdir $TempPath 2>/dev/null" 2>$null

# Delete sourcelink.json (already embedded in PDB)
$SourcelinkPath = Join-Path $OutDir "sourcelink.json"
if (Test-Path $SourcelinkPath) {
    Remove-Item -Path $SourcelinkPath -Force
    Write-Host "[SymbolServer] Deleted: sourcelink.json"
}

Write-Host "[SymbolServer] Complete: $UploadedCount uploaded, $FailedCount failed"

if ($FailedCount -gt 0) {
    exit 1
}

exit 0
