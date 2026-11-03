# CYBROWSER 宣传网站

静态网站，无需构建。将本目录的 `index.html`、`styles.css`、`main.js` 和 `assets/` 一起部署到任意静态托管服务即可。

本地预览：在本目录运行 `python -m http.server 4173`，然后访问 `http://localhost:4173/`。

## 下载流程

网站通过 GitHub Releases API 查询 `Younglllboy/CY_Browser` 的最新正式发布版，选择文件名匹配 `CYBROWSER*win*x64*.zip` 的资源，下载按钮指向该资源的 `browser_download_url`。若 API 暂时不可用，按钮回退至已验证的 v1.0.2 ZIP。页面另有 GitHub 最新发布页入口。

采用便携版方案：下载完整 ZIP，解压到固定位置，运行其中的 `CYBROWSER.exe`。桌面快捷方式可由用户右键 EXE 后选择“发送到 → 桌面快捷方式”。不要单独移动 EXE；浏览器依赖同目录的 CEF 文件。当前版本只有 Windows x64 发布包，网站没有声称提供安装器或自动启动。

发布新版本时，请确认 GitHub Release 含有命名符合上述模式的 Windows x64 ZIP。若需要维持 API 不可用时的直链回退，请同时更新 `index.html` 和 `main.js` 中的版本化 URL。
