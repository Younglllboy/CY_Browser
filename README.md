# CYBROWSER

CYBROWSER 是基于 Chromium Embedded Framework（CEF）的 Windows x64 桌面浏览器。当前版本为 v2.0.0 免安装版。AI 助手和插件系统仍在规划中，当前版本尚未提供这两项功能。

## 下载和使用

从 [Releases](https://github.com/Younglllboy/CY_Browser/releases) 下载 `CYBROWSER-v2.0.0-win-x64.zip`，完整解压后运行文件夹中的 `CYBROWSER.exe`。请保留解压后的 DLL、资源文件及 `locales` 文件夹；单独复制 EXE 无法正常运行。当前仅提供 Windows x64 版本，没有安装程序。升级时可用新版本文件夹运行，浏览器数据仍保存在同一 Windows 用户目录。

浏览器数据保存在当前 Windows 用户的 `%LOCALAPPDATA%\CYBROWSER`，不位于程序文件夹中。删除程序文件夹不会自动清除这些数据。

## 当前功能

- 地址栏可输入网址或搜索词；支持后退、前进、刷新和主页操作。
- 可切换 Bing、百度、Google、DuckDuckGo 及 Firefox（Google）搜索预设，并保存选择。
- 提供网页标题、站点图标和快捷入口栏。
- 提供浏览历史、收藏标签、下载记录和设置管理界面；支持收藏分组与快捷入口管理。
- 下载文件默认保存到 `%LOCALAPPDATA%\CYBROWSER\downloads`，可查看下载记录。
- 在 Windows 中对 PDF 选择“打开方式”并指定 `CYBROWSER.exe` 后，浏览器会将文件路径转换为 `file:///` 地址并显示。若要双击 PDF 时直接打开，请将 CYBROWSER 设置为 `.pdf` 文件的默认应用。
- 设置页提供“设置为默认浏览器”入口，点击后将 CYBROWSER 注册到当前用户的 Windows 默认应用列表，并打开系统设置。然后在 Windows 中选择 CYBROWSER 处理 HTTP、HTTPS 和 HTML 文件。便携版移动位置后需要重新点击此入口。

### v2.0.0 更新

- 支持 Windows“打开方式”传入本地 PDF 路径，自动转换为 `file:///` 地址；也支持本地 HTML 文件路径。
- 设置页新增默认浏览器入口，可将 CYBROWSER 注册到 Windows 默认应用列表，并打开系统设置以选择 HTTP、HTTPS 与 HTML 文件的默认应用。

以上描述对应当前源码中的功能。当前版本仍处于早期阶段，欢迎通过 Issues 反馈兼容性问题和功能建议。

## 后续计划

以下是改进目标，尚未包含在 v2.0.0 中，具体实现与时间将根据测试和反馈调整：

1. **AI 基础能力**：设计可选的页面摘要、问答和搜索辅助；明确用户触发、数据发送范围与隐私提示。
2. **AI 服务接入**：支持可配置的模型服务，先完成凭据保存、失败处理和使用成本提示，再逐步扩展能力。
3. **插件机制**：设计扩展接口、权限声明及隔离机制，先验证小范围插件，再考虑插件管理和分发。
4. **发布质量**：补充自动化构建、独立设备测试、版本更新和 Windows 代码签名。

## 从源码构建

源码仓库根目录就是本目录。准备 Visual Studio C++ 工具链、CMake，以及与项目匹配的 [CEF Windows x64 二进制发行包](https://cef-builds.spotifycdn.com/index.html)（当前使用 CEF 156.0.14）。CEF 发行包不包含在 Git 仓库中。

在 PowerShell 中运行，按实际安装情况修改生成器名称和 `CEF_ROOT`：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DUSE_SANDBOX=OFF -DCEF_ROOT="C:\path\to\cef_binary_windows64"
cmake --build build --config Release --target CYBROWSER
```

输出位于 `build\app\Release\`。将该目录的运行文件整体打包，并附上 CEF 的 `LICENSE.txt` 和 `CREDITS.html`。本项目包含修改自 CEF `cefsimple` 的代码，许可文本见 [LICENSE.txt](LICENSE.txt)。

