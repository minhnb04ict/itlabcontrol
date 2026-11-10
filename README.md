# ITLabControl

**ITLabControl** là phần mềm mã nguồn mở quản lý phòng máy tin học dành cho trường học, chạy trên Windows 10/11.
Giáo viên có thể xem và điều khiển màn hình học sinh, trình chiếu, khoá máy, gửi và thu bài, chặn web, ứng dụng và USB,
quản lý lớp học, điểm danh và tổ chức kiểm tra.

ITLabControl được phát triển dựa trên [Veyon](https://github.com/veyon/veyon) (Virtual Eye On Networks).

> ⚠️ Dự án đang ở **Giai đoạn 0** (dựng nền tảng). Chưa có bản phát hành.

## Tài liệu

- [Thiết kế và kế hoạch phát triển](docs/ITLabControl_Design_Plan.md)
- [Hướng dẫn build trên Windows](docs/BUILD.md)

## Cấu trúc mã nguồn

| Thư mục | Nội dung |
|---|---|
| `core/` | Thư viện lõi (kế thừa Veyon) |
| `master/` | Console giáo viên |
| `server/`, `service/`, `worker/` | Agent chạy trên máy học sinh |
| `configurator/`, `cli/` | Công cụ cấu hình và dòng lệnh |
| `plugins/` | Tính năng dạng plugin. Plugin riêng của ITLabControl đặt tên `itlab-*` |
| `nsis/` | Bộ cài Windows |

## Nhánh Git

- `main`: mã nguồn ITLabControl.
- `upstream`: bám theo `veyon/veyon` (nhánh `main`), được merge định kỳ vào `main`.

## Giấy phép và ghi công

ITLabControl phát hành theo giấy phép **GNU GPL v2** (xem [LICENSE](LICENSE) / [COPYING](COPYING)).

Phần lớn mã nguồn nền tảng thuộc dự án **Veyon**, Copyright (c) Tobias Junghans và cộng đồng Veyon.
Thông tin bản quyền gốc được giữ nguyên trong từng file. Mọi thay đổi của ITLabControl cũng được phát hành theo GPLv2.
