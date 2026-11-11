# ITLabControl - vẽ lại các ảnh thương hiệu (thay ảnh có chữ "Veyon 4" của bản gốc)
#
#   master/resources/splash.png  : màn hình khởi động Bảng điều khiển giáo viên (640x320)
#   nsis/welcome-page.bmp        : ảnh dọc trang chào mừng/kết thúc của bộ cài (164x314)
#   nsis/header.bmp              : ảnh đầu các trang của bộ cài (150x57)
#
# Cách dùng (PowerShell trên Windows, từ thư mục gốc của repo):
#   powershell -ExecutionPolicy Bypass -File scripts\branding\make-branding-images.ps1
#
# Lưu ý: file này phải lưu dạng UTF-8 CÓ BOM để PowerShell 5.1 đọc đúng tiếng Việt.

param([string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..\..").Path)

Add-Type -AssemblyName System.Drawing

$teal = [System.Drawing.Color]::FromArgb(0x19, 0x8C, 0xB3)   # màu thanh xanh của splash gốc
$dark = [System.Drawing.Color]::FromArgb(0x54, 0x54, 0x54)
$grey = [System.Drawing.Color]::FromArgb(0x8A, 0x8A, 0x8A)
$product = 'ITLabControl'

function New-Canvas([int]$w, [int]$h) {
	$bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	$g.SmoothingMode = 'AntiAlias'
	$g.TextRenderingHint = 'AntiAliasGridFit'
	$g.Clear([System.Drawing.Color]::White)
	return $bmp, $g
}

function Font([string]$name, [float]$px) {
	New-Object System.Drawing.Font $name, $px, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
}

function Draw-Eye($g, [float]$cx, [float]$cy, [float]$w) {
	$pen = New-Object System.Drawing.Pen $dark, ([Math]::Max(2, $w / 17))
	$g.DrawEllipse($pen, $cx - $w / 2, $cy - $w / 4, $w, $w / 2)
	$r = $w / 6.5
	$g.FillEllipse((New-Object System.Drawing.SolidBrush $dark), $cx - $r, $cy - $r, 2 * $r, 2 * $r)
}

$center = New-Object System.Drawing.StringFormat
$center.Alignment = 'Center'; $center.LineAlignment = 'Center'

# ───── 1. splash.png (640x320) ─────
$W = 640; $H = 320; $bar = 32
$bmp, $g = New-Canvas $W $H
$tealBrush = New-Object System.Drawing.SolidBrush $teal
$g.FillRectangle($tealBrush, 0, 0, $W, $bar)
$g.FillRectangle($tealBrush, 0, $H - $bar, $W, $bar)
Draw-Eye $g ($W / 2) 75 52
$g.DrawString($product, (Font 'Segoe UI Light' 50), (New-Object System.Drawing.SolidBrush $dark), (New-Object System.Drawing.RectangleF 0, 95, $W, 80), $center)
$g.DrawString('Quản lý phòng máy tin học', (Font 'Segoe UI' 17), (New-Object System.Drawing.SolidBrush $grey), (New-Object System.Drawing.RectangleF 0, 172, $W, 30), $center)
$g.DrawString('Phát triển từ mã nguồn mở Veyon', (Font 'Segoe UI' 13), (New-Object System.Drawing.SolidBrush $grey), (New-Object System.Drawing.RectangleF 0, 222, $W, 24), $center)
$left = New-Object System.Drawing.StringFormat; $left.LineAlignment = 'Center'
$right = New-Object System.Drawing.StringFormat; $right.Alignment = 'Far'; $right.LineAlignment = 'Center'
$footRect = New-Object System.Drawing.RectangleF 13, ($H - $bar), ($W - 26), $bar
$g.DrawString('© 2026 ITLabControl', (Font 'Segoe UI' 13), [System.Drawing.Brushes]::White, $footRect, $left)
$g.DrawString('github.com/minhnb04ict/itlabcontrol', (Font 'Segoe UI' 13), [System.Drawing.Brushes]::White, $footRect, $right)
$g.Dispose()
$bmp.Save("$RepoRoot\master\resources\splash.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# ───── 2. welcome-page.bmp (164x314): chữ dọc, đọc từ dưới lên như bản gốc ─────
$W = 164; $H = 314
$bmp, $g = New-Canvas $W $H
$g.FillRectangle((New-Object System.Drawing.SolidBrush $teal), 0, 0, 8, $H)
$g.TranslateTransform(($W / 2 + 6), ($H / 2 - 18))
$g.RotateTransform(-90)
$g.DrawString($product, (Font 'Segoe UI Light' 40), (New-Object System.Drawing.SolidBrush $dark), (New-Object System.Drawing.RectangleF (-$H / 2), -32, $H, 64), $center)
$g.ResetTransform()
Draw-Eye $g ($W / 2 + 4) ($H - 28) 40
$g.Dispose()
$bmp.Save("$RepoRoot\nsis\welcome-page.bmp", [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()

# ───── 3. header.bmp (150x57) ─────
$W = 150; $H = 57
$bmp, $g = New-Canvas $W $H
$g.DrawString($product, (Font 'Segoe UI Light' 23), (New-Object System.Drawing.SolidBrush $dark), (New-Object System.Drawing.RectangleF 0, 0, $W, $H), $center)
$g.Dispose()
$bmp.Save("$RepoRoot\nsis\header.bmp", [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()

"Da tao: splash.png, welcome-page.bmp, header.bmp"
