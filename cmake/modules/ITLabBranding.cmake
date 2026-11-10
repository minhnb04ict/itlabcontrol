# ITLabBranding.cmake - tên thương hiệu ITLabControl dùng chung cho toàn dự án
#
# Copyright (c) 2026 ITLabControl contributors
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Mọi tên sản phẩm/công ty/dịch vụ hiển thị cho người dùng lấy từ đây.
# Các biến này được đưa vào veyonconfig.h (C++) và veyon.nsi (bộ cài).

set(ITLAB_PRODUCT_NAME          "ITLabControl")
set(ITLAB_ORGANIZATION_NAME     "ITLabControl")
set(ITLAB_ORGANIZATION_DOMAIN   "github.com/minhnb04ict/itlabcontrol")
set(ITLAB_WEBSITE               "https://github.com/minhnb04ict/itlabcontrol")
set(ITLAB_COPYRIGHT             "2026 ITLabControl contributors; based on Veyon (c) 2004-2026 Veyon Solutions / Tobias Junghans")

# Tên Windows Service. Khác "VeyonService" để cài song song được với Veyon gốc.
set(ITLAB_SERVICE_NAME          "ITLabControlService")

# Tên hiển thị của các ứng dụng
set(ITLAB_MASTER_NAME           "ITLabControl Teacher Console")
set(ITLAB_CONFIGURATOR_NAME     "ITLabControl Configurator")

# Tên file bộ cài và thư mục cài đặt
set(ITLAB_INSTALLER_BASENAME    "itlabcontrol")
set(ITLAB_INSTALL_DIRNAME       "ITLabControl")
