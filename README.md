# ndk-hello —— 最小移植链条验证工程

一个不含 Java、不含 Gradle 的 Android 原生 APK：
NativeActivity + EGL + GLES3，画一个会变色的三角形。

目的是**在这台 65 分钟的免费云电脑上，一次跑完整条链路**：交叉编译 → 打包 → 对齐 → 签名 → 真机运行。

## 环境（10 GB 磁盘够用）

```bash
# 1. JDK（apksigner 依赖）
sudo apt-get update && sudo apt-get install -y openjdk-17-jre-headless unzip

# 2. Android 命令行工具
mkdir -p ~/android-sdk/cmdline-tools && cd ~/android-sdk/cmdline-tools
wget -q https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip -q commandlinetools-linux-*.zip && mv cmdline-tools latest

# 3. 只装必需的组件（约 4 GB）
export ANDROID_SDK_ROOT=$HOME/android-sdk
yes | ~/android-sdk/cmdline-tools/latest/bin/sdkmanager --sdk_root=$ANDROID_SDK_ROOT \
  "platform-tools" "platforms;android-34" "build-tools;34.0.0" "ndk;26.1.10909125"
```

装完就占用约 5 GB，10 GB 的云盘够，但**别装 Android Studio，也别装 Unity**。

## 构建

```bash
cd ndk-hello
chmod +x build.sh
./build.sh                 # 默认 arm64-v8a
```

产出 `ndk-hello.apk`，直接拷到手机安装。

## 每一步在干什么

| 步骤 | 工具 | 学到的东西 |
|---|---|---|
| 1 | `aarch64-linux-android26-clang` | 交叉编译：target triple 决定 ABI 和最低 API |
| 2 | `aapt2 link` | AndroidManifest 被编译成二进制 XML，资源索引怎么生成 |
| 3 | `zip` | APK 就是一个 zip，`lib/<abi>/` 目录决定装哪个架构 |
| 4 | `zipalign` | 4 字节对齐，影响运行时 mmap 效率和安装校验 |
| 5 | `apksigner` | v2/v3 签名块，Android 11+ 装包必需 |

## 排查

```bash
adb logcat -s ndkhello
```

- `shader compile failed` → GLES 版本或驱动问题
- `no matching EGLConfig` → 设备不支持 GLES3（这台不会）
- 安装报 `INSTALL_FAILED_NO_MATCHING_ABIS` → ABI 选错了
