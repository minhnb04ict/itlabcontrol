# Lab thử nghiệm ITLabControl bằng VirtualBox

Lab gồm **1 máy giáo viên (GV) và 1 máy học sinh (HS)** chạy trong VirtualBox, dùng để cài thử bộ cài
mà không làm ảnh hưởng máy phát triển (bộ cài có Windows Service và driver bàn phím/chuột).

Cấu hình tham chiếu: host Windows 11 Home, RAM 16 GB, máy ảo đặt trên SSD ngoài `F:`.

## 1. Cài VirtualBox

1. Tải **VirtualBox 7.x – Windows hosts** tại <https://www.virtualbox.org/wiki/Downloads>.
2. Chạy file cài, để mặc định và chọn **Yes** khi Windows hỏi cài driver mạng.
   Nếu được yêu cầu cài *Microsoft Visual C++ Redistributable*, hãy cài theo hướng dẫn.
3. Mở VirtualBox: **File → Preferences → General → Default Machine Folder** = `F:\VMs`.

> Windows Home không có Hyper-V nên dùng VirtualBox. Nếu máy đang bật tính năng ảo hoá của Windows (WSL2/VBS),
> VirtualBox vẫn chạy được nhưng chậm hơn; điều đó chấp nhận được cho lab.

## 2. Tải bộ cài Windows (miễn phí, dùng thử 90 ngày)

- Vào **Microsoft Evaluation Center**: <https://www.microsoft.com/evalcenter>
  → **Windows 11 Enterprise** → tải bản **ISO 64-bit, English (United States)**, khoảng 6 GB.
- Lưu vào `F:\ISO\`.

## 3. Mạng ảo cho lab

**File → Tools → Network Manager → NAT Networks → Create**:

| Thuộc tính | Giá trị |
|---|---|
| Name | `itlab-net` |
| IPv4 Prefix | `10.10.1.0/24` |
| Enable DHCP | ✔ |

Hai máy cùng nằm trong `itlab-net`, thấy nhau như trong một phòng máy và vẫn ra Internet được.
(Kịch bản **khác subnet** sẽ dựng thêm sau: một mạng `10.10.2.0/24` và một máy ảo router.)

## 4. Tạo máy ảo

Tạo 2 máy, **New** với thông số:

| Thuộc tính | `ITLAB-GV` | `ITLAB-HS01` |
|---|---|---|
| ISO | `F:\ISO\...Win11...Enterprise...iso` | như bên trái |
| Type / Version | Microsoft Windows / Windows 11 (64-bit) | như bên trái |
| Skip Unattended Installation | ✔ (để tự đặt tên máy và tài khoản) | ✔ |
| RAM | 4096 MB | 4096 MB |
| CPU | 2 | 2 |
| Ổ cứng | 50 GB, dynamically allocated | 50 GB |
| EFI / TPM | Mặc định của VirtualBox 7 cho Windows 11 (EFI + TPM 2.0) | như bên trái |

Sau khi tạo: **Settings → Network → Adapter 1 → Attached to: NAT Network → `itlab-net`**.

> Chỉ nên chạy cùng lúc 2 máy ảo (tổng 8 GB), phần còn lại để cho Windows host và VS Code.

## 5. Cài Windows trong mỗi máy ảo

1. Start máy ảo, cài Windows 11 bình thường.
2. Đặt **tên máy**: `ITLAB-GV` và `ITLAB-HS01`.
   Tài khoản: GV dùng `giaovien` (Administrator); HS dùng `admin` (Administrator, để cài đặt) và `hocsinh` (Standard user, để thử nghiệm).
3. Cài **Guest Additions**: menu **Devices → Insert Guest Additions CD image** → chạy `VBoxWindowsAdditions.exe` → khởi động lại.
4. **Devices → Shared Folders**: thêm `D:\Projects\itlab-build`, tên `itlab-build`, tick *Read-only* và *Auto-mount*.
   Trong máy ảo, thư mục xuất hiện dạng ổ mạng (ví dụ `Z:`), chứa bộ cài `itlabcontrol-*-setup.exe`.
5. Tắt máy ảo và **chụp snapshot** tên `clean-windows`, để luôn quay lại trạng thái sạch được.

## 6. Cài ITLabControl (khi build xong)

| Máy | Cài |
|---|---|
| `ITLAB-GV` | Đầy đủ (Service + Teacher Console) |
| `ITLAB-HS01` | Chỉ phần Service: chạy bộ cài với `/NoMaster`, hoặc bỏ chọn *Teacher Console* |

Bước cấu hình khoá xác thực và phòng máy sẽ được hướng dẫn ở bước kiểm thử 0A.4.
