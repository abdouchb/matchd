<#
.SYNOPSIS
    Writes a Zarqaa OS ISO to a USB stick and verifies the copy.
.DESCRIPTION
    The ISO is a hybrid image (MBR + GPT, BIOS + UEFI), so it is written raw,
    byte for byte (like Rufus "DD mode" or balenaEtcher). Only USB disks that
    are not the Windows system/boot disk are offered, and nothing is written
    until you type ERASE. After writing, the stick is read back and compared
    with the ISO's SHA-256.
.PARAMETER Iso
    ISO to write. Defaults to the newest dist\ZarqaaOS-*.iso.
#>
[CmdletBinding()]
param([string]$Iso)

$ErrorActionPreference = 'Stop'

function Read-Full([IO.Stream]$Stream, [byte[]]$Buffer, [int]$Count) {
    $total = 0
    while ($total -lt $Count) {
        $n = $Stream.Read($Buffer, $total, $Count - $total)
        if ($n -le 0) { break }
        $total += $n
    }
    return $total
}

function Write-RawImage {
    param([string]$Source, [string]$Target, [int]$SectorSize = 512, [int]$ChunkSize = 4MB)

    $src = [IO.File]::OpenRead($Source)
    $dst = New-Object IO.FileStream($Target, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite,
                                    [IO.FileShare]::ReadWrite, 4096, [IO.FileOptions]::WriteThrough)
    try {
        $length = $src.Length
        $buffer = New-Object byte[] $ChunkSize

        # The first chunk holds the partition tables. Writing it last stops
        # Windows from mounting the new volumes (and locking them) mid-write.
        $offsets = New-Object System.Collections.Generic.List[long]
        for ([long]$off = $ChunkSize; $off -lt $length; $off += $ChunkSize) { $offsets.Add($off) }
        $offsets.Add(0)

        [long]$written = 0
        foreach ($off in $offsets) {
            $want = [int][Math]::Min([long]$ChunkSize, $length - $off)
            $src.Position = $off
            $got = Read-Full $src $buffer $want
            if ($got -ne $want) { throw "Short read from ISO at offset $off" }

            # Raw disk writes must be whole sectors
            $padded = [int]([Math]::Ceiling($got / $SectorSize) * $SectorSize)
            if ($padded -gt $got) { [Array]::Clear($buffer, $got, $padded - $got) }

            $dst.Position = $off
            $dst.Write($buffer, 0, $padded)
            $written += $got
            Write-Progress -Activity 'Writing Zarqaa OS to USB' -Status ('{0:N0} / {1:N0} MB' -f ($written / 1MB), ($length / 1MB)) `
                           -PercentComplete ([int](100 * $written / $length))
        }
        $dst.Flush($true)
    }
    finally {
        Write-Progress -Activity 'Writing Zarqaa OS to USB' -Completed
        $src.Dispose()
        $dst.Dispose()
    }
}

function Test-RawImage {
    param([string]$Source, [string]$Target, [int]$SectorSize = 512, [int]$ChunkSize = 4MB)

    $length = (Get-Item -LiteralPath $Source).Length
    $expected = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash
    $sha = [Security.Cryptography.SHA256]::Create()
    $dst = New-Object IO.FileStream($Target, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $buffer = New-Object byte[] $ChunkSize
        for ([long]$off = 0; $off -lt $length; $off += $ChunkSize) {
            $want = [int][Math]::Min([long]$ChunkSize, $length - $off)
            $aligned = [int]([Math]::Ceiling($want / $SectorSize) * $SectorSize)
            $dst.Position = $off
            $got = Read-Full $dst $buffer $aligned
            if ($got -lt $want) { return $false }
            [void]$sha.TransformBlock($buffer, 0, $want, $null, 0)
            Write-Progress -Activity 'Verifying USB' -PercentComplete ([int](100 * ($off + $want) / $length))
        }
        [void]$sha.TransformFinalBlock((New-Object byte[] 0), 0, 0)
        $actual = -join ($sha.Hash | ForEach-Object { $_.ToString('X2') })
        return $actual -eq $expected
    }
    finally {
        Write-Progress -Activity 'Verifying USB' -Completed
        $dst.Dispose()
        $sha.Dispose()
    }
}

# Dot-sourcing (". .\burn_usb.ps1") only loads the functions above
if ($MyInvocation.InvocationName -eq '.') { return }

function Wait-Close { Read-Host "`nPress Enter to close" | Out-Null }

try {
    $root = Split-Path -Parent $PSScriptRoot
    if (-not $Iso) {
        $latest = Get-ChildItem -Path (Join-Path $root 'dist') -Filter 'ZarqaaOS-*.iso' -ErrorAction SilentlyContinue |
                  Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (-not $latest) { throw 'No ISO found in dist\. Run make_iso.bat first.' }
        $Iso = $latest.FullName
    }
    $Iso = (Resolve-Path -LiteralPath $Iso).Path

    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Host 'Writing to a USB disk needs administrator rights - asking Windows...'
        $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Iso `"$Iso`""
        Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList $argList -Wait
        return
    }

    Write-Host "`n=== Zarqaa OS USB maker ===" -ForegroundColor Cyan
    $isoSize = (Get-Item -LiteralPath $Iso).Length
    Write-Host ("ISO: {0}  ({1:N1} MB)" -f (Split-Path -Leaf $Iso), ($isoSize / 1MB))

    $sidecar = "$Iso.sha256"
    if (Test-Path -LiteralPath $sidecar) {
        $want = ((Get-Content -LiteralPath $sidecar -TotalCount 1) -split '\s+')[0]
        $have = (Get-FileHash -LiteralPath $Iso -Algorithm SHA256).Hash
        if ($want -ne $have) { throw 'The ISO is damaged (checksum mismatch). Run make_iso.bat again.' }
        Write-Host 'ISO checksum OK' -ForegroundColor Green
    }

    $disk = $null
    while (-not $disk) {
        $disks = @(Get-Disk | Where-Object {
            $_.BusType -eq 'USB' -and -not $_.IsBoot -and -not $_.IsSystem -and $_.Size -ge $isoSize
        } | Sort-Object Number)

        if ($disks.Count -eq 0) {
            $answer = Read-Host "`nNo USB stick found. Plug one in and press Enter to rescan (Q = quit)"
            if ($answer -match '^[qQ]') { Wait-Close; return }
            continue
        }

        Write-Host "`nUSB disks:"
        foreach ($d in $disks) {
            $letters = (Get-Partition -DiskNumber $d.Number -ErrorAction SilentlyContinue |
                        Where-Object { [int]$_.DriveLetter -gt 0 } |
                        ForEach-Object { "$($_.DriveLetter):" }) -join ' '
            Write-Host ('  [{0}]  {1,-34} {2,8:N1} GB   {3}' -f $d.Number, $d.FriendlyName, ($d.Size / 1GB), $letters)
        }

        $answer = Read-Host "`nDisk number to write to (R = rescan, Q = quit)"
        if ($answer -match '^[qQ]') { Wait-Close; return }
        if ($answer -match '^\d+$') {
            $disk = $disks | Where-Object { $_.Number -eq [int]$answer } | Select-Object -First 1
            if (-not $disk) { Write-Host 'That number is not in the list.' -ForegroundColor Yellow }
        }
    }

    if ($disk.Size -gt 256GB) {
        Write-Host 'Warning: this disk is larger than 256 GB - it may be an external hard drive, not a USB stick.' -ForegroundColor Yellow
    }
    Write-Host ("`nALL DATA on disk {0} ({1}, {2:N1} GB) WILL BE ERASED." -f $disk.Number, $disk.FriendlyName, ($disk.Size / 1GB)) -ForegroundColor Red
    if ((Read-Host 'Type ERASE to continue') -cne 'ERASE') { Write-Host 'Cancelled - nothing was written.'; Wait-Close; return }

    # Guard against the stick being swapped while the prompt was open
    $now = Get-Disk -Number $disk.Number
    if ($now.UniqueId -ne $disk.UniqueId -or $now.Size -ne $disk.Size -or $now.BusType -ne 'USB' -or $now.IsBoot -or $now.IsSystem) {
        throw 'The selected disk changed. Nothing was written - please run again.'
    }

    if ($now.IsOffline)  { Set-Disk -Number $now.Number -IsOffline $false }
    if ($now.IsReadOnly) { Set-Disk -Number $now.Number -IsReadOnly $false }
    if ($now.PartitionStyle -ne 'RAW') {
        # Removing the partitions makes Windows release the stick's volumes
        Clear-Disk -Number $now.Number -RemoveData -RemoveOEM -Confirm:$false
    }

    $target = "\\.\PhysicalDrive$($now.Number)"
    $sector = [int]$now.LogicalSectorSize
    if ($sector -le 0) { $sector = 512 }

    $timer = [Diagnostics.Stopwatch]::StartNew()
    Write-Host "`nWriting..."
    Write-RawImage -Source $Iso -Target $target -SectorSize $sector
    Write-Host ('Written in {0:N1} s' -f $timer.Elapsed.TotalSeconds)

    Write-Host 'Verifying...'
    if (-not (Test-RawImage -Source $Iso -Target $target -SectorSize $sector)) {
        throw 'Verification FAILED: the USB stick does not match the ISO. Try another stick or port.'
    }
    Update-Disk -Number $now.Number -ErrorAction SilentlyContinue

    Write-Host "`nDone - the USB stick is verified and bootable." -ForegroundColor Green
    Write-Host 'To boot it: open the PC boot menu (usually F12, F11, F8 or Esc) and pick the USB stick.'
    Write-Host 'If it does not start, turn off Secure Boot in the BIOS/UEFI settings (the Limine loader is not signed).'
    Wait-Close
}
catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    Wait-Close
    exit 1
}
