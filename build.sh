#!/usr/bin/env bash
# 手工构建最小 NDK APK：clang -> aapt2 -> zip -> zipalign -> apksigner
# 用法： ./build.sh [arm64-v8a|armeabi-v7a]
set -euo pipefail

ABI="${1:-arm64-v8a}"
case "$ABI" in
  arm64-v8a)   TRIPLE=aarch64-linux-android;        API=26 ;;
  armeabi-v7a) TRIPLE=armv7a-linux-androideabi;     API=26 ;;
  *) echo "不支持的 ABI: $ABI"; exit 1 ;;
esac

SDK="${ANDROID_SDK_ROOT:-$HOME/android-sdk}"
NDK="${ANDROID_NDK_HOME:-$(ls -d "$SDK"/ndk/* 2>/dev/null | sort -V | tail -1 || true)}"
BT="$(ls -d "$SDK"/build-tools/* 2>/dev/null | sort -V | tail -1 || true)"
PLATFORM="$(ls -d "$SDK"/platforms/android-* 2>/dev/null | sort -V | tail -1 || true)"

for v in NDK BT PLATFORM; do
  if [ -z "${!v}" ]; then echo "缺 $v，先用 sdkmanager 装（见 README）"; exit 1; fi
done

CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/${TRIPLE}${API}-clang"
# 兜底：环境里的 SDK 可能没有可用 NDK，退回自己装的那份
if [ ! -x "$CC" ] && [ -d "$HOME/android-sdk/ndk" ]; then
  NDK="$(ls -d "$HOME"/android-sdk/ndk/* | sort -V | tail -1)"
  CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/${TRIPLE}${API}-clang"
fi
[ -x "$CC" ] || { echo "找不到编译器: $CC"; exit 1; }
echo "NDK = $NDK"

OUT="$PWD/build"
CRT="$PWD/jni"
rm -rf "$OUT"; mkdir -p "$OUT/lib/$ABI"

echo "==> 1/5 编译 native 库"
"$CC" -shared -fPIC -O2 -Wl,--build-id=sha1 \
  -o "$OUT/lib/$ABI/libmain.so" \
  "$CRT/main.c" "$CRT/android_native_app_glue.c" \
  -I"$CRT" \
  -landroid -llog -lEGL -lGLESv3 -lm

echo "==> 2/5 打包资源清单"
"$BT/aapt2" link -o "$OUT/base.apk" \
  -I "$PLATFORM/android.jar" \
  --manifest AndroidManifest.xml \
  --min-sdk-version 26 --target-sdk-version 34

echo "==> 3/5 塞入 so"
python3 - "$OUT/base.apk" "$OUT/lib/$ABI/libmain.so" "$ABI" <<'PY'
import sys, zipfile, os
apk, so, abi = sys.argv[1], sys.argv[2], sys.argv[3]
with zipfile.ZipFile(apk, "a", zipfile.ZIP_DEFLATED) as z:
    z.write(so, f"lib/{abi}/libmain.so")
print("   lib/%s/libmain.so 已写入" % abi)
PY

echo "==> 4/5 对齐"
"$BT/zipalign" -f -p 4 "$OUT/base.apk" "$OUT/aligned.apk"

echo "==> 5/5 签名"
KS="${KEYSTORE:-$HOME/.dsh-port/port.jks}"
if [ ! -f "$KS" ]; then
  mkdir -p "$(dirname "$KS")"
  echo "   生成密钥库 $KS（密码 dshport，仅测试用）"
  keytool -genkeypair -v -keystore "$KS" -alias port \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass dshport -keypass dshport \
    -dname "CN=DSH Port, O=DSH, C=CN" >/dev/null 2>&1
fi
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:dshport --key-pass pass:dshport \
  --out "$PWD/ndk-hello.apk" "$OUT/aligned.apk"

echo
echo "完成: $PWD/ndk-hello.apk"
ls -lh "$PWD/ndk-hello.apk"
echo "安装: adb install -r ndk-hello.apk   或直接拷到手机点击安装"
