# 编译与运行

本文记录 Windows Container View 的编译、部署和运行步骤，以及排查过程中确认的几个关键点。

架构说明见仓库根目录的 `README.adoc`。

---

## 1. 环境要求

| 组件 | 说明 |
|---|---|
| Visual Studio 2019 或更高 | 工程使用 v142 工具集，需安装「使用 C++ 的桌面开发」工作负载 |
| SDL2 2.32.2 开发库 | 见第 2 节，需放在仓库根目录 |
| Docker | 用于运行 Windows 容器（进程隔离或 Hyper-V 隔离） |
| Windows 10/11 或 Windows Server | 需启用 Containers 功能 |

运行端还需要 **VC++ 2019 x64 运行库**。

---

## 2. SDL2

`SDL2/` 是第三方二进制 SDK（SDL2 2.32.2 + SDL2_image，约 19 MB / 108 个文件），**已纳入版本控制**，克隆下来即可编译，不需要额外下载。只保留了编译和运行需要的部分，上游发行包里的 `docs/`、`cmake/`、`lib/x86/` 和顶层说明文件没有收录。

结构：

```
SDL2/
├── include/SDL2/              头文件（92 个）。源码里写的是 #include <SDL2/SDL.h>，
│                              include/ 下没有平铺的 SDL.h，前缀不能省
└── lib/x64/
    ├── SDL2.lib  SDL2main.lib  SDL2_image.lib     链接用
    ├── SDL2test.lib
    ├── SDL2.dll  SDL2_image.dll                   运行用
    ├── SDL2.pdb  SDL2_image.pdb                   调试符号，可删
    └── optional/
        └── libavif-16.dll  libtiff-5.dll  libwebp-7.dll  libwebpdemux-2.dll
            + 各自的 LICENSE
```

`Projects/Sandbox-Server/Sandbox-Server.vcxproj` 通过 `$(SolutionDir)SDL2\include` 和 `$(SolutionDir)SDL2\lib\x64` 引用它，**不依赖绝对路径**，仓库放在哪都能编译。

升级 SDL2 时，从 https://github.com/libsdl-org/SDL/releases 下载对应版本的 `SDL2-devel-<版本>-VC.zip` 和 `SDL2_image-devel-<版本>-VC.zip`，取其中的 `include/SDL2/`、`lib/x64/` 覆盖进来即可。

运行目录还需要 `SDL2.dll`、`SDL2_image.dll` 及 `lib/x64/optional/` 下的四个 DLL，见第 3 节。

---

## 3. 编译

用 Visual Studio 打开 `Sandbox.sln`，选 `Debug|x64`，生成解决方案。

或者命令行：

```powershell
msbuild Sandbox.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m
```

三个工程的产物都落在 `Output\x64\Debug\`：

| 工程 | 产物 | 角色 |
|---|---|---|
| `Sandbox` | `Sandbox.exe` | 入口，按命令行参数决定跑服务端还是客户端 |
| `Sandbox-Server` | `Sandbox-Server.dll` | 宿主机侧：RPC 服务端 + SDL2 显示 |
| `Sandbox-Client` | `Sandbox-Client.dll` | 容器侧：抓屏 + 输入注入 |

`Sandbox.exe` 通过导入库隐式链接两个 DLL，**运行目录必须同时具备三个文件和全部 SDL2 相关 DLL**：

```
Sandbox.exe  Sandbox-Client.dll  Sandbox-Server.dll
SDL2.dll  SDL2_image.dll
libavif-16.dll  libtiff-5.dll  libwebp-7.dll  libwebpdemux-2.dll
```

---

## 4. 运行

### 4.1 服务端（宿主机）

服务端**必须运行在有桌面会话的机器上**，因为它要创建 SDL 窗口显示画面。Server Core 上没有交互式桌面，`SDL_Init(SDL_INIT_VIDEO)` 会失败，窗口出不来。

```powershell
cd Output\x64\Debug
.\Sandbox.exe
```

打印 `Init` 后停住，窗口出现，等待客户端连接。

> `main.cpp` 里 `SandboxInitServer()` 的异常是被静默 `catch` 后 `return 0` 的，RPC 注册失败时进程会**一声不吭地退出**。别只看「没报错」就认为它活着，用 `Get-Process Sandbox` 确认。

### 4.2 放行防火墙

服务端监听固定端口 **5001**（见第 5 节）。宿主机上执行：

```powershell
New-NetFirewallRule -DisplayName "Sandbox RPC 5001" -Direction Inbound `
    -Protocol TCP -LocalPort 5001 -Action Allow -Profile Any
```

`-Profile Any` 不能省——虚拟网卡（VMware host-only、docker NAT 等）常被判成 Public 配置文件。

验证监听：

```powershell
Get-NetTCPConnection -State Listen -LocalPort 5001 | Select LocalAddress, LocalPort
```

应看到 `0.0.0.0:5001`。

### 4.3 客户端（容器）

