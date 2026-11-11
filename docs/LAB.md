# Kiểm thử ITLabControl

Có 2 cách, dùng cho 2 mục đích khác nhau. **Không cần máy ảo.**

| Cách | Dùng khi | Thử được |
|---|---|---|
| **1. Trên máy phát triển (localhost)** | Hằng ngày, sau mỗi lần sửa code | Xem màn hình, điều khiển, khoá, tin nhắn, gửi/thu file, các plugin mới |
| **2. Trên máy thật trong phòng máy** | Cuối mỗi giai đoạn, trước khi duyệt | Nhiều máy, 31 máy HS, khác subnet, Wake-on-LAN, hiệu năng mạng |

---

## Cách 1: Kiểm thử trên máy phát triển

Console giáo viên kết nối tới **chính máy đang chạy** qua `127.0.0.1`.

### 1.1 Cài đặt (sau mỗi lần build)

```bat
:: chạy trong cmd "Run as administrator"
D:\Projects\itlab-build\itlabcontrol-4.11.4.0-win64-setup.exe /S /NoInterception
```

- **Đóng Teacher Console và Configurator trước khi cài.** Nếu đang mở, chúng giữ các file plugin,
  bộ cài ở chế độ im lặng **không ghi đè được và cũng không báo lỗi**, nên máy vẫn chạy bản cũ.
  Kiểm tra: so sánh `Get-FileHash` của plugin trong `C:\Program Files\ITLabControl\plugins` với bản vừa build.
- `/S`: cài im lặng, không hiện giao diện. Cài đè lên bản cũ được, giữ nguyên cấu hình.
- `/NoInterception`: **không** cài driver bàn phím/chuột trên máy phát triển.

### 1.2 Cấu hình lần đầu (chỉ làm 1 lần)

```bat
cd /d "C:\Program Files\ITLabControl"
veyon-cli authkeys create teacher
veyon-cli authkeys setaccessgroup teacher/private Users
veyon-cli config set Authentication/Method 1
veyon-cli networkobjects add location "Phong Thu Nghiem"
veyon-cli networkobjects add computer "May GV (localhost)" 127.0.0.1 "" "Phong Thu Nghiem"
veyon-cli service restart
```

> Chạy các lệnh trên trong **cmd**, không chạy trong PowerShell: PowerShell 5.1 bỏ qua tham số rỗng `""`.

| Thành phần | Vị trí |
|---|---|
| Chương trình | `C:\Program Files\ITLabControl` |
| Cấu hình | Registry `HKLM\SOFTWARE\ITLabControl` |
| Khoá xác thực | `C:\ProgramData\ITLabControl\keys` |
| Service | `ITLabControlService` |

Tất cả đều tách biệt với Veyon nếu máy có cài Veyon.

### 1.3 Thử nghiệm

1. Mở **Start Menu → ITLabControl → ITLabControl Teacher Console**.
2. Ở bảng bên trái chọn phòng **Phong Thu Nghiem**; ô "May GV (localhost)" hiện ảnh màn hình của chính máy.
3. Thử: **Remote view** (xem), **Remote control** (điều khiển), **Text message** (tin nhắn), **Screenshot** (chụp màn hình).

> ⚠️ **Lock không thử được qua localhost.** Veyon cố ý bỏ qua máy có địa chỉ localhost khi khoá
> (`plugins/screenlock/ScreenLockFeaturePlugin.cpp`), để giáo viên không tự khoá máy đang chạy Console.
> Nút chỉ đổi thành "Unlock" mà màn hình không bị khoá; đây là hành vi đúng. Thử Lock ở **Cách 2** (máy thật).

### 1.4 Gỡ cài đặt

**Settings → Apps → ITLabControl → Uninstall**. Thêm `/ClearConfig` khi chạy `uninstall.exe` để xoá luôn cấu hình.

---

## Cách 2: Kiểm thử trên máy thật

1. Chép `itlabcontrol-*-setup.exe` và khoá công khai `C:\ProgramData\ITLabControl\keys\public\teacher\key` sang USB.
2. **Máy GV:** chạy bộ cài đầy đủ, nhập khoá **riêng tư** (*private key*) qua Configurator (hoặc tạo khoá mới trên máy GV rồi xuất khoá công khai cho các máy HS).
3. **Mỗi máy HS:** chạy bộ cài với `/NoMaster`, rồi nhập khoá công khai:
   ```bat
   cd /d "C:\Program Files\ITLabControl"
   veyon-cli authkeys import teacher/public E:\key
   veyon-cli config set Authentication/Method 1
   veyon-cli service restart
   ```
4. Trên máy GV, thêm phòng và các máy HS theo IP/hostname (hoặc nhập CSV, sẽ có ở GĐ1).

Hướng dẫn chi tiết cho phòng máy thật sẽ hoàn thiện ở GĐ1, cùng chức năng nhập danh sách máy từ CSV.
