# Hướng dẫn build ITLabControl trên Windows

Hướng dẫn cho người **mới làm C++/Qt**. Làm lần lượt từ trên xuống.
Đã kiểm chứng ngày 2026-11-10 trên Windows 11, MSYS2 UCRT64, Qt 6.12.0, CMake 4.4.5.

## 1. Yêu cầu

| Hạng mục | Tối thiểu |
|---|---|
| Hệ điều hành | Windows 10/11 x64 |
| RAM | 16 GB (build dùng `-j6`; nếu máy chậm hoặc thiếu RAM, giảm xuống `-j4`) |
| Ổ đĩa | ~2 GB cho MSYS2 + Qt (ổ C), ~1 GB mã nguồn, ~3–5 GB thư mục build |
| Phần mềm | [Git for Windows](https://git-scm.com/download/win), [MSYS2](https://www.msys2.org/) cài tại `C:\msys64` |

> Không cần Docker. Bản phát hành chính thức của Veyon được cross-compile từ Linux; ITLabControl build trực tiếp trên Windows bằng MSYS2 để tiện phát triển.

## 2. Cài thư viện bằng MSYS2 (làm 1 lần)

Mở **"MSYS2 UCRT64"** từ Start Menu (biểu tượng màu vàng nâu), rồi chạy:

```bash
pacman -Syu        # cập nhật MSYS2; nếu cửa sổ tự đóng thì mở lại và chạy lại lệnh này

P=mingw-w64-ucrt-x86_64
pacman -S --needed git $P-toolchain $P-cmake $P-ninja \
  $P-qt6-base $P-qt6-tools $P-qt6-translations $P-qt6-httpserver $P-qt6-declarative \
  $P-qca-qt6 $P-openssl $P-lzo2 $P-libjpeg-turbo $P-libpng $P-zlib \
  $P-openldap $P-cyrus-sasl $P-libvncserver $P-nsis
```

Khi được hỏi chọn gói trong nhóm `toolchain`, nhấn Enter để chọn tất cả.

### 2b. Thư viện Interception (khoá bàn phím/chuột ở mức driver)

MSYS2 không có gói này, nên phải tự build từ mã nguồn [oblitum/Interception](https://github.com/oblitum/Interception) (một file C):

```bash
cd /tmp && git clone --depth 1 https://github.com/oblitum/Interception.git && cd Interception/library
gcc -O2 -shared -DINTERCEPTION_EXPORT -o interception.dll interception.c -Wl,--out-implib,libinterception.dll.a
cp interception.dll /ucrt64/bin/ && cp libinterception.dll.a /ucrt64/lib/ && cp interception.h /ucrt64/include/
```

> Giấy phép Interception: miễn phí cho mục đích **phi thương mại** (LGPL-3.0); dùng thương mại phải mua license.
> ITLabControl phân phối miễn phí, không thương mại nên được phép dùng. Không được bán kèm bản có Interception khi chưa có license thương mại.

## 3. Lấy mã nguồn

Mở **Git Bash** hoặc cmd:

```bash
git clone https://github.com/minhnb04ict/itlabcontrol.git D:/Projects/ITLAB_CONTROL
cd D:/Projects/ITLAB_CONTROL
git submodule update --init --recursive      # tải UltraVNC, libvncserver, kldap... (~40 MB)

# Liên kết với Veyon gốc để cập nhật về sau
git remote add upstream https://github.com/veyon/veyon.git
git fetch upstream
git branch upstream upstream/main
```

## 4. Cấu hình (CMake)

Trong cửa sổ **MSYS2 UCRT64**:

```bash
mkdir -p /d/Projects/itlab-build && cd /d/Projects/itlab-build
cmake /d/Projects/ITLAB_CONTROL -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DWITH_LTO=OFF \
  -DCMAKE_INSTALL_PREFIX=/d/Projects/itlab-build/install
```

- Bản dịch tiếng Việt `translations/veyon_vi.ts` tự được build thành `veyon_vi.qm` (gói `qt6-declarative` cần cho công cụ `lupdate`).
  Mỗi lần build, `lupdate` cập nhật `veyon_vi.ts` theo các chuỗi `tr()` mới trong mã; chuỗi mới ở trạng thái *unfinished* cần được dịch.

- Thư mục build đặt **ngoài repo**, để không lẫn file sinh ra vào Git.
- `RelWithDebInfo`: chạy nhanh nhưng vẫn debug được. `WITH_LTO=OFF`: build nhanh hơn khi phát triển. Bản phát hành sẽ bật lại LTO.
- Cấu hình đúng sẽ in ra khối `Veyon build summary`.

## 5. Build

```bash
cd /d/Projects/itlab-build
ninja -j6
```

Lần đầu mất khoảng 10–30 phút. Những lần sau chỉ build lại file đã sửa.

Build thành công sẽ tạo ra: `cli/veyon-cli.exe`, `cli/veyon-wcli.exe`, `configurator/veyon-configurator.exe`,
`master/veyon-master.exe`, `server/veyon-server.exe`, `service/veyon-service.exe`, `worker/veyon-worker.exe`, cùng 20 plugin `.dll` trong `plugins/`.

## 6. Kiểm tra nhanh

Trong cửa sổ **MSYS2 UCRT64** (để thấy được các DLL của Qt):

```bash
cd /d/Projects/itlab-build
./cli/veyon-cli.exe about > about.txt; cat about.txt
# Kết quả mong đợi: Veyon 4.11.4, Qt 6.12.0, OpenSSL 3.x
```

## 6b. Đóng gói: thư mục chạy được và bộ cài

Trong cửa sổ **MSYS2 UCRT64**:

```bash
cd /d/Projects/ITLAB_CONTROL
bash scripts/msys2/package.sh /d/Projects/itlab-build --installer
```

| Kết quả | Mô tả |
|---|---|
| `D:\Projects\itlab-build\package\` | Thư mục chạy được (~95 file): chương trình, plugin, DLL Qt/OpenSSL, driver Interception. Script tự dò DLL phụ thuộc bằng `ldd`. |
| `D:\Projects\itlab-build\veyon-<phiên bản>-win64-setup.exe` | Bộ cài NSIS (~31 MB). Đổi tên thành ITLabControl ở bước 0A.5. |

Bỏ `--installer` nếu chỉ cần thư mục chạy được. Kiểm tra thư mục đóng gói có đủ DLL (chạy từ **cmd** thường, không phải MSYS2):

```bat
cd /d D:\Projects\itlab-build\package
veyon-cli.exe plugin list
```

Kết quả mong đợi: liệt kê khoảng 27 plugin (WindowsPlatformPlugin, RemoteAccess, ScreenLock, ...).

> ⚠️ **Không cài bộ cài lên máy phát triển.** Bộ cài sẽ cài Windows Service và driver bàn phím/chuột. Hãy cài trên máy ảo lab (bước 0A.4).

## 7. Lỗi thường gặp

| Lỗi | Cách xử lý |
|---|---|
| `cmake: command not found` | Bạn đang ở cửa sổ MSYS2 thường, không phải **UCRT64**. Mở đúng "MSYS2 UCRT64". |
| `Could NOT find Qca-qt6` / `LZO` / `Ldap` | Thiếu gói ở bước 2, cài lại đúng tên với tiền tố `mingw-w64-ucrt-x86_64-`. |
| Máy đơ khi build | Giảm số luồng: `ninja -j2`. |
| Lỗi submodule trống (`3rdparty/ultravnc` rỗng) | Chạy lại `git submodule update --init --recursive`. |
| `interception.h: No such file or directory` | Chưa làm bước 2b. |
| Hàng loạt lỗi `GWL_USERDATA was not declared` trong UltraVNC | Mã nguồn thiếu bản vá nhận dạng 64-bit trong `CMakeLists.txt` (đã có trong repo ITLabControl). Xoá thư mục build rồi cấu hình lại. |
| `Generating ... veyon_vi.ts` lỗi mã `0xC0000135` | Thiếu gói `qt6-declarative` (lupdate cần `Qt6Qml.dll`). |
| `veyon-cli plugin list` không thấy plugin | Bình thường khi chạy trực tiếp từ thư mục build; plugin chỉ được nạp sau khi đóng gói. |

## 8. Các bản vá so với Veyon gốc

Ghi lại mọi chỗ sửa lõi Veyon để dễ merge về sau:

| File | Thay đổi | Lý do |
|---|---|---|
| `CMakeLists.txt` | Nhận dạng Windows 64-bit theo `CMAKE_SIZEOF_VOID_P` | MSYS2 để trống `CMAKE_SYSTEM_PROCESSOR`, khiến Veyon build nhầm cấu hình 32-bit |
| `CMakeLists.txt` | `-Wno-error=old-style-definition`, `-Wno-error=unused-but-set-variable` với GCC ≥ 15 | Mã cũ trong `3rdparty/ultravnc` lỗi với GCC mới |
| `cmake/modules/ITLabBranding.cmake` (mới) + `CMakeLists.txt` | Nơi duy nhất khai báo tên thương hiệu | Đổi tên chỉ cần sửa 1 file |
| `core/src/veyonconfig.h.in` | Thêm các macro `ITLAB_*` | Đưa tên thương hiệu vào C++ |
| `core/src/VeyonCore.cpp` | Tên tổ chức/ứng dụng của Qt = ITLabControl | Cấu hình lưu ở registry riêng, không đụng Veyon nếu máy có cài Veyon |
| `plugins/platform/windows/WindowsServiceFunctions.cpp` | Tên service = `ITLabControlService` | Cài song song được với Veyon |
| `plugins/platform/windows/WindowsFilesystemFunctions.cpp` | Thư mục dữ liệu `%PROGRAMDATA%\ITLabControl`, `%APPDATA%\ITLabControl` | Khoá xác thực và dữ liệu tách khỏi Veyon |
| `master/src/MainWindow.cpp`, `configurator/src/MainWindow.cpp` | Tiêu đề cửa sổ | Hiển thị tên ITLabControl |
| `cmake/modules/WindowsBuildHelpers.cmake` | Thông tin phiên bản của file .exe/.dll | ProductName/CompanyName = ITLabControl, giữ ghi công Veyon |
| `nsis/veyon.nsi.in` | Tên bộ cài, thư mục cài, Start Menu, shortcut | Bộ cài mang tên ITLabControl |

> Chưa đổi (có chủ đích): tên file `veyon-*.exe`, icon, và các chuỗi giao diện còn chữ "Veyon".
> Tên file exe giữ nguyên để dễ merge với Veyon. Chuỗi giao diện sẽ được thay qua bản dịch tiếng Việt ở GĐ1. Icon chờ có thiết kế logo.
