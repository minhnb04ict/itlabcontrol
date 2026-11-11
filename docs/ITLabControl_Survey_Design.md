# Thiết kế tính năng Khảo sát nhanh (`itlab-survey`)

Trạng thái: **đã duyệt 2026-11-11**. Làm thành **Giai đoạn 1b**, ngay sau GĐ1 và trước GĐ2. Đủ 11 dạng câu hỏi; ẩn danh là ẩn hoàn toàn (mục 3.1).
Bản này mở rộng mục "Khảo sát nhanh" trong `ITLabControl_Design_Plan.md` mục 3.1.

## 1. Mục tiêu

GV hỏi nhanh cả lớp ngay trong giờ học (lấy ý kiến, kiểm tra mức hiểu bài, câu hỏi nhanh có đáp án đúng) và thấy **bảng thống kê cập nhật theo thời gian thực** khi HS trả lời.

- Gửi một câu hỏi chỉ mất **2–3 thao tác** (có mẫu dựng sẵn).
- HS trả lời trong cửa sổ nổi trên cùng màn hình, chỉ cần bấm chuột.
- GV thấy ngay: bao nhiêu máy đã trả lời, biểu đồ phân bố đáp án, đáp án của từng máy.
- Làm việc cùng tính năng **Nhóm**: gửi khảo sát cho một nhóm, xem/lọc kết quả theo nhóm.

Khảo sát **không thay thế** bài kiểm tra (`itlab-quiz`, GĐ4): mỗi lần chỉ gửi một câu, không chấm điểm, không bảo mật đề.

## 2. Các dạng câu hỏi

| # | Dạng | HS thấy | Tuỳ chọn của GV | Thống kê |
|---|---|---|---|---|
| 1 | **Đúng / Sai** | 2 nút lớn | Đáp án đúng (không bắt buộc) | Cột Đúng/Sai, % trả lời đúng |
| 2 | **Có / Không** (± "Không chắc") | 2–3 nút | Bật/tắt "Không chắc" | Cột theo đáp án |
| 3 | **Chọn 1: A, B, C, D…** | Nút A, B, C… | Số đáp án 2–10; **nội dung từng đáp án có thể bỏ trống** (khi câu hỏi đã có trên bảng/máy chiếu, HS chỉ cần chọn chữ cái); đáp án đúng (không bắt buộc) | Cột theo đáp án, % đúng |
| 4 | **Chọn nhiều** | Ô đánh dấu A, B, C… | Như dạng 3; có thể đặt số lựa chọn tối đa | Cột theo từng lựa chọn, % chọn đúng hoàn toàn |
| 5 | **Thang số 1…n** | Hàng nút 1, 2, …, n | n = 2–10; nhãn hai đầu (ví dụ "Rất khó" … "Rất dễ") | Phân bố, **điểm trung bình**, trung vị |
| 6 | **Mức độ đồng ý** (Likert 5 mức) | Rất không đồng ý → Rất đồng ý | Đổi nhãn được | Phân bố, điểm trung bình |
| 7 | **Đánh giá sao** 1–5 | 5 ngôi sao | – | Trung bình sao, phân bố |
| 8 | **Mức hiểu bài** (đèn giao thông) | 🟢 Hiểu · 🟡 Chưa chắc · 🔴 Chưa hiểu | – | Phân bố; **tô màu ô máy** trên Console |
| 9 | **Nhập số** | Ô nhập số | Khoảng cho phép; đáp án đúng ± sai số | Trung bình, nhỏ/lớn nhất, biểu đồ tần suất, % đúng |
| 10 | **Trả lời ngắn** (văn bản) | Ô nhập 1 dòng (tối đa 200 ký tự) | Đáp án đúng (so khớp không phân biệt hoa/thường, dấu cách) | **Gộp các câu trả lời giống nhau**, đếm số lần, danh sách đầy đủ |
| 11 | **Xếp thứ tự** | Kéo thả sắp xếp các mục | 2–8 mục | Thứ hạng trung bình của từng mục |

Ghi chú:
- Dạng 3 và 4 đáp ứng yêu cầu "A: …, B: …, C: …, D: … có để điền nội dung ở sau hoặc không".
- Dạng 5 đáp ứng yêu cầu "1/2/3/4/…/n".
- Mỗi câu hỏi có thể kèm **một hình ảnh** (ảnh chụp màn hình GV hoặc file ảnh) ở giai đoạn S2.

