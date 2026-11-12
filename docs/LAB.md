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

## Cách 2: Kiểm thử trên máy thật (triển khai một phòng)

Làm theo thứ tự 2.1 → 2.5. **Mọi máy (GV và HS) phải cài cùng một phiên bản** ITLabControl.

### 2.1 Máy giáo viên (làm 1 lần cho mỗi phòng)

1. Cài `itlabcontrol-*-setup.exe` bản đầy đủ (bấm Next đến hết).
2. Mở **ITLabControl Configurator → Phòng máy**:
   - **Phòng của máy này:** gõ tên phòng, ví dụ `Phòng 201`.
   - Bấm **"Tạo khoá cho phòng này (máy giáo viên)…"**, chọn ổ USB. Máy tạo file `phong-201_public_key.pem` trên USB.
   - Nếu được hỏi "Xoá các khoá bí mật khác (teacher…)?", chọn **Yes**. Nếu không xoá, Console có thể dùng nhầm khoá chung và điều khiển được cả phòng khác.
   - (Tuỳ chọn) **Màn hình khoá → Chọn ảnh…**: logo trường; sửa dòng chữ hiện khi khoá máy.
   - Bấm **Áp dụng**.
3. Mở Teacher Console → **Quản lý phòng** → tạo phòng `Phòng 201` (theo mẫu tên hoặc tự dò máy).

### 2.2 Chuẩn bị USB cho các máy học sinh của phòng

Thư mục `ITLabControl-HocSinh` trên USB gồm:

| File | Bắt buộc | Ghi chú |
|---|---|---|
| `itlabcontrol-*-setup.exe` | ✔ | Cùng phiên bản với máy GV |
| `CAI-DAT-MAY-HOC-SINH.cmd` | ✔ | Script cài máy HS |
| `phong-201_public_key.pem` | ✔ | Khoá của **đúng phòng này**, tạo ở bước 2.1 |
| `quantri_public_key.pem` | | Khoá của bộ phận IT, nếu cần một máy điều khiển được mọi phòng |
| `man-hinh-khoa.png` | | Logo hiện trên màn hình khoá |

> ⚠️ **Không để khoá của phòng khác hoặc khoá chung cũ `teacher_public_key.pem` trong thư mục.**
> Script nhập **mọi** file `*_public_key.pem` có trong thư mục, và gỡ mọi khoá cũ không có trong đó.

### 2.3 Mỗi máy học sinh

Chuột phải `CAI-DAT-MAY-HOC-SINH.cmd` → **Run as administrator** → nhập tên phòng (`Phòng 201`) → chờ chữ **XONG** → khởi động lại máy.

Làm bằng giao diện thay cho script (ví dụ khi đổi máy sang phòng khác): **Configurator → Phòng máy**:
- Sửa **Phòng của máy này**.
- Bấm **"Chỉ cho phép các khoá này (máy học sinh)…"**, chọn file khoá của phòng (có thể chọn thêm khoá quản trị).
- Bấm **Áp dụng**.

### 2.4 Máy GV và HS khác mạng con (subnet)

Tường lửa Windows trên từng máy được bộ cài **tự cấu hình** (quy tắc "ITLabControl Server" và "ITLabControl Worker",
áp cho mọi loại mạng). Chỉ khi máy GV và máy HS nằm ở **hai mạng con khác nhau** mới cần IT cấu hình thêm trên router/tường lửa giữa hai mạng:

| Cổng | Giao thức | Chiều | Dùng cho |
|---|---|---|---|
| 14100 | TCP | GV → HS | Kết nối điều khiển chính (xem, khoá, gửi file…) |
| 14400 | TCP | HS → GV | Trình chiếu màn hình GV |
| 9 | UDP | GV → mạng HS | Bật máy từ xa (Wake-on-LAN); router cần cho phép *directed broadcast* |

- Cổng 14200 và 14300 chỉ dùng **trong cùng một máy**, không cần mở.
- Khác mạng con thì tên máy thường **không tra ra được** (không có DNS nội bộ): trong Quản lý phòng hãy nhập máy theo **địa chỉ IP**, hoặc nhờ IT thêm bản ghi DNS.
- Veyon gốc dùng cổng 11100–11400; ITLabControl dùng 14xxx nên chạy song song được với Veyon.

### 2.5 Phiếu kiểm thử phòng thật

Ghi **Đạt / Không đạt** và ghi chú (máy nào, hiện tượng gì). Mục có ⭐ là tính năng mới, cần chú ý.

