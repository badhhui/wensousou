# Apple Silicon Mac Docker 构建指南

本文说明如何在 Apple Silicon Mac 上用 Docker 构建 UOS ARM64 离线安装包。Mac 只作为宿主机，真正的编译、测试和打包都发生在 `linux/arm64` Debian 10 容器中。

## 适用场景

- 目标机是 UOS 专业版 ARM64，无法联网或缺少开发依赖。
- 开发机是 Apple Silicon Mac，已安装 Docker Desktop。
- 需要生成可复制到离线 UOS 机器安装的 `.deb` 包。

## 构建原理

```text
Apple Silicon Mac
  |
  | scripts/build-on-mac-docker.sh
  v
linux/arm64 Debian 10 Docker 容器
  |
  | 编译 Qt / C++ / Java Worker / SQLite
  | 运行测试和目标机预检
  v
dist/offline-kit/wensousou_版本号_arm64.deb
```

选择 Debian 10 是为了让容器里的 glibc 维持在 `2.28`，避免生成的原生二进制在 UOS 1022 目标机上因为 glibc 版本过高而无法启动。

## 前置条件

1. Apple Silicon Mac，架构为 `arm64`。
2. Docker Desktop 已安装并正在运行。
3. 当前仓库完整，包含 `resources/libsimple-linux-aarch64.so`。
4. 首次构建需要网络，后续可复用 `.docker-cache/`。

检查：

```bash
uname -s
uname -m
docker info
```

预期：

```text
Darwin
arm64
```

## 一键构建

在仓库根目录执行：

```bash
chmod +x scripts/*.sh parser/build.sh packaging/*.sh
./scripts/build-on-mac-docker.sh
```

构建完成后输出：

```text
dist/offline-kit/
  wensousou_1.1.4_arm64.deb
  check-target.sh
  INSTALL-OFFLINE.md
  SHA256SUMS
  licenses/
```

## 脚本做了什么

`scripts/build-on-mac-docker.sh` 会：

1. 检查当前机器是否为 Apple Silicon Mac。
2. 检查 Docker Desktop 是否可用。
3. 使用 `packaging/docker/Dockerfile.buster-arm64` 构建 `linux/arm64` 编译镜像。
4. 将仓库挂载到容器的 `/workspace`。
5. 将 `.docker-cache/` 挂载到容器的 `/docker-cache`，用于缓存 Qt 编译结果。
6. 在容器中执行 `scripts/build-offline-local.sh`。

容器内的构建流程会继续执行：

1. 准备 SQLite amalgamation、Tika、Temurin JDK/JRE。
2. 编译或复用 Qt 5.15.2。
3. 编译 `parser-worker.jar`。
4. 运行 Parser Worker 冒烟测试。
5. 运行启动器输入法插件测试。
6. CMake 编译 Qt 主程序。
7. 执行 Qt Test 测试。
8. 打包 `/opt/wensousou` 和启动脚本为 ARM64 `.deb`。
9. 生成 `SHA256SUMS`。

## 缓存目录

```text
.docker-cache/
  wensousou-qt-5.15.2/
  wensousou-qt-build/
```

`.docker-cache/` 不提交到 Git。首次构建 Qt 会比较慢；后续构建会复用该目录，速度会明显变快。

如果怀疑 Qt 缓存损坏，可以删除后重建：

```bash
rm -rf .docker-cache/wensousou-qt-5.15.2 .docker-cache/wensousou-qt-build
./scripts/build-on-mac-docker.sh
```

## 离线依赖缓存

项目构建中下载的外部归档会放在：

```text
third_party/cache/
```

该目录同样不提交到 Git。Mac Docker 构建通常会在容器内准备这些依赖；如果要迁移到离线 UOS 机器重新编译，需要把 `third_party/cache/` 一并复制过去。

## 手动预检安装包

构建完成后可以在 Docker 容器里再次运行目标预检：

```bash
docker run --rm --platform linux/arm64 \
  -v "$PWD:/workspace" \
  wensousou-builder-buster-arm64 \
  bash -lc 'cd /workspace/dist/offline-kit && ./check-target.sh wensousou_1.1.4_arm64.deb'
```

通过时会看到：

```text
Target preflight passed. Install with:
  sudo dpkg -i wensousou_1.1.4_arm64.deb
```

## 复制到离线 UOS 安装

将整个 `dist/offline-kit/` 复制到 UOS ARM64 目标机，然后执行：

```bash
cd offline-kit
./check-target.sh wensousou_1.1.4_arm64.deb
sudo dpkg -i wensousou_1.1.4_arm64.deb
wensousou --self-check
```

## 常见问题

### Docker 没启动

报错：

```text
Docker Desktop is not running.
```

处理：启动 Docker Desktop 后重新执行构建脚本。

### 构建出的包版本号不对

版本号需要同时修改：

- `CMakeLists.txt`
- `src/main.cpp`
- `scripts/build-offline-kit.sh`
- `scripts/build-on-mac-docker.sh`
- `scripts/check-target.sh`
- `README.md`
- `INSTALL-OFFLINE.md`

修改后重新执行：

```bash
./scripts/build-on-mac-docker.sh
```

### 目标机打不开程序

先在目标机运行：

```bash
wensousou-diagnose
wensousou --self-check
```

重点看：

- `libsimple.so` 是否可加载。
- Qt XCB 插件依赖是否完整。
- Fcitx Qt5 输入法插件依赖是否完整。
- JRE 和 Tika Worker 是否可运行。

### 不要提交的目录

以下目录是构建产物或第三方缓存，已经由 `.gitignore` 排除：

```text
build/
dist/
.docker-cache/
third_party/cache/
third_party/runtime/
third_party/qt-5.15.2/
parser/build/
```

## 适配其他项目的做法

核心思路是：不要让 Mac 直接猜目标 Linux 环境，而是把目标构建环境固定在 Docker 镜像里。

通用结构可以是：

```text
scripts/build-on-mac-docker.sh
packaging/docker/Dockerfile.<target>
scripts/build-offline-local.sh
scripts/check-target.sh
```

对旧 glibc 目标机尤其重要：构建镜像的 glibc 版本应小于或等于目标机，否则程序可能无法运行。