容器镜像最低要求 **Server Core**（Nano Server 砍掉了 Win32k，无法创建窗口）。进程隔离和 Hyper-V 隔离都可以。

把 `Output\x64\Debug\` 挂载进容器，以 `client <宿主机IP>` 参数启动：

```
docker run --rm -ti --isolation process ^
  -v C:\path\to\Output\x64\Debug:C:\Sandbox:ro ^
  mcr.microsoft.com/windows/servercore:20H2 ^
  C:\Sandbox\Sandbox client <宿主机IP>
```

`<宿主机IP>` 是**运行 Sandbox.exe 那台机器**上、容器可达的地址：

- 容器和 Sandbox.exe 在同一台机器 → 容器的默认网关，即宿主 `vEthernet (nat)` 的地址
- 跨机器（如容器在 VM 里、Sandbox.exe 在物理机上）→ 物理机在 VM 可达网络中的地址

先在容器里验证连通性：

```
powershell -c "Test-NetConnection <宿主机IP> -Port 5001"
```

`TcpTestSucceeded : True` 才算通。

> **容器镜像版本必须与宿主 Windows build 一致**，否则进程隔离模式启动会失败。`mcr.microsoft.com/windows/servercore:20H2` 对应宿主 build 19042。

### 4.4 预期结果

宿主机窗口显示容器内的桌面（蓝色背景 + cmd.exe 控制台），鼠标键盘可操作。

客户端**正常工作时是静默的**——帧循环里没有日志输出。以下都是正常现象：

| 输出 | 含义 |
|---|---|
| 无输出、进程持续运行 | 正常 |
| `No more visible windows, exiting.` | 容器内所有可见窗口关闭，正常退出 |

---

## 5. 端口配置

RPC 端点定义在 `Source/Interface/SandboxConfig.h`：

```c
#define SANDBOX_RPC_ENDPOINT L"5001"
```

**客户端和服务端必须使用同一个值**，修改后需要重新编译 `Sandbox-Client.dll` 和 `Sandbox-Server.dll` 两侧。

### 为什么用固定端口

原实现用 `RpcServerUseProtseq` + `RpcEpRegister` 注册**动态端口**，客户端必须先连宿主机 TCP 135 的 endpoint mapper 查询真实端口，再连那个动态端口。这带来三个问题：

1. 防火墙要同时放行 135 和一个每次重启都变的动态端口
2. 服务进程（无交互会话）不会弹出防火墙授权对话框，动态端口无法预先配置规则
3. 跨网段部署时 135 经常被拦

改用 `RpcServerUseProtseqEp` 指定固定端点后，客户端在 string binding 里直接写明端口，**不再需要 endpoint mapper，TCP 135 完全不用开**。

---

## 6. 排错

### `Exception 6ba encountered while sending data`

`0x6BA` = `RPC_S_SERVER_UNAVAILABLE`，网络层问题。客户端在第一次真正发起网络调用（`RpcBeginImage`）时才报，说明窗口站初始化和绑定都成功了，纯粹连不上。

按顺序查：

1. 宿主机 `Get-NetTCPConnection -State Listen -LocalPort 5001` 有没有输出 → 没有就是服务端没起来
2. 容器里 `Test-NetConnection <宿主机IP> -Port 5001` → False 就是防火墙或路由问题
3. IP 是否写错 → 见 4.3 的说明

注意客户端日志里的 `Done.` **不能证明连通性**：`RpcStringBindingCompose` 和 `RpcBindingFromStringBinding` 都是纯本地操作，真正的连接推迟到第一次调用才建立。

### 画面出来了，鼠标能动，但键盘打字没反应

如果按回车、退格、方向键有反应，**只有字母和数字打不出来**，那是宿主机上的**输入法**吃掉了字符键——输入法把字母吞进拼音组字缓冲，控制键原样放行。

宿主机的输入法（如中文拼音）在远端会话上组字本来就没有意义。`Rendering.cpp` 创建窗口后调用了 `SDL_StopTextInput()`，把输入法从窗口上摘掉，让所有物理按键都以原始扫描码到达。

如果这行被去掉后问题复现，就是它。

### 方向键、Home/End、小键盘回车无效

宿主侧的 SDL→DIK 扫描码表（`Rendering.cpp` 里的 `scanCodeToKeyNum`）把**扩展键**的高位置 1（`DIK_LEFT = 0xCB`、`DIK_UP = 0xC8`、`DIK_DELETE = 0xD3` 等）。客户端注入前必须剥掉这个标记位并打上 `KEYEVENTF_EXTENDEDKEY`，否则 `SendInput` 认不出扫描码直接丢弃。处理逻辑在 `Client.cpp` 的 `SBET_KeyEvent` 分支。

### 状态栏闪烁 / 对话框按钮闪烁

容器里**没有 DWM**（`tasklist /fi "imagename eq dwm.exe"` 无输出），没有合成器，也就没有可读的屏幕表面。`BitBlt` 读窗口 DC 得到的画面完全不可用（`printwnd.cpp` 里作者的原注释也记录了这点），**`WM_PRINT` 是唯一能拿到像素的途径**。

而 `PrintWindow` 的 `PRF_CHILDREN` 那一趟对主题化通用控件不可靠：

| 控件 | 现象 | 状态 |
|---|---|---|
| 状态栏 `msctls_statusbar32` | 只画背景，丢分区和文字 | 已修：`printwnd.cpp` 的 `PrintWindowTree()` 对每个后代显式重打一遍 |
| 对话框按钮 `Button` | 间歇消失或丢文字，约 12% 的帧 | **未解决**，无 DWM 环境下 `WM_PRINT` 的机制性限制 |

判断某个控件是否受影响：连续抓帧后逐帧比对，变化区域会精确落在该控件矩形内。

```
frame018 -> frame019 : 811 px  bbox=(591,572)-(690,599)   ← 正好是 OK 按钮
```

排查时试过但**无效**的方向，避免重复投入：

| 尝试 | 结果 |
|---|---|
| 清理暂存位图 `ghTempBitmap` | 无效（但消除了脏数据贴图，保留） |
| 抓屏限帧到 20fps | 对闪烁无效（但显著降 CPU，保留） |
| `BitBlt` 读窗口 DC | 画面完全不可用 |
| `PW_RENDERFULLCONTENT` | 无 DWM，不可用 |
| `RedrawWindow(RDW_UPDATENOW)` + `PrintWindow` | 反而让所有子窗口的 `PrintWindow` 返回失败 |
| 每个子窗口连打两次 | 无效 |
| 只重打状态栏类、不动其他控件 | 按钮仍然闪 |

另外注意 `PrintWindow` 的**返回值不可信**：`PrintWindow failed for child Edit` 会每帧出现，但编辑区画面完全正常。不能拿返回值当判据。

### 在容器里拖动窗口可以，但鼠标点击没反应

拖动依赖的是移动事件，点击走的是 `MOUSEEVENTF_*DOWN/UP`。定位由 `SetCursorPos` 完成，`SendInput` 里的 `dx/dy` 没有配 `MOUSEEVENTF_ABSOLUTE`，会被忽略——这是预期行为，位置由前者负责。

---

## 7. 调试提示

客户端 `Client.cpp` 的 `SBET_KeyEvent` 分支里有一行按键日志：

```cpp
cout << "scan:" << input.ki.wScan << " vk: " << input.ki.wVk << " received" << endl;
```

它在**注入之前**无条件打印，所以能精确区分「事件没到容器」和「到了但注入失败」：

| 现象 | 结论 |
|---|---|
| 按键没有对应日志 | 宿主侧 SDL 没产生事件，查宿主机输入层 |
| 有日志但目标程序无反应 | 事件到了，问题在注入侧（前台窗口、UIPI 权限） |

验证正常键值参考：

| 键 | 日志 |
|---|---|
| A | `scan:30 vk: 65` |
| Enter | `scan:28 vk: 13` |
| 左方向键 | `scan:75 vk: 37`（0xCB 剥掉高位后为 0x4B） |

确认无误后可以删掉这行，避免每个按键都刷屏。

---

## 8. 已知限制

- **无认证**：服务端以 `RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH` 注册，任何能连上 5001 端口的主机都可以读取画面并注入输入。仅适合可信网络中的 POC 使用。
- **对话框按钮会闪烁**：容器无 DWM，`WM_PRINT` 对主题化控件支持不完整。状态栏已通过显式重打后代修复，按钮未解决，详见第 6 节。
- **单会话**：服务端状态（`Rendering.cpp` 的位图缓冲、纹理、事件队列）都是全局单例，同一时刻只能服务一个容器。
- **抓屏有帧率上限**：客户端循环按 `Client.cpp` 的 `FrameIntervalMs` 节流，当前 33 ms（~30fps）。这个值是拿容器 CPU 换流畅度的旋钮。
- **每帧全屏 PNG**：客户端用 GDI+ 把整屏编码成 PNG，服务端再用 `IMG_LoadPNG_RW` 解码。实测编码约 6.9 ms / 1024×768 帧，占 33 ms 帧预算的 21%，**不是主要开销**；换成 WIC 反而慢 1.8 倍。真正的浪费在于 81% 的相邻帧完全相同却仍然整屏重编码重传输，见 [IMAGE-ANALYSIS.md](IMAGE-ANALYSIS.md) 第 2、3 节。
- **NAT 抹平源 IP**：容器流量经 docker NAT 后，服务端看到的所有连接源地址都是宿主机地址，无法用 IP 区分容器。
- **没有音频**：容器里没有任何音频端点（`waveOutGetNumDevs()` 返回 0），也没有可抓的流。要做得在用户态伪造端点，成本是另起一个项目。详见 [AUDIO.md](AUDIO.md)。
