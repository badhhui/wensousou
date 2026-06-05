# 文搜搜

文搜搜是一款面向 UOS ARM64 离线电脑的本地文档全文检索工具。主程序使用 Qt
Widgets 和 C++17，索引使用 SQLite FTS5 与 `simple` 中文分词扩展，文本抽取由随包
携带的 Apache Tika Worker 完成。

## 功能

- 同时维护多个不重叠的目录，可在设置中选择是否在启动时执行增量扫描，默认关闭。
- 默认支持 `doc`、`docx`、`wps`、`xls`、`xlsx`、`et`、`ppt`、`pptx`、`pdf`；
  `txt` 可在设置中手动开启。
- 支持中文全文搜索、目录、文件类型和修改日期筛选。为控制索引体积和查询耗时，
  默认不建立拼音索引。
- 搜索结果展示首处命中内容并标黄，可通过行内图标预览、打开文件或打开文件夹。
- 搜索结果显示总命中数，支持页码跳转和每页数量切换。
- 搜索结果默认按相关度排列；点击修改时间表头可切换时间降序、升序和恢复相关度排序。
- 首页使用醒目的搜索工作台，并可折叠搜索区扩大结果空间。目录维护、索引统计和失败文件
  原因统一放在索引管理页面。
- 正文预览使用独立弹窗，并标黄展示每一处命中内容，可跳转到上一处或下一处。
- 建立索引时在底部状态栏显示当前文件、已处理数量、失败数量和总文件数量。
- 搜索在独立后台线程执行，查询期间显示进度提示；新查询会中断并替换旧查询。
- 支持重试失败文件、超大文件和解析超时保护。
- PDF 仅索引已有文字层，不执行 OCR。
- 默认跳过超过 `10 MB` 的文件，每个文件最多索引 `200 万` 字符，单文件解析超时为
  `30 秒`。可以在索引设置中调整阈值。

## 在联网 UOS 1070 ARM64 机器构建

联网 UOS 1070 构建机首次使用时，先安装基础编译工具、XCB 和字体渲染开发包：

```bash
chmod +x scripts/*.sh
./scripts/install-uos-build-deps.sh
```

Qt 依赖不完整时，构建脚本也会一次性列出缺失项。安装完成后执行：

```bash
chmod +x scripts/*.sh parser/build.sh packaging/wensousou.sh
./scripts/build-offline-kit.sh
```

脚本会准备 SQLite、Tika、Temurin JDK/JRE，必要时编译 Qt 5.15.2，然后输出：

```text
dist/offline-kit/
  wensousou_1.0.20_arm64.deb
  check-target.sh
  INSTALL-OFFLINE.md
  SHA256SUMS
  licenses/
```

如果希望提前下载全部外部归档，再将整个项目目录复制到 UOS 构建机，可在联网电脑执行：

```bash
./scripts/prefetch-offline-deps.sh
```

归档会保存到 `third_party/cache/`。该目录默认不提交到 Git，但复制项目目录时需要一并复制。

如果旧版脚本在 Qt 配置阶段报错 `Could not find qmake spec ''`，请更新
`scripts/build-qt.sh` 后重新运行。脚本只构建应用所需的 `qtbase`，显式使用
`linux-g++` mkspec，强制将生成的 `qt.conf` 传递给 bootstrap qmake，并自动清理
上一次失败留下的 Qt 源码、构建目录和不完整安装。

UOS 1070 仓库提供的 `xcb-util` 版本为 `0.3.8.1`。构建脚本会自动应用兼容补丁，
将 Qt 5.15.2 的 `xcb-util` 检测门槛从 `0.3.9` 调整为 `0.3.8`。

Qt 会启用系统 `fontconfig`、FreeType 与 DBus，并将 Fcitx Qt5 输入法插件随应用打包。
构建脚本使用标记文件识别旧 Qt 产物；
旧产物缺少标记时会自动重新编译，避免安装后出现控件存在但文字不显示的问题。

为兼容 Qt 5.15 在旧系统上的 bootstrap qmake，Qt 源码、中间文件和安装结果会写入
项目同级的纯 ASCII 路径，不再占用根分区的 `/tmp`：

```text
../wensousou-qt-build-5.15.2-<UID>/
../wensousou-qt-5.15.2/
```

如需使用容量更充足的数据盘，可指定纯 ASCII 路径：

```bash
export WENSOUSOU_QT_WORK_ROOT=/data/wensousou-qt-build
export QT_PREFIX=/data/wensousou-qt-5.15.2
./scripts/build-offline-kit.sh
```

配置失败时，脚本会自动打印 `config.log` 的末尾内容。

## 在断网 UOS ARM64 机器重新编译

断网机器必须保留完整源码目录和 `third_party/cache/`。如果之前已经成功编译过 Qt，
还应保留项目同级目录 `../wensousou-qt-5.15.2/`，这样不需要再次编译 Qt。

执行：

```bash
chmod +x scripts/*.sh parser/build.sh packaging/*.sh
./scripts/build-offline-local.sh
```

该命令禁止联网下载。缓存不完整时会直接列出缺少的文件。构建结果仍然输出到：

```text
dist/offline-kit/wensousou_1.0.20_arm64.deb
```

如果断网机器缺少编译工具或 XCB 开发包，需要通过 U 盘补齐对应的 `.deb` 安装包；
仅安装旧版 `wensousou` 应用无法提供编译环境。

## 在 Apple Silicon Mac 构建 ARM64 安装包

如果 UOS 机器已经断网并且缺少开发包，可以在 Apple Silicon Mac 上通过 Docker
使用 Debian 10 ARM64 容器构建。容器的 glibc 为 `2.28`，生成的安装包可以复制到
UOS 1022 离线安装。

启动 Docker Desktop 后执行：

```bash
chmod +x scripts/build-on-mac-docker.sh
./scripts/build-on-mac-docker.sh
```

首次构建会编译 Qt 5.15.2，时间较长。后续构建复用 `.docker-cache/` 中的 Qt。
输出仍然位于：

```text
dist/offline-kit/wensousou_1.0.20_arm64.deb
```

## 开发构建

已有 Qt 5.15 开发环境时：

```bash
./scripts/prepare-deps.sh
./parser/build.sh
cmake -S . -B build/dev -DCMAKE_PREFIX_PATH=/path/to/qt
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

本仓库包含用户提供的 UOS ARM64 `resources/libsimple-linux-aarch64.so`。其 SHA-256 为：

```text
1d99176872686c54f386605fb4cdafcc83fc1bb0073251c5b2f46343a5515f81
```
