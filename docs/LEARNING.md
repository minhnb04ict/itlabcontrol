# Học C++/Qt để viết plugin ITLabControl

Tài liệu dành cho người **chưa làm C++/Qt**, học **vừa đủ để viết plugin `itlab-*`**, không học dàn trải.
Mọi ví dụ lấy từ mã của dự án, chủ yếu từ plugin mẫu [`plugins/itlab-hello`](../plugins/itlab-hello/).

> Plugin mẫu `itlab-hello` (nút "Xin chào") **mặc định không build** và không có trong bộ cài.
> Để học và thử nghiệm, bật lại trong thư mục build: `cmake -DWITH_ITLAB_SAMPLES=ON .` rồi `ninja`.

> Yêu cầu trước: đã build được dự án theo [BUILD.md](BUILD.md) và cài thử theo [LAB.md](LAB.md).

---

## 0. Lộ trình 3 tuần

| Tuần | Học | Kết quả cuối tuần |
|---|---|---|
| **1** | C++ tối thiểu (mục 1) + đọc kỹ `itlab-hello` | Giải thích được từng dòng của `ItlabHelloFeaturePlugin.cpp` |
| **2** | Qt (mục 2), CMake (mục 3), kiến trúc Veyon (mục 4) | Làm xong bài tập 1–2 (mục 8) |
| **3** | Đọc 3 plugin Veyon (mục 5), gỡ lỗi (mục 7) | Làm xong bài tập 3–4; tự tạo một plugin mới theo checklist (mục 6) |

Mỗi ngày: khoảng 1 giờ đọc tài liệu và 1–2 giờ thực hành trên mã thật. **Sửa, build, cài, thử** nhiều lần quan trọng hơn đọc nhiều.

---

## 1. C++ tối thiểu cần biết

Nguồn học chính (miễn phí, tiếng Anh dễ đọc): **<https://www.learncpp.com>**, học các chương 1–13, 15, 20 (lambda).
Dưới đây là những gì **thực sự xuất hiện** trong plugin.

### 1.1 File `.h` và `.cpp`
- `.h` (header) **khai báo**: class có gì, hàm nhận gì, trả gì.
- `.cpp` **cài đặt**: thân các hàm.
- `#pragma once` ở đầu `.h`: tránh bị nạp hai lần.
- `#include "X.h"` (file của dự án) và `#include <QString>` (thư viện).

### 1.2 Class, kế thừa, `override`
```cpp
class ItlabHelloFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
    ...
    const FeatureList& featureList() const override;   // "override": ghi đè hàm của lớp cha
};
```
Veyon định nghĩa sẵn các **interface** (lớp cha có hàm ảo `virtual`). Plugin **ghi đè** những hàm cần dùng.
Từ khoá `override` giúp trình biên dịch báo lỗi nếu viết sai tên hoặc tham số.

### 1.3 Tham chiếu `&`, `const`
```cpp
bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
```
- `X&`: **tham chiếu**, dùng trực tiếp đối tượng gốc, không sao chép.
- `const X&`: tham chiếu **chỉ đọc**. Đây là cách truyền tham số phổ biến nhất trong Veyon.
- Hàm có `const` ở cuối (`featureList() const`): hàm này **không thay đổi** đối tượng.

### 1.4 `auto`, `enum class`
```cpp
const auto requestId = QUuid::createUuid();    // trình biên dịch tự suy ra kiểu
enum class FeatureCommand { ShowHello, HelloAcknowledged };
```
`enum class` an toàn hơn `enum` thường: phải viết đầy đủ `FeatureCommand::ShowHello`.

### 1.5 Con trỏ, `new`, ai xoá đối tượng?
```cpp
auto box = new QMessageBox( ... );          // tạo trên heap, nhận về con trỏ
box->setAttribute( Qt::WA_DeleteOnClose );  // "->" thay cho "." khi dùng con trỏ
```
Trong Qt, **không gọi `delete` bằng tay** ở hầu hết trường hợp:
- `Qt::WA_DeleteOnClose`: cửa sổ tự xoá khi đóng.
- Đối tượng có **parent** (`new X(parent)`) bị xoá cùng parent.
- `QPointer<X>`: con trỏ tự về `nullptr` khi đối tượng bị xoá (xem `MessageContext`).

