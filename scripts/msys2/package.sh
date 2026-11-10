#!/usr/bin/env bash
#
# ITLabControl - đóng gói bản build MSYS2 UCRT64 thành thư mục chạy được
# và (tuỳ chọn) bộ cài NSIS.
#
# Copyright (c) 2026 ITLabControl contributors
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Cách dùng (trong cửa sổ MSYS2 UCRT64):
#   scripts/msys2/package.sh <thư_mục_build> [--installer]
#
# Kết quả:
#   <thư_mục_build>/package/   – thư mục chạy được (portable)
#   <thư_mục_build>/*setup.exe – bộ cài, nếu có --installer

set -euo pipefail

if [ "${MSYSTEM:-}" != "UCRT64" ]; then
	echo "Lỗi: hãy chạy script trong cửa sổ 'MSYS2 UCRT64'." >&2
	exit 1
fi

SRCDIR=$(cd "$(dirname "$0")/../.." && pwd)
BUILDDIR=$(cd "${1:?Thiếu tham số: thư mục build}" && pwd)
MAKE_INSTALLER=${2:-}
PREFIX=/ucrt64
QTPLUGINS=$PREFIX/share/qt6/plugins
OUT=$BUILDDIR/package

echo "==> Tạo thư mục $OUT"
rm -rf "$OUT"
mkdir -p "$OUT"/{plugins,translations,crypto,imageformats,platforms,styles,tls,interception}

echo "==> Chép chương trình và plugin"
find "$BUILDDIR" -mindepth 2 -name 'veyon-*.exe' -not -path "$OUT/*" -exec cp '{}' "$OUT/" ';'
cp "$BUILDDIR/core/veyon-core.dll" "$OUT/"
find "$BUILDDIR/plugins" -name '*.dll' -exec cp '{}' "$OUT/plugins/" ';'
# Giống bố cục của Veyon: thư viện dùng chung (lib*.dll) và vnchooks.dll nằm cạnh file exe
mv "$OUT"/plugins/lib*.dll "$OUT/" 2>/dev/null || true
mv "$OUT/plugins/vnchooks.dll" "$OUT/"

echo "==> Chép plugin Qt/QCA"
cp "$QTPLUGINS/platforms/qwindows.dll" "$OUT/platforms/"
cp "$QTPLUGINS/imageformats/qjpeg.dll" "$OUT/imageformats/"
cp "$QTPLUGINS"/styles/*.dll "$OUT/styles/"
cp "$QTPLUGINS/tls/qopensslbackend.dll" "$OUT/tls/"
cp "$QTPLUGINS/crypto/libqca-ossl.dll" "$OUT/crypto/"

echo "==> Chép bản dịch"
if compgen -G "$BUILDDIR/translations/*.qm" > /dev/null; then
	cp "$BUILDDIR"/translations/*.qm "$OUT/translations/"
fi
# NSIS yêu cầu thư mục translations có ít nhất một file .qm
cp "$PREFIX/share/qt6/translations/qtbase_en.qm" "$OUT/translations/"

echo "==> Chép thành phần bên thứ ba"
cp "$SRCDIR/3rdparty/ddengine/ddengine64.dll" "$OUT/"
cp "$SRCDIR"/3rdparty/interception/* "$OUT/interception/"

echo "==> Dò và chép các DLL phụ thuộc trong $PREFIX/bin"
# Lặp cho tới khi không còn DLL mới (phụ thuộc bắc cầu)
while :; do
	NEW=$( { find "$OUT" \( -name '*.exe' -o -name '*.dll' \) -not -path "$OUT/interception/*" -print0 \
		| xargs -0 ldd 2>/dev/null \
		| awk '{print $3}' | grep -i "^$PREFIX/bin/" | sort -u \
		| while read -r dll; do
			[ -e "$OUT/$(basename "$dll")" ] || echo "$dll"
		done; } || true)
	[ -z "$NEW" ] && break
	echo "$NEW" | xargs -I{} cp {} "$OUT/"
done

echo "==> Tài liệu và giấy phép"
cp "$SRCDIR/COPYING" "$OUT/"
cp "$SRCDIR/COPYING" "$OUT/LICENSE.TXT"
cp "$SRCDIR/README.md" "$OUT/README.TXT"
unix2dos -q "$OUT"/*.TXT 2>/dev/null || sed -i 's/$/\r/' "$OUT"/*.TXT

echo "==> Thu gọn (strip) file nhị phân"
find "$OUT" \( -name '*.exe' -o -name '*.dll' \) -not -path "$OUT/interception/*" -exec strip --strip-unneeded '{}' ';' 2>/dev/null || true

echo "==> Xong: $(find "$OUT" -type f | wc -l) file, $(du -sh "$OUT" | cut -f1)"

if [ "$MAKE_INSTALLER" = "--installer" ]; then
	echo "==> Tạo bộ cài NSIS"
	cp -r "$SRCDIR/nsis" "$OUT/"
	# makensis bản Windows không chấp nhận "/" trong đường dẫn file của lệnh File
	# và các ảnh/icon MUI, nên đổi sang "\" (bỏ qua các dòng chứa URL)
	sed -E '/^\s*(File |!define MUI_[A-Z_]*(BITMAP|ICON) )/{ /:\/\//! s#/#\\#g }' \
		"$BUILDDIR/nsis/veyon.nsi" > "$OUT/veyon.nsi"
	(cd "$OUT" && makensis -V2 veyon.nsi)
	mv "$OUT"/*setup.exe "$BUILDDIR/"
	rm -rf "$OUT/nsis" "$OUT/veyon.nsi"
	echo "==> Bộ cài: $(ls "$BUILDDIR"/*setup.exe)"
fi
