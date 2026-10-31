Add-Type -AssemblyName System.Drawing
$imagePath = "C:\Users\ipiou\Desktop\pcb ai\ZarqaaOS\desktop\images\Mountain_Minimalist_Artistic_Landscape.jpg"
$bmp = New-Object System.Drawing.Bitmap($imagePath)
$resized = New-Object System.Drawing.Bitmap($bmp, 1024, 768)

$outFile = "C:\Users\ipiou\Desktop\pcb ai\ZarqaaOS\include\gui\wallpaper.h"
$writer = [System.IO.StreamWriter]::new($outFile)
$writer.WriteLine("#ifndef WALLPAPER_H")
$writer.WriteLine("#define WALLPAPER_H")
$writer.WriteLine("#include <stdint.h>")
$writer.WriteLine("static const uint32_t wallpaper_data[1024 * 768] = {")

$rect = New-Object System.Drawing.Rectangle(0, 0, 1024, 768)
$bmpData = $resized.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

$bytes = 1024 * 768 * 4
$ptr = $bmpData.Scan0
$buffer = New-Object byte[] $bytes
[System.Runtime.InteropServices.Marshal]::Copy($ptr, $buffer, 0, $bytes)
$resized.UnlockBits($bmpData)

$sb = New-Object System.Text.StringBuilder
$sb.Capacity = $bytes * 3

for ($i = 0; $i -lt $bytes; $i += 4) {
    $b = $buffer[$i]
    $g = $buffer[$i+1]
    $r = $buffer[$i+2]
    $a = 255
    $val = [uint32](([uint32]$a -shl 24) -bor ([uint32]$r -shl 16) -bor ([uint32]$g -shl 8) -bor [uint32]$b)
    [void]$sb.AppendFormat("0x{0:X8},", $val)
    if (($i % 64) -eq 60) {
        [void]$sb.AppendLine()
    }
}
$writer.Write($sb.ToString())
$writer.WriteLine("};")
$writer.WriteLine("#endif")
$writer.Close()
