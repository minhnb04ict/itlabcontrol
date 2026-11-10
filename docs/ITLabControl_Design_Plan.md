# ITLabControl: Thiết kế và Kế hoạch phát triển

> Phần mềm quản lý phòng máy tin học, phát triển trên mã nguồn mở **Veyon** (https://github.com/veyon/veyon).
> Mục tiêu: đạt bộ tính năng tương đương NetSupport School 14 (xem [NetSupport_School_14_Feature_Structure.md](NetSupport_School_14_Feature_Structure.md)).
> Trạng thái: **BẢN NHÁP v2, chờ duyệt.** Mỗi giai đoạn chỉ bắt đầu sau khi được duyệt.
> Ngày lập: 2026-11-10 · Nền tảng: Veyon 4.11.4 (commit `e9eeb00`, 2026-10-06)

## 0. Bối cảnh đã chốt (2026-11-10)

| Hạng mục | Quyết định | Ảnh hưởng tới kế hoạch |
|---|---|---|
| Hệ điều hành | **Chỉ Windows 10/11** (x64) | Chỉ build cho Windows; bỏ hỗ trợ Linux khỏi lộ trình; được dùng thoải mái API Windows |
| Kinh nghiệm đội | **Chưa làm C++/Qt** | Thêm lộ trình học trong GĐ0; plugin mẫu có chú thích chi tiết; ưu tiên sửa và mở rộng hơn là viết lõi |
| Phân phối | **Mã nguồn mở GPLv2, có thể cho nhiều trường** | Repo công khai từ đầu; cần chứng chỉ ký code; tài liệu cài đặt cho người ngoài; i18n chuẩn |
| Quy mô | **Số phòng không cố định; mỗi phòng 1 máy GV + 31 máy HS; cùng hoặc khác subnet** | Quản lý nhiều phòng ngay từ GĐ1; kết nối qua router (khác subnet) dùng IP/hostname trực tiếp; Hub chỉ bắt buộc khi có NAT |
| Ưu tiên | **Đưa phần Hạn chế (web/app/USB) lên trước Lớp học** | Đổi thứ tự GĐ2 ↔ GĐ3; phần Hạn chế tự làm cơ chế phiên và heartbeat riêng |

---

## 1. Quyết định nền tảng

### 1.1 Vì sao chọn Veyon
Veyon đã có sẵn khoảng 40% tính năng cốt lõi của NetSupport School: xem màn hình, điều khiển từ xa, khoá màn hình, trình chiếu, tin nhắn, bật/tắt máy, gửi/thu file, đăng nhập/đăng xuất, mở web/ứng dụng. Veyon còn có:
- Kiến trúc **plugin**: tính năng mới viết thành plugin, hạn chế sửa lõi, dễ cập nhật theo upstream.
- Qt6/C++, CMake, có sẵn bộ cài NSIS và platform plugin Windows hoàn chỉnh (`plugins/platform/windows`). ITLabControl chỉ build Windows.
- Bảo mật bằng cặp khoá RSA hoặc xác thực tài khoản đăng nhập, có ACL (Access Control Rules).
- Có Web API (cổng 11080) để làm console web hoặc mobile sau này.

### 1.2 Ràng buộc giấy phép (quan trọng)
- Veyon dùng giấy phép **GPLv2**, nên ITLabControl, vốn là sản phẩm phái sinh, **bắt buộc theo GPLv2**.
- Dự án sẽ **công khai mã nguồn trên GitHub theo GPLv2** ngay từ đầu, nên nghĩa vụ "cung cấp kèm mã nguồn" được đáp ứng tự nhiên. Mỗi bản phát hành cần gắn tag tương ứng với mã nguồn.
- Cần thêm: `README`, `CONTRIBUTING.md`, header GPL trong mỗi file mới, và **chứng chỉ ký code** (code signing) để Windows SmartScreen và antivirus không chặn bộ cài khi phát cho các trường.
- Phải giữ thông tin bản quyền gốc của Veyon. Được đổi tên và thương hiệu thành ITLabControl.
- **Không** sao chép mã, tài nguyên hay giao diện của NetSupport. Chỉ tham khảo danh mục tính năng.

### 1.3 Chiến lược mã nguồn
```
github: <org>/itlabcontrol   (fork từ veyon/veyon)
├── nhánh upstream    → bám veyon/master, định kỳ merge vào
├── nhánh main        → ITLabControl
├── core/, master/, server/, service/, worker/   → hạn chế sửa (chỉ branding, hook cần thiết)
└── plugins/itlab-*   → TOÀN BỘ tính năng mới đặt ở đây
```
Nguyên tắc: **mọi tính năng mới là plugin `itlab-*`**. Chỉ sửa lõi khi plugin không làm được, và ghi lại từng chỗ sửa trong `docs/core-patches.md`.

---

## 2. Kiến trúc

### 2.1 Thành phần (kế thừa Veyon)

| Thành phần | Tiến trình Veyon | Tên ITLabControl | Chạy ở đâu | Vai trò |
|---|---|---|---|---|
| Service | `veyon-service` | ITLab Agent Service | Máy học sinh, Windows Service (SYSTEM) | Khởi chạy Server cho từng phiên người dùng |
| Server | `veyon-server` | ITLab Agent | Máy học sinh, trong phiên người dùng | VNC server + nhận lệnh tính năng (TCP **11100**) |
| Worker | `veyon-worker` | ITLab Worker | Máy học sinh | Hiển thị UI tính năng (khoá màn hình, tin nhắn, trình chiếu…) |
| Master | `veyon-master` | **ITLab Teacher Console** | Máy giáo viên | Console điều khiển |
| Configurator | `veyon-configurator` | ITLab Configurator | Mọi máy | Cấu hình, quản lý khoá, phòng máy |
| CLI | `veyon-cli` | `itlab-cli` | Mọi máy | Script, triển khai hàng loạt |
| *(mới)* | – | **ITLab Hub** (tuỳ chọn, giai đoạn 5) | Server trường | Lưu tập trung: lớp, điểm danh, nhật ký, báo cáo, chính sách |

Cổng mặc định của Veyon: 11100 (Server), 11200 (VNC nội bộ), 11300 (Feature Worker Manager), 11400 (Demo/trình chiếu), 11080 (Web API).

### 2.1b Mô hình mạng (1 GV + 31 HS mỗi phòng, nhiều phòng)
```
 Phòng A (VLAN/subnet 10.1.1.0/24)        Phòng B (10.1.2.0/24)
 ┌──────────┐   ┌─────────── 31 HS ──┐     ┌──────────┐  ┌── 31 HS ──┐
 │ Máy GV A │──►│ Agent :11100       │     │ Máy GV B │─►│ Agent      │
 └──────────┘   └────────────────────┘     └──────────┘  └────────────┘
        │  (khác subnet: route trực tiếp qua router, mở firewall 11100/11400)
        ▼
 ┌──────────────────────────────┐  tuỳ chọn, GĐ5: lưu tập trung, relay khi NAT,
 │ ITLab Hub (server trường)    │  đồng bộ danh sách phòng và chính sách
 └──────────────────────────────┘
```
- **Cùng hoặc khác subnet nhưng có định tuyến:** Console kết nối thẳng tới IP/hostname máy học sinh, đúng cách Veyon đang làm. Không cần Hub.
- **Danh sách phòng:** mỗi phòng là một "Location" trong BuiltinDirectory (tên phòng, máy GV, 31 máy HS theo IP/hostname/MAC). Nhập bằng CSV. Một bộ cài dùng chung cho mọi phòng.
- **Phân quyền:** ACL của Veyon giới hạn "GV phòng A chỉ điều khiển được máy phòng A", dựa theo Location.
- **Băng thông:** 31 thumbnail là mức Veyon xử lý tốt. Trình chiếu dùng TCP tới 31 máy, đủ cho mạng LAN 100 Mbps/1 Gbps; multicast để ở GĐ6.
- **Có NAT** (hiếm gặp trong trường học): bắt buộc phải có Hub relay ở GĐ5.

### 2.2 Luồng một tính năng (mô hình plugin Veyon)
```
Teacher Console (Master)                  Máy học sinh
┌──────────────────────┐   FeatureMessage   ┌─────────────────────┐   ┌──────────────────┐
│ Plugin::startFeature │ ─────────────────► │ Server: plugin::    │──►│ Worker: plugin:: │
│ (nút trên toolbar)   │   qua kênh VNC +   │ handleFeatureMessage│   │ handleFeature-   │
│                      │ ◄───────────────── │ (quyền SYSTEM/user) │◄──│ Message (UI)     │
└──────────────────────┘   trạng thái/data  └─────────────────────┘   └──────────────────┘
```
Mỗi plugin cài đặt `FeatureProviderInterface` (mẫu tham khảo: `plugins/textmessage`), gồm khai báo `Feature` (UID, tên, icon, cờ Master/Service/Worker), `startFeature` / `stopFeature` phía Master, và `handleFeatureMessage` phía Server/Worker.

### 2.3 Thành phần cần quyền hệ thống (Windows)
Các tính năng hạn chế (web, ứng dụng, USB, máy in) chạy trong **Service (SYSTEM)** để học sinh không tắt được. Cách làm ưu tiên API có sẵn của Windows, **tránh viết kernel driver** vì cần ký driver EV và rủi ro làm treo máy (BSOD):

| Tính năng | Cách thực hiện đề xuất |
|---|---|
| Chặn web | Chính sách trình duyệt (Chrome/Edge/Firefox URLBlocklist/Allowlist qua registry) + Windows Filtering Platform (WFP, user-mode API) chặn theo IP/cổng + tuỳ chọn DNS filter cục bộ |
| Chặn ứng dụng | Theo dõi tiến trình qua WMI/ETW rồi kết thúc theo danh sách; tuỳ chọn AppLocker nếu dùng Windows Pro/Edu |
| USB / CD | Registry `USBSTOR` / Removable Storage policies (chặn, chỉ đọc) |
| Máy in | Print Spooler API (tạm dừng hoặc xoá job, chặn hàng đợi) |
| Theo dõi gõ phím | Low-level keyboard hook trong Worker (chỉ lưu theo danh sách từ khoá) |
| Khoá bàn phím/chuột | Có sẵn trong Veyon (`InputDevicesLock`, `ScreenLock`) |

---

## 3. Đối chiếu tính năng: NetSupport School và Veyon

Trạng thái: ✅ Veyon có sẵn · 🟡 có một phần, cần mở rộng · ❌ chưa có, phải viết mới

| Nhóm | Tính năng NetSupport | Veyon | Plugin / việc cần làm | Giai đoạn |
|---|---|---|---|---|
| **Giám sát** | Lưới thumbnail màn hình | ✅ Monitoring | Việt hoá, đổi giao diện | 1 |
| | Watch / Control | ✅ RemoteView / RemoteControl | – | 1 |
| | Scan (xoay vòng) | 🟡 Slideshow panel | Thêm chu kỳ cấu hình được 2–60 giây | 1 |
| | Spotlight / phóng to | ✅ Spotlight | – | 1 |
| | Snapshot | ✅ Screenshot | – | 1 |
| | Ghi lại và phát lại phiên (Replay) | ❌ | `itlab-replay` | 4 |
| | Trạng thái pin, Wi-Fi, ứng dụng đang chạy | 🟡 SessionInfo/UserInfo | `itlab-status` | 3 |
| **Điều khiển** | Khoá màn hình, bàn phím, chuột | ✅ ScreenLock, InputDevicesLock | Ảnh khoá tuỳ chỉnh | 1 |
| | Blank screen | ✅ (ScreenLock) | – | 1 |
| | Tắt/bật/khởi động lại, Wake-on-LAN | ✅ PowerControl | – | 1 |
| | Đăng nhập/đăng xuất từ xa | ✅ UserLogin/UserLogoff | – | 1 |
| | Chạy ứng dụng, mở website | ✅ StartApp, OpenWebsite | – | 1 |
| | Lệnh từ xa / PowerShell | ❌ | `itlab-remotecmd` (có ACL) | 5 |
| **Trình chiếu** | Show màn hình giáo viên | ✅ Demo (Window/FullScreen) | – | 1 |
| | Chiếu màn hình một học sinh cho cả lớp | ✅ ShareUserScreen | – | 1 |
| | Show qua multicast | ❌ (Demo dùng TCP) | Nghiên cứu, chưa ưu tiên | 6 |
| | Show video / ứng dụng | 🟡 | Chiếu cửa sổ ứng dụng | 4 |
| | Bảng trắng (Whiteboard) | ❌ | `itlab-whiteboard` | 4 |
| **Giao tiếp** | Tin nhắn một chiều | ✅ TextMessage | – | 1 |
| | Chat hai chiều | ❌ | `itlab-chat` | 3 |
| | Học sinh xin trợ giúp (Help Request) | ❌ | `itlab-helprequest` + phím nóng | 3 |
| | Thanh công cụ học sinh (Student Toolbar) | ❌ | `itlab-studenttoolbar` | 3 |
| | Khảo sát nhanh (Survey) | ❌ | `itlab-survey` | 4 |
| **Lớp học** | Phòng máy, danh sách máy | ✅ BuiltinDirectory / LDAP | Nhập từ CSV/Excel | 1 |
| | Danh sách lớp theo học sinh | ❌ | `itlab-classroom` | 3 |
| | Điểm danh (Student Register) | ❌ | `itlab-attendance` | 3 |
| | Hẹn giờ tiết học, mục tiêu bài học | ❌ | `itlab-lesson` | 3 |
| | Nhóm trưởng (Group Leader) | ❌ | Chưa ưu tiên | 6 |
| **File** | Gửi file / thu bài | ✅ DistributeFiles / FileCollect | Thêm thư mục theo lớp/ngày, đặt tên theo học sinh | 1 |
| | Clipboard | ✅ ClipboardExchange | – | 1 |
| **Hạn chế** | Chặn web (whitelist/blacklist/chặn hết) | ❌ | `itlab-webfilter` | 2 |
| | Chặn ứng dụng | ❌ | `itlab-appcontrol` | 2 |
| | Chặn USB/CD, chỉ đọc | ❌ | `itlab-devicecontrol` | 2 |
| | Chặn/tạm dừng in | ❌ | `itlab-printcontrol` | 2 |
| | Tắt âm thanh | ❌ | `itlab-audio` | 2 |
| | Theo dõi gõ phím | ❌ | `itlab-keymonitor` | 2 |
| | Lịch sử web/ứng dụng | ❌ | Ghi log trong các plugin trên | 2 |
| **Kiểm tra** | Soạn đề, thi trắc nghiệm, chấm điểm | ❌ | `itlab-quiz` (designer + player) | 4 |
| | Nhật ký buổi học (Journal, PDF) | ❌ | `itlab-journal` | 4 |
| **Quản trị** | Kiểm kê phần cứng/phần mềm | ❌ | `itlab-inventory` | 5 |
| | Triển khai agent từ xa | 🟡 (NSIS silent + CLI) | Bộ cài MSI + script GPO | 5 |
| | Server trung gian qua subnet/NAT | ❌ | ITLab Hub | 5 |
| | Chống gỡ/tắt agent (Protect) | 🟡 (Service + ACL) | Bảo vệ service, mật khẩu gỡ cài đặt | 5 |
| | Chính sách tập trung | 🟡 (export/import cấu hình) | Đồng bộ cấu hình qua Hub hoặc GPO | 5 |
| | Nhật ký thao tác giáo viên (audit), báo cáo | ❌ | Hub | 5 |
| **Khác** | Giao diện tiếng Việt | 🟡 (Qt i18n, chưa có `vi`) | Tạo `translations/veyon_vi.ts` | 0–1 |
| | Đổi thương hiệu ITLabControl | – | Tên, icon, bộ cài | 0 |

---

## 4. Kế hoạch theo giai đoạn

> Ước lượng tính cho **1 lập trình viên toàn thời gian, đang học C++/Qt**: thời gian đã cộng thêm phần học và một biên an toàn. Với người đã thạo C++/Qt, có thể rút khoảng 30%.
> Mỗi giai đoạn kết thúc bằng một **bản build chạy được** và một **buổi duyệt**.
> Thứ tự đã đổi theo yêu cầu: **Hạn chế (GĐ2) đứng trước Lớp học (GĐ3).**

### Giai đoạn 0: Chuẩn bị nền tảng và học C++/Qt (3–4 tuần)
**Mục tiêu:** build được Veyon gốc trên Windows, đổi tên thành ITLabControl, dựng quy trình làm việc, và đội nắm được kiến thức đủ để viết plugin.

**0A. Hạ tầng (tuần 1–2)**
1. Tạo repo **công khai** trên GitHub, fork từ Veyon; thiết lập các nhánh `upstream` và `main`; kéo các submodule (ultravnc, libvncserver, kldap, qthttpserver…).
2. Môi trường build. Đề xuất: **cross-compile trong Docker (Linux → Windows, MinGW)**, giống cách Veyon phát hành chính thức, ổn định và tái lập được. Phương án dự phòng: build trực tiếp trên Windows bằng MSYS2 + Qt6. Viết `docs/BUILD.md` từng bước cho người mới.
3. CI: GitHub Actions build `ITLabControl-Setup.exe` (NSIS, x64) cho mỗi commit, gắn file vào mỗi Release.
4. Kiểm thử (không dùng máy ảo, xem `docs/LAB.md`): hằng ngày trên máy phát triển qua `localhost`; cuối giai đoạn trên ít nhất 2 máy thật trong phòng máy.
5. Đổi thương hiệu: tên sản phẩm, icon, about, tên service, thư mục cài đặt, registry key. Giữ credit Veyon và GPL.
6. Tạo `README.md`, `CONTRIBUTING.md`, mẫu header GPL cho file mới.

**0B. Học C++/Qt theo dự án (tuần 2–4, chạy song song)**
1. Tài liệu `docs/LEARNING.md`: lộ trình C++ hiện đại (class, con trỏ thông minh, lambda), Qt (QObject, signal/slot, QWidget, Qt Designer `.ui`, QThread), CMake cơ bản.
2. Đọc và giải thích 3 plugin Veyon từ đơn giản tới phức tạp: `textmessage` → `screenlock` → `filetransfer`.
3. Plugin mẫu `plugins/itlab-hello`, chú thích từng dòng. Giáo viên bấm nút, máy học sinh hiện cửa sổ và trả lời "OK" về Console. Đây là khuôn cho mọi plugin sau này.

**Bàn giao:** `itlabcontrol-<ver>-win64-setup.exe` cài được; Console điều khiển được máy HS; plugin `itlab-hello` hoạt động.
**Tiêu chí duyệt:** (1) trên máy phát triển, xem/điều khiển/khoá/nhắn tin được qua `localhost`; (2) trên ≥ 2 máy thật, xem/điều khiển/khoá được máy HS; (3) người trong đội tự sửa được `itlab-hello`. Kịch bản khác subnet chuyển sang kiểm thử ở GĐ1.

### Giai đoạn 1: Tính năng cốt lõi, nhiều phòng, Việt hoá (3–4 tuần)
**Mục tiêu:** hoàn thiện tính năng có sẵn của Veyon cho phòng máy 1 GV + 31 HS, nhiều phòng.
1. Việt hoá toàn bộ giao diện (Console, Configurator, thông báo phía HS) qua `translations/veyon_vi.ts`, đồng thời gửi bản dịch ngược lại cho Veyon.
2. **Quản lý nhiều phòng:** nhập CSV `Phòng, Tên máy, IP/hostname, MAC, Vai trò (GV/HS)`; dò máy theo dải IP; bố cục lưới mặc định 8×4 khớp sơ đồ phòng.
3. **Phân quyền theo phòng:** mẫu ACL "máy GV phòng X chỉ điều khiển máy HS phòng X"; tài khoản quản trị điều khiển được mọi phòng.
4. Scan xoay vòng có cấu hình chu kỳ; ảnh màn hình khoá tuỳ chỉnh (logo trường).
5. Gửi/thu bài: thư mục mặc định theo `Phòng/Lớp/Ngày/Tên HS`, có hiển thị tiến độ.
6. Cấu hình Windows Firewall tự động khi cài; hướng dẫn mở cổng 11100/11400 trên router khi khác subnet.
7. Kiểm thử đầy đủ: Wake-on-LAN (lưu ý khác subnet cần directed broadcast), tắt/khởi động lại, đăng nhập từ xa, trình chiếu tới 31 máy.

**Tiêu chí duyệt:** dạy được một tiết hoàn chỉnh (chiếu màn hình, khoá máy, gửi và thu bài) bằng tiếng Việt; GV phòng A không thấy được phòng B.

### Giai đoạn 2: Hạn chế và giám sát (5–7 tuần, rủi ro kỹ thuật cao nhất) ⬆ đã đẩy lên
**Mục tiêu:** chặn web, ứng dụng, USB, in ấn, âm thanh, không bị học sinh vượt qua và không làm "kẹt" máy.

0. **Lõi hạn chế `itlab-restrictcore` (làm trước tiên).** Vì phần Lớp học/Tiết học giờ đứng sau, hạn chế cần cơ chế phiên riêng:
   - Mọi hạn chế chạy trong **Service (SYSTEM)** và được lưu trạng thái để tồn tại qua khởi động lại máy.
   - **Heartbeat:** Console gửi tín hiệu sống định kỳ. Mất tín hiệu quá N phút (cấu hình được) thì tự gỡ hạn chế.
   - **Hết hạn:** mỗi lần áp hạn chế đều có thời hạn tối đa.
   - **Gỡ khẩn cấp:** lệnh `itlab-cli restrictions clear`, chạy cục bộ bằng quyền admin.
1. `itlab-webfilter`: 4 chế độ (tự do / chỉ whitelist / chặn blacklist / chặn hết). Áp qua chính sách Chrome/Edge/Firefox và Windows Filtering Platform; có trang thông báo khi bị chặn; ghi lịch sử truy cập.
2. `itlab-appcontrol`: whitelist/blacklist ứng dụng theo tên/đường dẫn/hash, chặn sẵn trình duyệt portable và VPN/proxy phổ biến; ghi lịch sử ứng dụng đã chạy.
3. `itlab-devicecontrol`: USB/CD chặn hẳn hoặc chỉ đọc.
4. `itlab-printcontrol`: tạm dừng hoặc chặn in.
5. `itlab-audio`: tắt tiếng máy HS.
6. `itlab-keymonitor`: cảnh báo khi gõ từ khoá trong danh sách. **Tắt mặc định** vì lý do quyền riêng tư và để tránh antivirus báo nhầm.
7. Console: thanh "Hạn chế" với các nút bật/tắt cho cả phòng hoặc từng máy, và biểu tượng trạng thái trên mỗi thumbnail.
8. Các danh sách web/app dùng chung: lưu dạng file, nhập/xuất được, chia sẻ giữa các phòng.

**Tiêu chí duyệt:** HS dùng tài khoản thường không vượt qua được các hạn chế; tắt Console đột ngột thì máy HS tự gỡ hạn chế sau N phút; gỡ xong máy về trạng thái ban đầu 100%.

### Giai đoạn 3: Quản lý lớp học và giao tiếp (3–4 tuần) ⬇ đã lùi
1. `itlab-classroom`: danh sách lớp (HS ↔ máy), lưu JSON/SQLite trên máy GV.
2. `itlab-attendance`: điểm danh tự động theo tài khoản đăng nhập hoặc HS tự nhập tên; xuất Excel.
3. `itlab-lesson`: hẹn giờ tiết học, tiêu đề và mục tiêu bài học, tự kết thúc tiết. **Kết thúc tiết thì tự gỡ hạn chế** (nối với `itlab-restrictcore`).
4. `itlab-studenttoolbar`: thanh công cụ phía HS (lớp, giờ còn lại, trạng thái hạn chế, nút xin trợ giúp, chat).
5. `itlab-helprequest`: HS xin trợ giúp, Console hiện hàng đợi yêu cầu.
6. `itlab-chat`: chat GV ↔ HS, có tuỳ chọn cho HS chat với nhau.
7. `itlab-status`: hiển thị ứng dụng đang dùng, web đang mở, mạng trên thumbnail.

**Tiêu chí duyệt:** tạo lớp, điểm danh, HS xin trợ giúp và chat được với GV; kết thúc tiết thì hạn chế được gỡ.

### Giai đoạn 4: Giảng dạy và kiểm tra (4–6 tuần)
1. `itlab-quiz`: soạn đề (trắc nghiệm, đúng/sai, điền từ, có hình ảnh), giao đề, làm bài phía học sinh, chấm tự động, xuất kết quả Excel.
2. `itlab-survey`: câu hỏi nhanh và biểu đồ kết quả theo thời gian thực.
3. `itlab-whiteboard`: bảng trắng chia sẻ.
4. `itlab-journal`: xuất PDF nhật ký tiết học (ghi chú, ảnh màn hình, kết quả khảo sát/kiểm tra).
5. `itlab-replay`: ghi màn hình học sinh và phát lại (định dạng MP4).

**Tiêu chí duyệt:** tổ chức được một bài kiểm tra 15 phút cho cả phòng và nhận kết quả tự động.

### Giai đoạn 5: Quản trị tập trung (4–6 tuần)
1. **ITLab Hub** (server trường, chạy Windows/Linux, có Docker): lưu lớp, điểm danh, kết quả kiểm tra, nhật ký thao tác; đồng bộ chính sách; relay kết nối khi khác subnet.
2. `itlab-inventory`: kiểm kê phần cứng/phần mềm, xuất báo cáo.
3. Triển khai hàng loạt: bộ cài MSI, hướng dẫn GPO, cài silent bằng CLI.
4. Bảo vệ agent: chặn tắt service, mật khẩu khi gỡ cài đặt, cảnh báo khi agent mất kết nối.
5. `itlab-remotecmd`: chạy lệnh/PowerShell từ xa, chỉ cho vai trò quản trị.
6. Console web nhẹ, dùng Veyon Web API, cho bộ phận IT.

### Giai đoạn 6: Hoàn thiện và phát hành cho nhiều trường (liên tục)
Trình chiếu multicast, nhóm trưởng, tối ưu băng thông; tài liệu cho giáo viên và cho IT trường; trang web dự án; ký code mọi bản phát hành; kênh hỗ trợ cộng đồng (GitHub Issues/Discussions).

### Tổng quan tiến độ
```
Tuần:  1   3   5   7   9  11  13  15  17  19  21  23  25  27  29  31  33  35
GĐ0   ████                                   (hạ tầng + học C++/Qt)
GĐ1       ████                               (cốt lõi, nhiều phòng, tiếng Việt)
GĐ2           ███████                        (HẠN CHẾ: web/app/USB/in)
GĐ3                  ████                    (lớp học, giao tiếp)
GĐ4                      ███████             (kiểm tra, khảo sát…)
GĐ5                             ███████      (Hub, kiểm kê, triển khai)
GĐ6                                    ───── liên tục ─────
```
- Mốc **dùng thử thực tế** đầu tiên: hết GĐ2 (khoảng tuần 15, gần 4 tháng). Lúc này đã có điều khiển, trình chiếu, gửi/thu bài và chặn web/app/USB.
- Tổng ước tính khoảng **8–9 tháng** cho 1 người mới học C++/Qt. Với 2 người, có thể rút còn khoảng 5–6 tháng, vì GĐ3 và GĐ4 làm song song được sau khi xong GĐ2.

---

## 5. Rủi ro và cách giảm thiểu

| Rủi ro | Mức | Giảm thiểu |
|---|---|---|
| Lọc web bị vượt qua (VPN, proxy, trình duyệt portable) | Cao | Kết hợp chính sách trình duyệt + WFP + chặn ứng dụng proxy/VPN; chấp nhận không chặn tuyệt đối 100% |
| Hạn chế bị "kẹt" khi console giáo viên tắt đột ngột | Cao | Hạn chế gắn với phiên làm việc, có heartbeat, tự gỡ khi hết thời gian |
| Antivirus nhận nhầm agent là mã độc (hook, keylogger) | Trung bình | Ký code (code signing), xin whitelist, đặt tính năng theo dõi phím ở chế độ tắt mặc định |
| Merge upstream Veyon bị xung đột | Trung bình | Hạn chế sửa lõi, ghi lại mọi patch, merge hàng quý |
| Build trên Windows phức tạp (submodule, MinGW) | Trung bình | Dùng Docker cross-compile như Veyon; CI build tự động |
| Quyền riêng tư học sinh (theo dõi, ghi màn hình) | Trung bình | Hiển thị biểu tượng "đang được giám sát", có chính sách lưu trữ và xoá dữ liệu |
| Nghĩa vụ GPL khi phân phối | Thấp | Công khai repo, kèm file nguồn trong bộ cài |

---

## 6. Trạng thái duyệt

| Giai đoạn | Trạng thái |
|---|---|
| Bối cảnh và kiến trúc (mục 0–3) | Đã chốt câu trả lời 2026-11-10 |
| GĐ0: Nền tảng và học C++/Qt | **Đang thực hiện** (duyệt 2026-11-10) |

#### Tiến độ GĐ0
| Bước | Trạng thái |
|---|---|
| 0A.1 Repo GitHub, nhánh `main`/`upstream`, submodule | ✅ Xong |
| 0A.2 Build trên Windows bằng MSYS2 (`docs/BUILD.md`) | ✅ Xong |
| 0A.3 Đóng gói và bộ cài NSIS (`scripts/msys2/package.sh`) | ✅ Xong (CI GitHub Actions: chưa làm) |
| 0A.4 Kiểm thử (`docs/LAB.md`) | 🟡 Tiêu chí (1) localhost **đạt** 2026-11-10: Remote view, Text message, Screenshot. Tiêu chí (2) trên máy thật: chờ |
| 0A.5 Đổi thương hiệu ITLabControl | ✅ Xong (còn icon/logo) |
| 0A.6 README, giấy phép | ✅ Xong |
| 0B Tài liệu học C++/Qt, plugin mẫu `itlab-hello` | Chưa bắt đầu |
| GĐ1 → GĐ6 | Chưa duyệt |

### Việc bạn cần chuẩn bị cho GĐ0
- Tài khoản GitHub (cá nhân hoặc tổ chức) để tạo repo công khai `itlabcontrol`.
- Máy phát triển: Windows 10/11, RAM ≥ 16 GB, ổ trống ≥ 60 GB. Build bằng MSYS2 (không cần Docker), kiểm thử không cần máy ảo.
- (Có thể để sau, nhưng cần trước khi phát hành cho trường khác) Chứng chỉ ký code. Có thể đăng ký miễn phí cho dự án mã nguồn mở qua SignPath Foundation.