### 1.6 Lambda
```cpp
connect( box, &QMessageBox::finished, this, [&worker, requestId]() {
    worker.sendFeatureMessageReply( ... );
} );
```
Lambda là **hàm không tên** viết ngay tại chỗ, thường để "làm việc này **sau**, khi sự kiện xảy ra".
`[&worker, requestId]` là phần **bắt biến**: `&worker` bắt tham chiếu, `requestId` bắt bản sao.
⚠ Chỉ bắt tham chiếu (`&`) khi chắc chắn đối tượng **còn sống** lúc lambda chạy.

### 1.7 Bẫy riêng của Veyon: sao chép `explicit`
```cpp
const auto msg = FeatureMessage{...}.addArgument(...);   // ❌ LỖI biên dịch
sendFeatureMessage( FeatureMessage{...}.addArgument(...), list );  // ✅ tạo và dùng ngay
```
`FeatureMessage` khai báo hàm sao chép là `explicit`, nên không được dùng `auto x = ...`.
(Đây là lỗi thật đã gặp khi viết `itlab-hello`.)

---

## 2. Qt cần biết

Tài liệu chính: **<https://doc.qt.io/qt-6/>**. Tra cứu từng class khi gặp, không cần đọc hết.

| Khái niệm | Ý nghĩa | Ví dụ trong dự án |
|---|---|---|
| `QObject` + `Q_OBJECT` | Lớp gốc của Qt. Có `Q_OBJECT` thì công cụ **moc** sinh thêm mã cho signal/slot, `tr()` | Đầu class plugin |
| **Signal / slot** | Cơ chế sự kiện: "khi A xảy ra (signal), gọi B (slot hoặc lambda)" | `connect(box, &QMessageBox::finished, …)` |
| `QString`, `QStringLiteral("…")` | Chuỗi Unicode. Dự án **cấm** chuyển ngầm từ `"…"`, phải dùng `QStringLiteral` hoặc `tr` | `QStringLiteral("ItlabHello")` |
| `tr("…")` | Chuỗi hiển thị cho người dùng, **dịch được**. Luôn viết **tiếng Anh** trong mã; tiếng Việt nằm trong `translations/veyon_vi.ts` | `tr( "Say hello" )` → dịch thành "Xin chào" |
| `QVariant`, `QVariantMap` | Hộp chứa giá trị bất kỳ kiểu nào; bảng tên → giá trị | Tham số của `controlFeature()` |
| `QUuid` | Mã định danh duy nhất | `RequestId`, UUID tính năng |
| `QWidget`, `QDialog`, `QMessageBox` | Giao diện | Hộp thoại chào |
| `.ui` + **Qt Designer** | Vẽ giao diện bằng kéo thả, sinh mã tự động | `plugins/textmessage/TextMessageDialog.ui` |
| `.qrc` | Nhúng ảnh/icon vào file `.dll`, gọi bằng đường dẫn `:/…` | `itlab-hello.qrc` |
| **Vòng lặp sự kiện** | Chương trình Qt chạy trong vòng lặp chờ sự kiện; `show()` không chặn, `exec()` chặn | `box->show()` |

### ⚠ Bài học thật: chương trình tự thoát khi cửa sổ cuối cùng đóng
Mặc định Qt **kết thúc chương trình khi cửa sổ cuối cùng đóng**. Lần chạy thử đầu tiên của `itlab-hello`,
Worker thoát ngay khi học sinh bấm OK, **trước khi** tin nhắn xác nhận kịp gửi đi, nên Console không nhận được gì.
Cách sửa: `box->setAttribute( Qt::WA_QuitOnClose, false );`
→ Mọi cửa sổ hiện ở **Worker** mà sau đó còn phải gửi dữ liệu đi đều cần dòng này.

---

## 3. CMake trong dự án

Mỗi plugin là **một thư mục trong `plugins/`** với một `CMakeLists.txt` ngắn:

```cmake
include(BuildVeyonPlugin)
build_veyon_plugin(itlabhello          # tên file .dll
    NAME ItlabHello
    SOURCES
    ItlabHelloFeaturePlugin.cpp
    ItlabHelloFeaturePlugin.h
    itlab-hello.qrc
    )
```
- `plugins/CMakeLists.txt` **tự tìm mọi thư mục con**, nên thêm thư mục là đủ, không phải sửa chỗ khác.
- Thêm/xoá thư mục plugin thì **chạy lại `cmake .`** trong thư mục build trước khi `ninja`.
- Script đóng gói tự chép mọi `*.dll` trong `plugins/` vào bộ cài.

---

## 4. Kiến trúc Veyon dưới góc nhìn plugin

### 4.1 Ba chương trình, một plugin
Cùng một file `.dll` được nạp vào 3 chương trình. Mỗi chương trình gọi một nhóm hàm khác nhau:

