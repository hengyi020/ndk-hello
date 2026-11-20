# 零成本构建：GitHub Actions

不需要云电脑、不需要花钱。合并到 `main` 就自动编出 APK。

## 为什么可行

- 公共仓库的 Actions 完全免费；私有仓库每月也有免费额度（2000 分钟）
- runner 是 **4 核 16 GB**，比便宜的云服务器强
- Ubuntu 环境，装 NDK 只要两三分钟
- 编出来的 APK 挂在 workflow 的 Artifacts 里，手机浏览器直接下载

## 三步

1. GitHub 上建个仓库（Public 或 Private 都行）
2. 把 `ndk-hello/` 整个目录推上去
3. 打开仓库的 **Actions** 标签，等 `build-apk` 跑完（约 3–5 分钟）
   → 点开这次运行，页面底部 **Artifacts** 里下 `ndk-hello-apk.zip`
   → 解压出 `ndk-hello.apk`，装到手机上

```bash
git init
git add .
git commit -m "ndk hello"
git branch -M main
git remote add origin https://github.com/<你的用户名>/<仓库名>.git
git push -u origin main
```

## 想手动触发

Actions 页面 → 选 `build-apk` → 右侧 **Run workflow**。

## 注意

- 仓库里有 `.github/workflows/build.yml` 才会跑，别忘了这个文件
- 私有仓库超出免费额度会停止构建，`retention-days: 7` 让产物 7 天后自动清掉
