# ITLabControl - kiểm tra (và bật, với -Fix) các thiết lập Windows cần cho "Bật máy" (Wake-on-LAN)
# Gọi từ KIEM-TRA-BAT-MAY.cmd. Không chỉnh được BIOS/UEFI: phần đó IT phải làm trên từng máy.
param(
	[string]$OutFile = "$PSScriptRoot\KIEM-TRA-BAT-MAY-$env:COMPUTERNAME.txt",
	[switch]$Fix
)

$lines = New-Object System.Collections.Generic.List[string]
function Say([string]$text) { $lines.Add($text); Write-Host $text }

if ($Fix) {
	Get-NetAdapter -Physical | ForEach-Object {
		$n = $_.Name
		Set-NetAdapterPowerManagement -Name $n -WakeOnMagicPacket Enabled -ErrorAction SilentlyContinue
		foreach ($k in '*WakeOnMagicPacket', 'S5WakeOnLan', 'EnablePME') {
			Set-NetAdapterAdvancedProperty -Name $n -RegistryKeyword $k -RegistryValue 1 -NoRestart -ErrorAction SilentlyContinue
		}
		powercfg -deviceenablewake $_.InterfaceDescription 2>$null | Out-Null
	}
	reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Power" /v HiberbootEnabled /t REG_DWORD /d 0 /f | Out-Null
	Say "=== ĐÃ BẬT các thiết lập Wake-on-LAN (cần khởi động lại máy) ==="
}

Say "Máy: $env:COMPUTERNAME   Thời điểm: $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
$cs = Get-CimInstance Win32_ComputerSystem
Say "Loại máy: $($cs.Manufacturer) $($cs.Model)   (PCSystemType=$($cs.PCSystemType): 1=máy bàn, 2=laptop)"

$battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue
if ($battery) {
	$ac = if ($battery.BatteryStatus -eq 2) { 'đang cắm sạc' } else { 'ĐANG CHẠY PIN' }
	Say "Pin: $ac. Laptop thường chỉ bật từ xa được khi CẮM SẠC."
}

$hiber = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' -Name HiberbootEnabled -ErrorAction SilentlyContinue).HiberbootEnabled
Say ("Fast startup: " + $(if ($hiber -eq 0) { 'đã TẮT (tốt)' } else { 'đang BẬT -> nên tắt' }))
Say ""

$route = Get-NetRoute -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue | Sort-Object RouteMetric | Select-Object -First 1
foreach ($a in Get-NetAdapter -Physical) {
	$used = if ($route -and $route.InterfaceIndex -eq $a.ifIndex) { '  <== card đang dùng để kết nối mạng' } else { '' }
	Say "=== Card: $($a.Name) | $($a.InterfaceDescription) | MAC $($a.MacAddress) | $($a.Status)$used"

	$kind = if ($a.InterfaceDescription -match 'USB|Dock|ThinkPad|Thunderbolt') { 'card USB/dock' }
	        elseif ($a.InterfaceDescription -match 'Wi-?Fi|Wireless|802\.11') { 'Wi-Fi' }
	        else { 'card gắn trong máy' }
	Say "    Loại: $kind"
	if ($kind -eq 'card USB/dock') { Say "    !!! Card USB/dock thường KHÔNG bật được máy sau khi Shut down (máy tắt thì cổng USB mất điện)." }
	if ($kind -eq 'Wi-Fi') { Say "    !!! Qua Wi-Fi gần như không bật máy từ xa được; hãy dùng dây mạng." }

	$pm = Get-NetAdapterPowerManagement -Name $a.Name -ErrorAction SilentlyContinue
	if ($pm) { Say "    Wake on Magic Packet (quản lý nguồn): $($pm.WakeOnMagicPacket)" }
	Get-NetAdapterAdvancedProperty -Name $a.Name -ErrorAction SilentlyContinue |
		Where-Object { $_.RegistryKeyword -match 'WakeOnMagicPacket|S5WakeOnLan|EnablePME|WolShutdown' } |
		ForEach-Object { Say "    $($_.DisplayName) [$($_.RegistryKeyword)] = $($_.DisplayValue)" }
}

Say ""
Say "Thiết bị được phép đánh thức máy (powercfg):"
(powercfg -devicequery wake_armed) | ForEach-Object { Say "    $_" }
Say ""
Say "Nhắc lại: trong BIOS/UEFI cần bật 'Wake on LAN' / 'Power On by PCI-E', tắt 'ErP' / 'Deep Sleep'."

$lines | Out-File -FilePath $OutFile -Encoding utf8