| # | Kiểm tra | Cách làm | Kết quả mong đợi | Kết quả |
|---|---|---|---|---|
| 1 | Hiện đủ máy | Mở Console, chọn phòng | Đủ 31 máy, có hình màn hình | |
| 2 | Xem / Điều khiển | Nháy đúp 1 máy; bấm Điều khiển | Thanh công cụ cố định ở trên, điều khiển được chuột/phím | |
| 3 | Khoá máy ⭐ | Bấm Khoá máy | Mọi máy HS khoá; hiện **logo trường và dòng chữ**; Ctrl+Alt+Del bị chặn | |
| 4 | Mở khoá | Bấm lại nút | Mọi máy mở khoá | |
| 5 | Trình chiếu | Trình chiếu toàn màn hình, rồi chế độ cửa sổ | 31 máy thấy màn hình GV, không giật nhiều | |
| 6 | Tin nhắn có link ⭐ | Gửi tin nhắn có `https://…` | HS bấm link mở được trình duyệt | |
| 7 | Quản lý màn hình ⭐ | Thu nhỏ / Mở rộng / Đóng tất cả ứng dụng | Đúng hành vi; đóng ứng dụng có hộp xác nhận | |
| 8 | Gửi file | Gửi 1 file Word cho cả phòng | Mọi máy nhận đủ | |
| 9 | Thu bài: xem trước ⭐ | Thu bài → **Xem trước** → bỏ tick vài file → Bắt đầu | Chỉ các file được tick về máy GV | |
| 10 | Thu bài: duyệt 1 máy ⭐ | Chọn 1 máy → Thu bài → **Duyệt…** → chọn vài file | Duyệt được ổ đĩa/thư mục; chỉ thu đúng file đã chọn | |
| 11 | Chia nhóm ⭐ | Tạo 2 nhóm; chọn Nhóm 1 → Khoá máy | Chỉ máy Nhóm 1 bị khoá | |
| 12 | Xoay vòng màn hình ⭐ | Mở bảng Slideshow, kéo thời gian lên 30 s | Mỗi máy hiện 30 giây | |
| 13 | Phân quyền theo phòng ⭐ | Máy GV phòng **khác** (khoá khác) thêm 1 máy HS phòng này | Không điều khiển được (báo lỗi xác thực) | |
| 14 | Khoá quản trị ⭐ | Máy IT có khoá `quantri` thêm máy HS | Điều khiển được | |
| 15 | Tự dò máy | Quản lý phòng → Tự dò | Máy HS tự xếp đúng phòng | |
| 16 | Đổi tên máy | Đổi tên hiển thị 1 máy | Tên mới hiện ngay, giữ sau khi mở lại Console | |
| 17 | Tắt / Khởi động lại | Chọn 2 máy → Khởi động lại | Hai máy khởi động lại | |
| 18 | Bật máy (WOL) | Tắt 1 máy (Shut down), chờ 1 phút, bấm Bật máy | Máy bật lên trong ~30 giây (xem mục 2.6 nếu không bật) | |
| 19 | Song song với Veyon | Máy có cài Veyon gốc | Veyon và ITLab cùng chạy, không ảnh hưởng nhau | |
| 20 | Hiệu năng | Console mở 30 phút với 31 máy | Không đứng, CPU máy GV ổn định | |

### 2.6 "Bật máy" không hoạt động

Bật máy dùng gói Wake-on-LAN (WOL) gửi tới địa chỉ MAC của máy HS. Gói này là gói *broadcast*, **không đi qua router**.

1. **Máy GV và máy HS phải cùng mạng con**, hoặc trong phòng phải còn **ít nhất một máy HS đang bật**. Từ bản 2026-11-12, Console
   nhờ tối đa 2 máy đang bật của cùng phòng phát gói bật máy ngay trong mạng của phòng.
   Kiểm tra mạng con: `ipconfig` trên máy GV và máy HS, ba số đầu của IPv4 (với mặt nạ 255.255.255.0) phải giống nhau.
2. **Máy HS có địa chỉ MAC** trong Quản lý phòng (tự có khi thêm máy bằng "Tự dò máy").
3. **Windows trên máy HS:** script cài máy HS đã tự bật *Wake on Magic Packet*, *Shutdown Wake-On-Lan* cho card mạng và
   **tắt Fast startup** (Fast startup làm card mạng không nhận gói bật máy sau khi Shut down). Máy cài bằng bản cũ hơn: chạy lại script.
4. **BIOS/UEFI máy HS** (phần mềm không chỉnh được, IT làm 1 lần mỗi máy): bật *Wake on LAN* / *Power On by PCI-E* /
   *Resume by LAN*; tắt *ErP* / *Deep Sleep* / *EuP 2013*.
5. **Laptop:** thường chỉ bật được khi cắm sạc và cắm **dây mạng**; qua Wi-Fi không bật được.
6. **Script kiểm tra:** chạy `KIEM-TRA-BAT-MAY.cmd` (Run as administrator) trên máy HS khi máy đang bật. Script ghi ra file
   `KIEM-TRA-BAT-MAY-<tên máy>.txt` cho biết card mạng đang dùng (card trong máy / **card USB, dock** / Wi-Fi), trạng thái
   Wake-on-LAN, Fast startup, pin, và hỏi có bật luôn các thiết lập không. Card USB/dock thường không bật được máy sau khi Shut down.
7. Thử từ dòng lệnh trên máy GV (cùng mạng con với máy HS): `veyon-cli power on 1C:69:7A:93:4A:CF` (thay bằng MAC của máy HS).

**Khi có lỗi:** ghi lại giờ xảy ra, gửi kèm các file log trong `C:\Windows\Temp\ITLabControl*.log` (máy HS)
và `%TEMP%\ITLabControl*.log` (máy GV). Cách bật log chi tiết: `docs/LEARNING.md` mục 7.1.
