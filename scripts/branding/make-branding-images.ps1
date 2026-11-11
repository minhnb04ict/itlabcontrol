# ITLabControl - tạo toàn bộ ảnh thương hiệu từ logo scripts/branding/itlab-logo.png
#
#   master/resources/splash.png             màn hình khởi động Bảng điều khiển giáo viên (640x320)
#   master/data/veyon-master.ico            icon file .exe / shortcut Bảng điều khiển giáo viên
#   configurator/data/veyon-configurator.ico icon file .exe / shortcut Configurator
#   configurator/resources/veyon-configurator.png  icon cửa sổ Configurator (128x128)
#   core/resources/icon16|22|32|64.png      icon cửa sổ, khay hệ thống, hộp thoại Giới thiệu
#   nsis/installer.ico, nsis/uninstaller.ico icon bộ cài / gỡ cài đặt
#   nsis/welcome-page.bmp (164x314), nsis/header.bmp (150x57)  ảnh của bộ cài
#
# Cách dùng (PowerShell trên Windows, từ thư mục gốc của repo):
#   powershell -ExecutionPolicy Bypass -File scripts\branding\make-branding-images.ps1
#
# Đổi logo: thay file scripts/branding/itlab-logo.png (PNG nền trong suốt) rồi chạy lại script.
# Lưu ý: file này phải lưu dạng UTF-8 CÓ BOM để PowerShell 5.1 đọc đúng tiếng Việt.