```
 MÁY GIÁO VIÊN                     MÁY HỌC SINH
┌───────────────────┐  TCP 11100  ┌──────────────────────┐  TCP 11300  ┌───────────────────────┐
│ MASTER            │────────────►│ SERVER               │────────────►│ WORKER                │
│ veyon-master.exe  │◄────────────│ veyon-server.exe     │◄────────────│ veyon-worker.exe      │
│ (Teacher Console) │             │ (chạy nền, quyền cao)│             │ (trong phiên của HS,  │
└───────────────────┘             └──────────────────────┘             │  hiện được cửa sổ)    │
                                                                       └───────────────────────┘
 startFeature()                    handleFeatureMessage(server…)        handleFeatureMessage(worker…)
 controlFeature()                  handleFeatureMessageFromWorker()
 handleFeatureMessage(cci…)
```

### 4.2 `FeatureMessage`: gói tin duy nhất
`FeatureMessage{ featureUid, command }.addArgument( Argument::X, giá_trị )`
- `featureUid`: UUID của tính năng. Mọi plugin đều nhận mọi tin nhắn, nên **dòng đầu mỗi hàm luôn là kiểm tra UUID**.
- `command`: `enum class FeatureCommand` của plugin.
- Tham số: `enum class Argument` của plugin.

### 4.3 Loại tính năng (`Feature::Flag`)
| Cờ | Ý nghĩa | Ví dụ |
|---|---|---|
| `Action` | Bấm một lần là xong | Xin chào, Text message, Reboot |
| `Mode` | Bật/tắt, có trạng thái | Lock, Demo |
| `Master`, `Service`, `Worker` (`AllComponents`) | Tính năng chạy ở những chương trình nào | Hầu hết dùng `AllComponents` |

### 4.4 Hai loại Worker
| Hàm ở Server | Worker chạy với quyền | Dùng khi |
|---|---|---|
| `sendMessageToUnmanagedSessionWorker` | **Tài khoản học sinh**, trong phiên đăng nhập | Hiện hộp thoại, tương tác với HS (`textmessage`, `itlab-hello`) |
| `sendMessageToManagedSystemWorker` | **SYSTEM**, Server quản lý vòng đời | Việc cần quyền cao hoặc phải bật/tắt chắc chắn (`screenlock`) |

### 4.5 Trả lời về Console: `MessageContext`
Server phải **ghi nhớ `MessageContext`** (kết nối tới từ Console nào) khi nhận lệnh, để sau đó
`server.sendFeatureMessageReply( context, reply )` gửi đúng về Console đó.
`itlab-hello` lưu theo `RequestId` trong `QMap<QUuid, MessageContext>`; `filetransfer` làm tương tự.

---

## 5. Đọc 3 plugin, từ dễ tới khó

| Thứ tự | Plugin | Học được gì | Đọc file |
|---|---|---|---|
| 1 | `plugins/textmessage` | Đường đi **một chiều** Master → Server → Worker; hộp thoại `.ui` cho GV nhập nội dung | `TextMessageFeaturePlugin.cpp`, `TextMessageDialog.ui` |
| 2 | `plugins/itlab-hello` | Đường đi **hai chiều**, `MessageContext`, `WA_QuitOnClose` | Cả 2 file, đọc theo thứ tự `BƯỚC 1 → 4` |
| 3 | `plugins/screenlock` | Tính năng loại **Mode** (bật/tắt), **managed system worker**, bỏ qua máy localhost | `ScreenLockFeaturePlugin.cpp` (hàm `controlFeature`, dòng ~80–110) |
| 4 | `plugins/filetransfer` | Tính năng có **trạng thái dài**, truyền dữ liệu từng phần, giao diện phức tạp | `FileTransferPlugin.cpp`. Đọc **sau cùng**, chỉ cần hiểu luồng chính |

Câu hỏi tự kiểm tra sau khi đọc mỗi plugin:
1. Giáo viên bấm nút thì hàm nào chạy đầu tiên?
2. Hàm nào chạy trên máy học sinh, trong chương trình nào?
3. Dữ liệu nào được gửi đi, gói trong tham số nào?
4. Có trả lời về Console không? Bằng cách nào?

---

## 6. Checklist tạo plugin mới

1. Chép `plugins/itlab-hello` thành `plugins/itlab-<tên>`.
2. Đổi tên class/file, sửa `CMakeLists.txt` và `.qrc`.
3. **Tạo 2 UUID mới** (UUID plugin, UUID tính năng), tuyệt đối không dùng lại:
   `node -e "console.log(require('crypto').randomUUID())"`
