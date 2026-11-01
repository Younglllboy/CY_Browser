# CYBROWSER 源码

此目录是基于 CEF 156.0.14 的 Windows x64 CMake 工程。CEF 二进制发行包需要单独获取，不包含在此源码仓库中。上级目录的 Visual Studio 空窗口示例不属于此工程。

在 CMD 中执行：

```cmd
cd /d <源码目录>
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DUSE_SANDBOX=OFF -DCEF_ROOT="C:\path\to\cef_binary_windows64"
cmake --build build --config Release --target CYBROWSER
build\app\Release\CYBROWSER.exe
```

将 `CEF_ROOT` 改成实际解压的 CEF Windows x64 二进制发行包目录。更换 CEF 版本或生成器时，请使用新的构建目录。`build/`、`out/`、`.vs/` 和 `dist/` 均不纳入 Git。

CEF 包自带说明以 Visual Studio 2022 为例。本机目前安装 Visual Studio 2026；能否完成构建以实际配置和编译结果为准。首次验证使用 `USE_SANDBOX=OFF`。

