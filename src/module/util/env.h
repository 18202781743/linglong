/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_ENV_H_
#define LINGLONG_SRC_MODULE_UTIL_ENV_H_

#include <QStringList>

namespace linglong {
namespace util {
const QStringList envList = {
    "DISPLAY",
    "LANG",
    "LANGUAGE",
    "XAUTHORITY",
    "XDG_SESSION_DESKTOP",
    "D_DISABLE_RT_SCREEN_SCALE",
    "XMODIFIERS",
    "DESKTOP_SESSION",
    "DEEPIN_WINE_SCALE",
    "XDG_CURRENT_DESKTOP",
    "XIM",
    "XDG_SESSION_TYPE",
    "CLUTTER_IM_MODULE",
    "QT4_IM_MODULE",
    "GTK_IM_MODULE",
    "auto_proxy",   // 网络系统代理自动代理
    "http_proxy",   // 网络系统代理手动http代理
    "https_proxy",  // 网络系统代理手动https代理
    "ftp_proxy",    // 网络系统代理手动ftp代理
    "SOCKS_SERVER", // 网络系统代理手动socks代理
    "no_proxy",     // 网络系统代理手动配置代理
    "USER",         // wine应用会读取此环境变量
    "PATH",
    "HOME",
    "QT_IM_MODULE",    // 输入法
    "LINGLONG_ROOT",   // 玲珑安装位置
    "WAYLAND_DISPLAY", // 导入wayland相关环境变量
    "QT_QPA_PLATFORM",
    "QT_WAYLAND_SHELL_INTEGRATION",
    "GDMSESSION",
    "QT_WAYLAND_FORCE_DPI",
    "GIO_LAUNCHED_DESKTOP_FILE" // 系统监视器
};
}
} // namespace linglong
#endif