### Mẫu câu hỏi nhanh (một chạm)
Nút mũi tên trên thanh công cụ "Khảo sát" mở menu các mẫu thường dùng. Chọn mẫu là gửi ngay, không cần gõ:
- "Các em đã hiểu bài chưa?" (dạng 8)
- "Đã làm xong chưa?" (Có / Chưa)
- "Chọn đáp án đúng" (A–D, không nội dung)
- "Đúng hay sai?"
- "Đánh giá bài học hôm nay" (sao 1–5)
- GV tự lưu thêm mẫu của mình.

## 3. Tuỳ chọn khi gửi

| Tuỳ chọn | Mặc định | Ý nghĩa |
|---|---|---|
| Thời gian trả lời | Không giới hạn | 15 giây – 10 phút; HS thấy đồng hồ đếm ngược, hết giờ cửa sổ tự đóng |
| Cho phép đổi đáp án | Bật | HS sửa được đến khi GV đóng khảo sát |
| Ẩn danh | Tắt | Bật thì **ẩn danh hoàn toàn** (dùng cho khảo sát ý kiến), xem mục 3.1 |
| Hiện kết quả cho HS sau khi đóng | Tắt | Gửi biểu đồ kết quả (và đáp án đúng nếu có) tới màn hình HS |
| Gửi cho | Máy đang hiển thị | Theo cơ chế chung: cả phòng, hoặc nhóm đang chọn |

### 3.1 Ẩn danh hoàn toàn (duyệt 2026-11-11)
Khi bật ẩn danh, **không nơi nào gắn câu trả lời với máy hay HS**:
- Console chỉ hiện thống kê tổng (biểu đồ, số đã trả lời/tổng số). **Không có** tab "Từng máy", **không** hiện nhãn đáp án trên ô máy, **không** có danh sách "Chưa trả lời" theo tên (chỉ còn con số).
- File kết quả (JSON/CSV) chỉ chứa câu hỏi, các câu trả lời đã xáo trộn thứ tự và số liệu thống kê; **không** có tên máy, tên đăng nhập hay thời điểm trả lời của từng người.
- Máy HS không gửi tên đăng nhập kèm câu trả lời. Để HS đổi được đáp án, Console chỉ giữ trong bộ nhớ một mã ngẫu nhiên cho mỗi kết nối; mã này bị xoá khi đóng khảo sát và không bao giờ ghi ra đĩa hay nhật ký (log).
- Cửa sổ câu hỏi bên HS ghi rõ "Khảo sát ẩn danh" để HS yên tâm trả lời.
- Không chuyển được khảo sát ẩn danh thành có tên sau khi đã gửi (và ngược lại).

## 4. Màn hình

### 4.1 Soạn câu hỏi (Console)
```
┌ Khảo sát nhanh ───────────────────────────────────────────────┐
│ Câu hỏi: [ Kết quả của 2 + 3 × 4 là bao nhiêu?             ]  │
│ Dạng:    (•) Chọn 1   ( ) Chọn nhiều  ( ) Đúng/Sai  ( ) Thang 1…n │
│          ( ) Mức hiểu bài  ( ) Nhập số  ( ) Trả lời ngắn  …    │
│ Đáp án:  A [ 20 ]  B [ 14 ] ✓ đúng  C [ 24 ]  D [    ]   [+][-] │
│ Thời gian: [Không giới hạn ▾]   ☑ Cho đổi đáp án   ☐ Ẩn danh    │
│ [Lưu thành mẫu]                           [Huỷ]  [Gửi tới 31 máy] │
└────────────────────────────────────────────────────────────────┘
```

### 4.2 Phía học sinh
Cửa sổ nhỏ luôn nằm trên cùng (không che toàn màn hình), nút lớn dễ bấm, có đồng hồ đếm ngược nếu đặt giờ.
Sau khi gửi: "Đã gửi câu trả lời" (và nút "Sửa" nếu được phép đổi đáp án).