param([string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..\..").Path)

Add-Type -AssemblyName System.Drawing

$teal = [System.Drawing.Color]::FromArgb(0x19, 0x8C, 0xB3)
$dark = [System.Drawing.Color]::FromArgb(0x33, 0x3D, 0x4F)
$grey = [System.Drawing.Color]::FromArgb(0x8A, 0x8A, 0x8A)
$product = 'ITLabControl'

# ───── logo: cắt sát phần có hình (bỏ viền trong suốt) ─────
$source = [System.Drawing.Bitmap]::FromFile("$PSScriptRoot\itlab-logo.png")
$minX = $source.Width; $minY = $source.Height; $maxX = 0; $maxY = 0
for ($y = 0; $y -lt $source.Height; $y += 2) {
	for ($x = 0; $x -lt $source.Width; $x += 2) {
		if ($source.GetPixel($x, $y).A -gt 20) {
			if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
			if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
		}
	}
}
$crop = New-Object System.Drawing.Rectangle $minX, $minY, ($maxX - $minX + 2), ($maxY - $minY + 2)
$logo = $source.Clone($crop, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$source.Dispose()

function New-Graphics($bmp) {
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	$g.SmoothingMode = 'AntiAlias'
	$g.InterpolationMode = 'HighQualityBicubic'
	$g.PixelOffsetMode = 'HighQuality'
	$g.TextRenderingHint = 'AntiAliasGridFit'
	return $g
}

# vẽ logo vừa khít trong khung (giữ tỉ lệ, căn giữa)
function Draw-Logo($g, [float]$x, [float]$y, [float]$w, [float]$h) {
	$scale = [Math]::Min($w / $logo.Width, $h / $logo.Height)
	$dw = $logo.Width * $scale; $dh = $logo.Height * $scale
	$g.DrawImage($logo, (New-Object System.Drawing.RectangleF ($x + ($w - $dw) / 2), ($y + ($h - $dh) / 2), $dw, $dh))
}

function Font([string]$name, [float]$px, $style = [System.Drawing.FontStyle]::Regular) {
	New-Object System.Drawing.Font $name, $px, $style, ([System.Drawing.GraphicsUnit]::Pixel)
}

# icon vuông nền trong suốt, kích thước $size
function New-IconBitmap([int]$size) {
	$bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
	$g = New-Graphics $bmp
	$g.Clear([System.Drawing.Color]::Transparent)
	$pad = [Math]::Max(0, [int]($size * 0.02))
	Draw-Logo $g $pad $pad ($size - 2 * $pad) ($size - 2 * $pad)
	$g.Dispose()
	return $bmp
}

function Save-Png($bmp, [string]$path) { $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png) }

# file .ico nhiều kích thước, mỗi ảnh lưu dạng PNG (Windows Vista trở lên)
function Save-Ico([string]$path, [int[]]$sizes) {
	$images = @()
	foreach ($s in $sizes) {
		$bmp = New-IconBitmap $s
		$ms = New-Object System.IO.MemoryStream
		$bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
		$images += , @($s, $ms.ToArray())
		$bmp.Dispose()
	}
	$out = New-Object System.IO.MemoryStream
	$w = New-Object System.IO.BinaryWriter $out
	$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$images.Count)
	$offset = 6 + 16 * $images.Count
	foreach ($img in $images) {
		$s = $img[0]; $data = $img[1]
		$dim = if ($s -ge 256) { 0 } else { $s }
		$w.Write([Byte]$dim); $w.Write([Byte]$dim); $w.Write([Byte]0); $w.Write([Byte]0)
		$w.Write([UInt16]1); $w.Write([UInt16]32)
		$w.Write([UInt32]$data.Length); $w.Write([UInt32]$offset)
		$offset += $data.Length
	}
	foreach ($img in $images) { $w.Write([byte[]]$img[1]) }
	$w.Flush()
	[System.IO.File]::WriteAllBytes($path, $out.ToArray())
	$w.Dispose()
}

$center = New-Object System.Drawing.StringFormat
$center.Alignment = 'Center'; $center.LineAlignment = 'Center'

# ───── 1. splash.png (640x320) ─────
$W = 640; $H = 320; $bar = 32
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = New-Graphics $bmp
$g.Clear([System.Drawing.Color]::White)
$tealBrush = New-Object System.Drawing.SolidBrush $teal
$g.FillRectangle($tealBrush, 0, 0, $W, $bar)
$g.FillRectangle($tealBrush, 0, $H - $bar, $W, $bar)
Draw-Logo $g 0 ($bar + 12) $W 150
$g.DrawString($product, (Font 'Segoe UI Semibold' 34), (New-Object System.Drawing.SolidBrush $dark), (New-Object System.Drawing.RectangleF 0, 205, $W, 44), $center)
$g.DrawString('Quản lý phòng máy tin học  ·  Phát triển từ mã nguồn mở Veyon', (Font 'Segoe UI' 14), (New-Object System.Drawing.SolidBrush $grey), (New-Object System.Drawing.RectangleF 0, 248, $W, 24), $center)
$left = New-Object System.Drawing.StringFormat; $left.LineAlignment = 'Center'
$right = New-Object System.Drawing.StringFormat; $right.Alignment = 'Far'; $right.LineAlignment = 'Center'
$footRect = New-Object System.Drawing.RectangleF 13, ($H - $bar), ($W - 26), $bar
$g.DrawString('© 2026 ITLabControl', (Font 'Segoe UI' 13), [System.Drawing.Brushes]::White, $footRect, $left)
$g.DrawString('github.com/minhnb04ict/itlabcontrol', (Font 'Segoe UI' 13), [System.Drawing.Brushes]::White, $footRect, $right)
$g.Dispose()
Save-Png $bmp "$RepoRoot\master\resources\splash.png"
$bmp.Dispose()

# ───── 2. icon ứng dụng ─────
$icoSizes = @(16, 24, 32, 48, 64, 128, 256)
Save-Ico "$RepoRoot\master\data\veyon-master.ico" $icoSizes
Save-Ico "$RepoRoot\configurator\data\veyon-configurator.ico" $icoSizes
Save-Ico "$RepoRoot\nsis\installer.ico" $icoSizes
Save-Ico "$RepoRoot\nsis\uninstaller.ico" $icoSizes
foreach ($s in 16, 22, 32, 64) { $b = New-IconBitmap $s; Save-Png $b "$RepoRoot\core\resources\icon$s.png"; $b.Dispose() }
$b = New-IconBitmap 128; Save-Png $b "$RepoRoot\configurator\resources\veyon-configurator.png"; $b.Dispose()

# ───── 3. welcome-page.bmp (164x314) ─────
$W = 164; $H = 314
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = New-Graphics $bmp
$g.Clear([System.Drawing.Color]::White)
$g.FillRectangle((New-Object System.Drawing.SolidBrush $teal), 0, 0, 6, $H)
Draw-Logo $g 14 70 140 110
$g.DrawString($product, (Font 'Segoe UI Semibold' 19), (New-Object System.Drawing.SolidBrush $dark), (New-Object System.Drawing.RectangleF 6, 186, ($W - 6), 30), $center)
$g.DrawString("Quản lý phòng máy tin học", (Font 'Segoe UI' 12), (New-Object System.Drawing.SolidBrush $grey), (New-Object System.Drawing.RectangleF 6, 214, ($W - 6), 22), $center)
$g.Dispose()
$bmp.Save("$RepoRoot\nsis\welcome-page.bmp", [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()

# ───── 4. header.bmp (150x57) ─────
$W = 150; $H = 57
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = New-Graphics $bmp
$g.Clear([System.Drawing.Color]::White)
Draw-Logo $g 4 3 142 51
$g.Dispose()
$bmp.Save("$RepoRoot\nsis\header.bmp", [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()

$logo.Dispose()
"Da tao: splash.png, 4 file .ico, icon16/22/32/64.png, veyon-configurator.png, welcome-page.bmp, header.bmp"