4. Đổi `Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.<Tên>")`, `name()`, `description()`.
5. Viết mọi chuỗi giao diện bằng **tiếng Anh** trong `tr()`.
6. Build: `cmake .` rồi `ninja -j6` (trong MSYS2 UCRT64, thư mục build). Bước build tự thêm chuỗi mới vào `translations/veyon_vi.ts`.
7. **Dịch các chuỗi mới** trong `veyon_vi.ts` (mở bằng **Qt Linguist**: `linguist translations/veyon_vi.ts`, hoặc sửa thẳng thẻ `<translation>`), build lại.
8. Đóng gói: `bash scripts/msys2/package.sh /d/Projects/itlab-build --installer`.
9. **Đóng Teacher Console**, rồi cài: `itlabcontrol-…-setup.exe /S /NoInterception` (quyền admin).
10. Kiểm tra plugin được nạp: `veyon-cli plugin list`.
11. Mở Console, thử qua localhost (trừ tính năng có khoá màn hình), thử cả 2 ngôn ngữ (nút ngôn ngữ góc dưới bên phải).
12. Commit kèm mô tả tiếng Việt, push. CI tự build lại bộ cài.

---

## 7. Gỡ lỗi

### 7.1 Bật log chi tiết (quyền admin, trong cmd)
```bat
cd /d "C:\Program Files\ITLabControl"
veyon-cli config set Logging/LogLevel 5
veyon-cli config set Logging/LogFileDirectory "C:\ProgramData\ITLabControl\logs"
veyon-cli service restart
```
Mức log: 1 Critical, 2 Error, 3 Warning, 4 Info, **5 Debug**. Trả về mặc định (Warning): `veyon-cli config set Logging/LogLevel 3`.
Sau khi đổi, **mở lại Teacher Console** để Console cũng ghi log mới.

### 7.2 Đọc log
Thư mục `C:\ProgramData\ITLabControl\logs`: `VeyonMaster.log`, `VeyonServer.log`,
`VeyonFeatureWorker-<UUID tính năng>.log`. Các dòng quan trọng:
```
FeatureManager::handleFeatureMessage(): [WORKER] FeatureMessage(ItlabHello,0,{…})   ← Worker nhận lệnh
FeatureWorkerManagerConnection::sendMessage(): FeatureMessage(ItlabHello,1,{…})      ← Worker gửi trả lời
VeyonCore::exec(): Exit                                                              ← Worker thoát
```
Ví dụ thật: hai dòng cuối cùng một mili-giây chính là dấu hiệu của lỗi `WA_QuitOnClose` ở mục 2.

### 7.3 Ghi log từ mã của mình
```cpp
vDebug() << "đã nhận yêu cầu" << requestId;     // chỉ hiện ở mức Debug
vWarning() << "không tìm thấy RequestId";        // mức Warning
```

---

## 8. Bài tập (làm trên bản sao của `itlab-hello`)

1. **Nhập nội dung lời chào**: thay lời chào cố định bằng hộp thoại cho giáo viên nhập (gợi ý: `QInputDialog::getText`, hoặc xem `TextMessageDialog`).
2. **Hai nút trả lời**: học sinh chọn "Có" hoặc "Không"; Console hiện câu trả lời tương ứng (thêm `Argument::Answer`).
3. **Gom kết quả**: thay vì mỗi máy một hộp thông báo, Console hiện **một** cửa sổ bảng (`QTableWidget`) liệt kê máy và câu trả lời, cập nhật khi có máy trả lời. (Đây chính là nền móng của `itlab-survey`.)
4. **Đếm ngược phía học sinh**: hộp thoại tự đóng sau 20 giây nếu học sinh không bấm (`QTimer::singleShot`), gửi trả lời "Không phản hồi".

Bài 2–3 là bước đệm trực tiếp cho plugin **khảo sát** ở GĐ3.

---

## 9. Tài liệu tham khảo

| Nguồn | Dùng để |
|---|---|
| <https://www.learncpp.com> | Học C++ từ đầu |
| <https://doc.qt.io/qt-6/> | Tra cứu class Qt |
| <https://doc.qt.io/qt-6/signalsandslots.html> | Signal/slot |
| <https://docs.veyon.io/> | Tài liệu sử dụng và quản trị Veyon |
| `core/src/FeatureProviderInterface.h` | Danh sách đầy đủ các hàm plugin có thể ghi đè, có chú thích |
| [ITLabControl_Design_Plan.md](ITLabControl_Design_Plan.md) | Danh sách plugin sẽ viết và đặc tả |