### 4.3 Kết quả theo thời gian thực (Console)
```
┌ Kết quả: Kết quả của 2 + 3 × 4 là bao nhiêu? ──────── 01:25 ─┐
│ Đã trả lời 24/31  ████████████████░░░░░   Đúng: 15 (62 %)     │
│                                                               │
│ A  20  ███████              6  (25 %)                         │
│ B  14  ███████████████     15  (62 %)  ✓                      │
│ C  24  ███                  3  (13 %)                         │
│ D       ░                   0                                 │
│                                                               │
│ [Biểu đồ] [Từng máy] [Chưa trả lời (7)]   Lọc nhóm: [Tất cả ▾] │
│ [Chiếu kết quả cho HS] [Xuất Excel/CSV] [Gửi lại câu này] [Đóng khảo sát] │
└───────────────────────────────────────────────────────────────┘
```
- **Tab Từng máy:** bảng Máy · Người dùng · Đáp án · Đúng/Sai · Thời gian trả lời; sắp xếp được.
- **Tab Chưa trả lời:** danh sách máy chưa trả lời, nút "Nhắc lại" (gửi lại câu hỏi cho riêng các máy này).
- **Trên lưới màn hình chính:** ô của mỗi máy có nhãn nhỏ hiện đáp án (hoặc màu với dạng 8), để GV thấy ngay máy nào chưa hiểu.
- Kết quả cập nhật ngay khi có câu trả lời (không cần bấm làm mới).

## 5. Kỹ thuật

- Plugin mới `plugins/itlab-survey` theo khuôn các plugin ITLab đã có (`itlab-rooms` đã dùng chiều trả lời HS → GV).
- **Luồng thông điệp** (`FeatureMessage`):
  1. Console → máy HS: `StartSurvey` (mã khảo sát, câu hỏi ở dạng JSON).
  2. Server máy HS chuyển cho Worker trong phiên người dùng, Worker hiện cửa sổ câu hỏi.
  3. HS trả lời: Worker → Server → **trả lời về Console** (`sendFeatureMessageReply`, như `itlab-rooms`), kèm mã khảo sát để bỏ qua câu trả lời của khảo sát cũ.
  4. Console → HS: `StopSurvey` (đóng cửa sổ), `ShowResults` (gửi kết quả nếu GV chọn chiếu).
- Máy HS kết nối lại giữa chừng: Console tự gửi lại câu hỏi đang mở cho máy đó.
- **Biểu đồ vẽ bằng `QPainter`**, không dùng Qt Charts (Qt Charts chỉ có giấy phép GPLv3 hoặc thương mại, dùng vào sẽ buộc cả bản phân phối theo GPLv3 thay vì GPLv2 hiện nay; thêm thư viện cũng làm bộ cài nặng hơn).
- **Lưu trữ:** mỗi khảo sát lưu thành file JSON + CSV trong `Documents\ITLabControl\Khảo sát\<ngày>\` trên máy GV; mẫu câu hỏi lưu trong cấu hình cá nhân của GV (giống danh sách nhóm). Xuất Excel dùng CSV UTF-8 có BOM (Excel đọc đúng tiếng Việt), chưa cần thư viện Excel.
- Danh tính HS: tên đăng nhập Windows và tên máy. Khi có `itlab-classroom` (GĐ3) sẽ hiện họ tên HS.

## 6. Chia giai đoạn đề xuất

| Giai đoạn | Nội dung | Ước tính |
|---|---|---|
| **S1. Cơ bản** | Dạng 1, 2, 3, 4, 5, 8, 10; thời gian trả lời; cho đổi đáp án; ẩn danh; cửa sổ kết quả realtime (biểu đồ, từng máy, chưa trả lời); đáp án đúng và % đúng; xuất CSV; gửi theo nhóm | 2–3 tuần |
| **S2. Mở rộng** | Dạng 6, 7, 9, 11; mẫu câu hỏi một chạm và lưu mẫu; chiếu kết quả cho HS; nhãn đáp án trên ô máy; "Nhắc lại" máy chưa trả lời; ảnh kèm câu hỏi | 2 tuần |
| **S3. Kết nối** | Chuỗi nhiều câu hỏi liên tiếp; lịch sử khảo sát trong buổi; dùng chung phần hiển thị cho `itlab-quiz`; đưa kết quả vào nhật ký tiết học (`itlab-journal`) | Cùng GĐ4 |

## 7. Quyết định của GV (2026-11-11)

1. Làm khảo sát **ngay sau GĐ1**, trước GĐ2 (thành Giai đoạn 1b).
2. **Đủ 11 dạng câu hỏi** như mục 2, chia vào S1 và S2 như mục 6.
3. Ẩn danh là **ẩn hoàn toàn**: cả trên Console lẫn trong file kết quả (mục 3.1).
