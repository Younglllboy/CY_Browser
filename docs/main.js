const repo = "Younglllboy/CY_Browser";
const fallbackUrl = `https://github.com/${repo}/releases/download/v1.0.2/CYBROWSER-v1.0.2-win-x64.zip`;
const links = [...document.querySelectorAll("[data-download]")];
const status = document.querySelector("#download-status");

// Resolve the actual asset name from GitHub instead of assuming future releases
// will keep the current versioned file naming convention.
async function latestRelease() {
  const response = await fetch(`https://api.github.com/repos/${repo}/releases/latest`, {
    headers: { Accept: "application/vnd.github+json" },
  });
  if (!response.ok) throw new Error(`GitHub API: ${response.status}`);
  const release = await response.json();
  const asset = release.assets?.find(item => /^CYBROWSER.*win.*x64.*\.zip$/i.test(item.name));
  if (!asset) throw new Error("No Windows x64 ZIP asset in latest release");
  return { url: asset.browser_download_url, version: release.tag_name };
}

let resolvedRelease;
latestRelease().then(result => {
  resolvedRelease = result;
  links.forEach(link => { link.href = result.url; });
  document.querySelector("#release-label").textContent = `Windows 64 位 · 当前版本 ${result.version}`;
  status.textContent = `Windows 64 位 · ${result.version} · 免费下载`;
}).catch(() => {
  // The verified v1.0.2 link remains usable when GitHub's API is unavailable.
  links.forEach(link => { link.href = fallbackUrl; });
});

links.forEach(link => link.addEventListener("click", async event => {
  if (resolvedRelease) return;
  event.preventDefault();
  status.textContent = "正在获取最新发布版本…";
  try {
    const result = await latestRelease();
    resolvedRelease = result;
    links.forEach(item => { item.href = result.url; });
    status.textContent = `即将下载 ${result.version} · Windows 64 位`;
    window.location.assign(result.url);
  } catch {
    status.textContent = "暂时无法确认最新版本，正在下载已验证的 v1.0.2。";
    window.location.assign(fallbackUrl);
  }
}));
