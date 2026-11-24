# Neverlose Injector

Neverlose 风格的加载器 / 注入器研究项目，包含两个独立实现：

| 目录 | 实现 | 技术栈 |
|---|---|---|
| [`old/`](old/) | 初代加载器（WPF 版） | C# / .NET 8 / WPF + C++ Win32 注入器 |
| [`new/`](new/) | 重建版加载器（原生版） | C++20 / DirectX 11 / Dear ImGui |

两个版本互不依赖，可分别构建、单独运行。仓库只保存源码与资源，**不包含任何 `.exe` / `.dll` 等可执行文件**，发布产物需自行编译。

---

## old/ — WPF 加载器

拥有 Neverlose 风格界面与动画的 DLL 注入器加载器，支持将 DLL 加密打包进单个 exe，运行时自动启动目标程序并完成注入。

- **界面**：雨滴玻璃拟态背景、卡片展开启动动画、注入进度动画，双主题（雨滴玻璃 / 经典深色）
- **个性化**：可更换挂件、自定义头像（头像文件名自动作为昵称）
- **打包**：AES-256 加密 → ConfuserEx 混淆 → 单文件打包
- **注入器**：`old/injtrol/` 内的 C++ Win32 注入器（CreateRemoteThread + LoadLibraryW / Manual Map）

构建（需要 .NET 8 SDK）：

```powershell
cd old
dotnet publish -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true
```

详细说明见 [`old/README.md`](old/README.md)。该版本的密钥文件 `CryptoKeys.g.cs`、负载目录 `cheat/` 与一键打包工具链按要求不入库。

---

## new/ — 原生重建版

按原版二进制反汇编证据逐项重建的原生加载器：DirectComposition 圆角/透明度、自绘桌面毛玻璃、ImGui 窗口阴影图集、原版图标与动画曲线均以原版常量为准。

- **渲染**：DirectX 11 + Dear ImGui 1.91.9 WIP，WIC 纹理加载，DirectComposition 合成
- **外观**：设置页可选 **模糊 / 液体玻璃 / 纯色**；毛玻璃由 DXGI Desktop Duplication 抓取窗口背后的桌面，在 GPU 上降采样 + 两遍可分离高斯完成；液体玻璃额外做圆角折射、色散与边缘高光
- **页面**：启动 / 连接、Dashboard、游戏启动弹窗、分支与更新日志、设置页
- **默认配置**：昵称 `Awa Subaru`，默认头像为 `new/assets/default_avatar.jpg`（由 `test.jpg` 缩放压缩得到）
- **注入边界**：`new/src/inject_boundary.cpp` 保持桩实现，不执行跨进程注入
- **预览模式**：支持 `--preview --page <页面> --capture <png>` 离屏截图，便于像素级比对

构建（需要 LLVM-MinGW 的 `clang++` 在 PATH 中）：

```powershell
cd new
.\build.bat
```

运行要求：`newnlinjector.exe` 需与 `assets/` 目录同级（资源通过模块目录查找）；用户数据默认写入 `%LOCALAPPDATA%\NLLoader`。

常用命令行参数：

| 参数 | 说明 |
|---|---|
| `--preview` | 预览模式，不触发实际运行逻辑 |
| `--page <名称>` | 指定预览页面，如 `connecting`、`profile-hover`、`settings` |
| `--data-dir <路径>` | 覆盖用户数据目录 |
| `--capture <路径>` | 渲染稳定后保存 PNG 截图并退出 |

---

## 仓库结构

```
Neverlose-Injector/
├── old/                  # 初代 WPF 加载器（C# + injtrol C++ 注入器）
│   ├── Assets/           # 图标、卡片图、头像
│   ├── injtrol/          # C++ 注入器源码
│   ├── *.xaml(.cs)       # 各窗口
│   └── README.md         # 旧版详细说明
├── new/                  # 原生重建版（C++20 + ImGui + D3D11）
│   ├── assets/           # 字体、图标、卡片图、默认头像
│   ├── src/              # 加载器源码
│   ├── third_party/      # Dear ImGui
│   └── build.bat
├── LICENSE
└── README.md
```

---

## 更新记录

### 2026-11-24 — new/ 原生版：GPU 毛玻璃 + 液体玻璃

**新增**

- **桌面毛玻璃改为 GPU 实现**：用 DXGI Desktop Duplication 抓取窗口背后的整块桌面帧 → 1/6 降采样 → 两遍可分离高斯模糊，替换原来的系统亚克力 / GDI 截屏 + CPU 模糊
- **新增「液体玻璃」外观**：移植 [imgui-liquid-glass](https://github.com/SoyBeanMilkx/imgui-liquid-glass) 的着色器算法到 HLSL（圆角矩形 SDF、边缘折射位移、七点色散、边缘内阴影/辉光/高光），按窗口原始分辨率合成；同时采样原始桌面帧与模糊帧混合，保留边缘折射细节
- **设置页外观三选一**：模糊 / 液体玻璃 / 纯色，默认液体玻璃，保存后立即生效；旧配置（`solid_appearance`）自动换算，无需手动修改
- **exe 图标**：用 `assets/neverlose_logo.png` 生成资源（`src/app.rc` + `src/app.ico`），编译出的 exe 在资源管理器和任务栏都有图标

**修复**

- **修复开启模糊后界面变纯色 / 全黑**：D3D11 中同一张纹理仍绑为渲染目标时再绑它的 SRV 会被静默丢弃，导致采样全 0；改为先切渲染目标再绑 SRV
- **修复拖动窗口时背景错位、拖影**：改为缓存整帧桌面画面，窗口移动时从同一帧按新矩形重新取样，不必等下一帧截屏
- **修复模糊导致的卡顿**：采集与模糊全部搬到 GPU，采集用 0 超时非阻塞调用，桌面画面无变化时直接复用上一帧结果
- **修复切换外观后仍显示上一种效果**：切换时强制重新合成，不再复用上一次的结果纹理
- **修复小窗口下玻璃边缘发虚**：合成分辨率从 1/6 提升到窗口原始分辨率，并按像素覆盖率做边缘抗锯齿

---

## 免责声明与法律声明

- 本项目**仅供学习与技术研究之用**，用于演示 UI 开发、Win32 API、图形渲染、加解密与 .NET 保护技术，不提供任何作弊功能本体。
- 本项目不包含任何游戏作弊代码；两个加载器均为通用 DLL 注入器外壳，与游戏无任何耦合。
- 使用本工具向任何第三方程序（包括但不限于受反作弊系统保护的游戏）注入代码，可能违反该程序的**最终用户许可协议与服务条款**，由此产生的一切后果（包括但不限于账号封禁）由**使用者自行承担**，作者概不负责。
- 本项目与 Valve Corporation、完美世界及任何游戏厂商**无任何关联**；CS:GO、CS2 等名称与商标归其各自所有者所有。
- 使用者须遵守所在国家/地区的法律法规。**不得**将本项目用于任何非法用途，**不得**用于破坏计算机信息系统或侵犯他人合法权益。
- 本项目按"现状"提供，不附带任何明示或默示的担保。源代码以 [AGPL-3.0](LICENSE) 协议开源，任何二次分发须遵守该协议。

## License

[GNU AGPL-3.0](LICENSE)
