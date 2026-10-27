# 镜像逆向：容器里到底有什么

把 `mcr.microsoft.com/windows/servercore:20H2` 的两层解出来后做了一轮静态分析（IDA + 字符串扫描），记录结论，避免以后重复挖。

分析对象：

| 层 | 内容 |
|---|---|
| `86b1797~` | 基础层（2020-09-11），完整 Windows 树 |
| `5260181~` | 累积更新层（2022-08-05），覆盖上层 |

版本 `10.0.19041.1889`。所有结论以这两层合并后的文件系统为准。

---

## 1. 图形栈：只有 GDI / GDI+ / WIC

这是本次最有价值的一组**否定结论**——它们把好几条"换个方法抓屏"的路直接堵死了。

| 组件 | 状态 | 含义 |
|---|---|---|
| `dwm.exe` / `dwmcore.dll` / `dwmredir.dll` / `dwmghost.dll` | **不存在** | 没有合成器。WinSxS 里只有 DWM 的 ADMX 策略模板，没有实现 |
| `d3d10warp.dll` | **不存在** | 没有 WARP 软件光栅化器 |
| `dcomp.dll` | **不存在** | 没有 DirectComposition |
| `magnification.dll` / `magnify.dll` | **不存在** | 放大镜 API（`MagSetImageScalingCallback`）不可用 |
| `mf.dll` / `mfreadwrite.dll` / `msmpeg2vdec.dll` | **不存在** | 完全没有 Media Foundation，没有任何 H.264 编解码器 |
| `GdiPlus.dll` | 存在 | 当前 POC 用的 |
| `WindowsCodecs.dll` | 存在 | WIC 可用 |
| `DWrite.dll` / `d2d1.dll` | 存在 | 有 Direct2D，但 D2D 也需要适配器 |
| `d3d11.dll` / `d3d12.dll` / `dxgi.dll` | 存在 | **但没意义**：见下 |

### 为什么 `d3d11.dll` 存在却没用

`d3d11.dll` 只是用户态运行时。`D3D11CreateDevice` 需要：

1. 一个 WDDM 适配器 → 容器里没有 GPU，没有显示设备
2. 或者 `D3D_DRIVER_TYPE_WARP` 软件适配器 → 实现在 `d3d10warp.dll` 里，**这个文件不在镜像中**

两条路都断，所以 D3D 在容器里创建不出 device。想走 D3D 抓屏或 D3D 加速渲染的，到此为止。

### 结论

**`WM_PRINT` / `PrintWindow` 仍然是容器里唯一能拿到窗口像素的途径**，没有备选。`printwnd.cpp` 里作者的原注释是对的，不是他没想到别的办法。

---

## 2. 编码器实测：现有选择已经是最优

`Docs/BUILD.md` 的「已知限制」里写着「每帧全屏 PNG …… 是全链路的主要开销」。实测下来这句话**不成立**，而且换成 WIC 反而更慢。

在宿主机上用一张真实的容器抓帧（1024×768）测的，各跑 30 次取平均：

| 编码器 | ms/帧 | 输出大小 |
|---|---|---|
| **GDI+ PNG**（当前实现） | **6.9** | 39,766 B |
| WIC PNG（`WindowsCodecs`） | 12.4 | 36,043 B |
| WIC BMP | 4.5 | 3,145,782 B |
| WIC JPEG q75 | 3.5 | 87,421 B |
| GDI+ `LockBits`（只取像素，不编码） | 1.2 | — |

> 注：用宿主机（10.0.19045）自带的 `gdiplus.dll` / `WindowsCodecs.dll` 测的，容器是 10.0.19041。编码器性能在两个补丁级别之间不会有量级差异，但严格说不是容器内实测。

读出来的三点：

1. **WIC PNG 比 GDI+ PNG 慢 1.8 倍。** 换成 WIC 是负优化，别换。
2. **PNG 编码只占 33 ms 帧预算的 21%。** 它不可能是「主要开销」。
3. JPEG 快一倍但大 2.2 倍而且有损，在局域网场景下不划算。

真正的开销在别处——最可能是 `PrintWindow`：每帧要对每个窗口发一次跨进程同步 `WM_PRINT`，About 对话框这种有十几个子控件的，一帧就是十几次跨进程 `SendMessage`。**这是猜测，没有实测**；要验证得在容器里加计时。

---

## 3. 实测帧变化率：这才是最大的优化空间

把导出的 120 帧逐像素对比：

```
帧数          120
相邻帧对比    119
完全相同      96  (81%)
有差异        23
差异像素      最小 237 / 中位 2433 / 最大 2433
屏幕总像素    786,432
最大脏区占比  0.309%
```

**81% 的相邻帧逐字节相同，最坏情况也只有 0.31% 的像素变了。**

而现在每帧的固定成本是：全屏 `PrintWindow` → 全屏 PNG 编码（6.9 ms）→ 40 KB RPC → 服务端全屏 PNG 解码 → 全屏纹理上传。**其中 99.7% 的工作在重复计算没有变化的内容。**

可做的优化，按收益排序：

| 方案 | 预期收益 | 成本 |
|---|---|---|
| 客户端保留上一帧缓冲，按行 `memcmp` 求脏矩形，只发脏区 | 静止画面下编码/传输/解码降 2～3 个数量级 | 中，要改 IDL 和两端 |
| 脏区小块各自编码成小 PNG（而不是整屏一张） | 编码面积从 786k 降到几千像素 | 小，建立在上一项之上 |
| 服务端只在收到脏区时 `SDL_UpdateTexture` 对应矩形 | 省掉全屏纹理上传 | 小 |
| 无变化时完全跳过整帧（只回 `RpcPopEvent`） | 空闲时容器 CPU 接近 0 | 很小，可先做这个验证方向 |

最后一项是**改动最小、能立刻验证方向**的：在客户端加一个「本帧和上帧一样就只收事件、不发图像」的分支，看容器 CPU 掉多少。如果掉得明显，再往上做脏矩形。

---

## 4. RDS/RDP 服务端栈完整存在，但容器里跑不起来

这一节是整个调研里花时间最多的部分，结论是**否定的**：RDP 在进程隔离的 Windows 容器里起不来，原因是架构性的，不是配置问题。

中途走了很长的弯路，弯路本身也记在下面，因为它比结论更有参考价值。

### 4.1 镜像里有什么

整套远程桌面服务端都在：

```
termsrv.dll          1.2 MB   RDP 服务本体
rdpcorets.dll        1.6 MB   协议核心（UMRDPProtocolManager）
rdpbase.dll          1.5 MB   协议基础
rdpserverbase.dll             服务端基础
rdpudd.dll                     UDP 传输
rdpdr.sys                     设备重定向驱动
rdpvideominiport.sys           RemoteFX 虚拟显示驱动
umrdp.dll                      用户态 RDP 显示驱动
umb.dll                        UMBus 用户态驱动（COM 服务端）
rdsdwmdr.dll                   RDS 的 DWM 驱动
terminpt.sys / hyperkbd.sys    输入
rdpinput.exe / rdpclip.exe / rdpshell.exe / RdpSa.exe / TSTheme.exe
```

### 4.2 容器刻意关掉了 RDP

`mcr.microsoft.com/windows/servercore:20H2` 里：

| 项 | 初始状态 |
|---|---|
| `TermService` 启动类型 | **Disabled** |
| `SessionEnv` | Running / Manual |
| `UmRdpService` | Stopped / Manual |
| `fDenyTSConnections` | 0 |
| `WinStations\RDP-Tcp\fEnableWinStation` | 1 |

### 4.3 第一道封锁与它的后门

`termsrv!CRemoteConnectionManager::Start`：

```c
v11 = CSLQuery::bRemoteConnAllowed;
...
if ( CContainerHelper::IsProcessRunningInContainer() != 0 )
{
    v11 = 0;                      // 容器里强制关掉
    _DbgPrintMessage(2, "TS in container and so Remote Connection is not allow");
}
...
if ( v11 != 0 ) { /* 枚举 WinStations，创建监听器 */ }   // 整段被跳过
```

后门在 `CContainerHelper::GetAllowAllListenerInContainer`：

```c
RegQueryValueExW(hKey, L"TemporaryALiC", ...)   // HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server
```

**非 0 时容器检测直接返回 0**，上面那个 `v11 = 0` 不会执行。名字是 "Temporary **A**llow **Li**stener in **C**ontainer"，微软自己留的测试开关，不需要改二进制。

实测设了 `TemporaryALiC=1` 之后这道封锁确实解除。

### 4.4 第一道失败：监听器起不来

监听器创建要经过 `termsrv` 里一串检查，实测**全部通过**（`RtlAdjustPrivilege(SE_AUDIT_PRIVILEGE)`、`CGraphicsFactory__StartGraphicsServices`、`WinStations` 枚举、`RegQueryListenerStart`、`LoadableProtocol_Object` 读取）。

真正的失败在**监听器创建循环内部**：

```
:Listener RDP-Tcp terminal class (null)
:Creating listener RDP-Tcp
ERR::CoCreateInstance( IWRdsProtocolManager ) failed: 0x80070002 in CProtocolManager::Initialize
ERR::ptrProtocolMgr->Initialize failed: 0x80070002 in CProtocolExMgr::CreateListener
:Protocol failed to create listener RDP-Tcp, error 0x80070002
ERR::No listeners
```

（这是打开 WPP 调试输出后抓到的，见 4.6。）

`0x80070002` = `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)`。顺着 `rdpcorets.dll` 往下：

```
CProtocolManager::Initialize
  CoCreateInstance({5828227c-20cf-4408-b73f-73ab70b8849f} /*CLSID_UMRDPProtocolManager*/)
    -> rdpcorets!CUMRDPProtocolManager::FinalConstruct
         -> CTiBus::Initialize
              CoCreateInstance({9197e04d-2b9f-4849-8bf7-75294eb5c043} /*CLSID_UMBusDriver*/)
                 实测成功（umb.dll 是用户态 COM 服务端）
              CTiBus::SetupDevice(this, this, L"TERMINPUT_BUS", ...)
         -> CTiNotify::CreateInstance    （反编译确认不产生文件操作）
```

`CTiBus::SetupDevice` 里：

```c
inited = (*(*(this + 25) + 48))(*(this + 25), a3 /*L"TERMINPUT_BUS"*/, a4, 0, this + 24);
//        ^^^^^^^^^^^^^^^^^^^
//        this+200 = CTiBus::Initialize 里 CoCreateInstance 出来的 UMBus 对象
```

槽位 48 对应 `CUMBusDriver::AddBusEnumerator`（`umb.dll` vtable 实测确认），它转到：

```c
// CUMBusDriver::CreateBusEnumerator
if ( DeviceIoControl(*(this + 12), 0x228000u, v16, v11, &OutBuffer, 0x14u, &BytesReturned, nullptr) )
{
    *a3 = OutBuffer;
    *a4 = v19;
}
else
{
    LastError = GetLastError();
    v9 = LastError;
    if ( LastError > 0 )
        v9 = (unsigned __int16)LastError | 0x80070000;   // ← 0x80070002 从这里出去
}
```

**`DeviceIoControl(UMBus句柄, IOCTL 0x228000, ...)` 失败，`GetLastError()` 返回 2。**

句柄本身是好的——`CUMBusDriver::GetRootUmBusEnumerator` 通过 `CUMBusDeviceInterface::OpenRootUmBusEnumerator` 打开成功，说明 **UMBus 内核驱动（`umbus.sys`，实测 `STATE: RUNNING`）是通的**。是驱动**拒绝**了这次请求：`0x228000` 是"按名字创建总线枚举器"，名字 `TERMINPUT_BUS` 驱动不认，返回 `STATUS_OBJECT_NAME_NOT_FOUND`。

### 4.5 第二道失败：会话（已定位到具体指令）

打上补丁 1 之后 3389 开始监听，`qwinsta` 里出现 `rdp-tcp 65536 Listen`。客户端能连上，
会话也建出来了（session 33/34，`csrss` 起来了，`CsrConnected` 达成），但客户端停在
**"正在配置远程会话"**，约 20 秒后断开。

`termsrv` 的 WPP 输出把失败点写得很清楚：

```
[termsrv] :Assign session id 33 to terminal Trace ID 0x35
[termsrv] ERR::this->ptrIWRdsProtocolConnection->NotifySessionId failed: 0x80070102 in CConnectionEx::AssignSession
[termsrv] ERR::AssignSession failed: 0x80070102 in CRemoteTerminal::DoConnect
```

**`0x80070102` 是 `CUMRDPConnection::NotifySessionId` 的返回值。** 顺着反编译往下，
它在 `rdpcorets.dll` 里的产生点是**唯一一条**：

```
CUMRDPConnection::NotifySessionId                     0x1800470b0  行 2576 "Failed to create Input devices"
  └─ (*(v4->vtbl[6]))(v4, SessionId, v57, 1)          0x180047523
       v4 = *((_QWORD *)this + 24)  =  CUMTSInputPlugin
     └─ CUMTSInputPlugin::CreateInputDevices          0x18004ff90   调用点 0x180050087 / 0x1800502b8
          └─ CTiDevice::WaitForDeviceReady            0x180051338
               └─ WaitForSingleObject(设备就绪事件, 20000) == 258 (WAIT_TIMEOUT)
                    └─ return 0x80070102
```

`CTiDevice::WaitForDeviceReady` 的原始体：

```c
__int64 __fastcall CTiDevice::WaitForDeviceReady(HANDLE *this)
{
  bool v2; // zf
  __int64 result; // rax

  v2 = WaitForSingleObject(*(this + 13), 0x4E20u) == 258;   // 20000 ms
  result = 2147942658LL;                                     // = 0x80070102
  if ( !v2 )
    return *((unsigned int *)this + 28);
  return result;
}
```

**`2147942658` 就是 `0x80070102`。** 等 20 秒设备就绪，超时（`WaitForSingleObject` 返回 258）
就把这个码原样返回。

设备就绪事件由 `CTiBus` 提供，而 `CTiBus::Initialize` 的调用者只有两处：
`CUMRDPProtocolManager::FinalConstruct`（**正是补丁 1 打的那个位置**）和 `CTermInputMgr::Initialize`。

**所以补丁 1 只是把失败从"监听器起不来"推迟到了"会话建不出来"，根因是同一个：
`CTiBus` 从来没被真正初始化过。**

#### 打补丁实验矩阵

| 补丁 | 内容 | 结果 |
|---|---|---|
| 1 | `FinalConstruct` 忽略 `CTiBus::Initialize` 失败（2 字节） | **监听器起来了**，3389 Listen |
| 2 | `CRdpPipeGfxPlugin::IsReady` 无条件返回 0 | 无效 |
| 3 | `CRdpPipeGfxPlugin::SendCommandChannelName` 无条件返回 0 | 无效 |
| 4 | `NotifySessionId` 整个跳过（入口 `xor eax,eax; ret`） | **客户端连上了**（黑屏，随后断开） |
| 6 | `NotifySessionId` 跑完但强制返回 0 | 仍卡住 |
| 7 | `NotifySessionId` 做输入初始化、跳过整个图形初始化块 | 仍卡住 |

补丁 4 和 6/7 的差异是整条线索的关键：**返回值不是问题，函数体里的副作用才是。**
补丁 7 保留了输入块、砍掉了图形块，仍然失败 —— **说明卡点在输入块，不在图形块。**
这和上面反编译出来的调用链完全一致。

补丁 2 / 3 为什么无效，现在也能解释了：

- **补丁 2**：`CRdpPipeGfxPlugin::IsReady` 超时返回的是 `E_FAIL`（`0x80004005`），不是
  `WAIT_TIMEOUT`。代码和实测错误码对不上，本来就不该是它。
- **补丁 3**：打的是 `CRdpPipeGfxPlugin::SendCommandChannelName`，但 `CRdpPipeGfxPlugin::SetSessionId`
  里 provider 的 `SetSessionId` **排在前面**，provider 一失败就直接返回，根本走不到这个函数。
  何况整个图形块都在输入块**之后**——输入块已经先失败了。

另外，`CUMRDPConnection+0x2F0`（`field 188`）非零时会跳过整个输入块、并把图形插件换成
`PlayBackGfxPlugin`。但它在 `CUMRDPConnection` 的方法里**只读不写**（大概是从
`_WRDS_CONNECTION_SETTINGS` 整块拷进来的），没有配置途径，这条捷径走不通。

### 4.6 和 16299.248 的对比：到底什么变了

从 KB4074588 的 MSU 里解出 1709 的 `rdpcorets.dll`（10.0.16299.248，2035712 字节），
和本镜像的 10.0.19041.1（1639936 字节）对扫运行期字符串：

| 标记 | 16299.248 | 19041.1 |
|---|---|---|
| `TERMINPUT_BUS` | 1 | 1 |
| `CUMTSInputPlugin` | 1 | 1 |
| `CTiDevice` / `CTiBus` / `CTermInputMgr` | 3 / 1 / 1 | 3 / 1 / 1 |
| `WaitForDeviceReady` / `CreateInputDevices` | 1 / 1 | 1 / 1 |
| `RDP::CommandChannelName` / `RdpCommandChannel` | 1 / 1 | 1 / 1 |
| **`SetRdpIddHandle`** | **0** | **3** |
| **`GraphicsPipeline`** | **0** | **25** |

**UMBus 输入栈两版都在**（`TERMINPUT_BUS` 是运行期宽字符串，不是符号表残留）。
20H2 相对 1709 新增的是 **IDD 图形管线**（`SetRdpIddHandle`、`RDV::RDP::GraphicsPipeline*`）。

也就是说，**"RDP 输入栈依赖 UMBus"不是 20H2 才有的**——两版一样。1709 能用，
要么是那时 `CTiBus::Initialize` 在容器里能成功，要么是它失败了但没走到
`WaitForDeviceReady`。**这一步还没有实验证据**，需要一份 1709 的镜像才能下结论。

### 4.7 根因（实测）：UMBus 设备接口的符号链接不在容器的对象命名空间里

**一句话：进程隔离容器的对象命名空间里没有 UMBus 设备接口的符号链接，
所以 `CreateFileW` 返回 `ERROR_FILE_NOT_FOUND`，也就是 `0x80070002`。**

实测（`Tools/Probe-UMBus.exe`，纯 Win32，宿主和容器各跑一遍）：

| | 宿主机 | 容器 |
|---|---|---|
| `[1]` `SetupDiGetClassDevs` | OK | **OK** |
| `[1]` `CreateFileW` `\\?\root#umbus#0000#{65a9a6cf-…}` | **OK** | **FAILED: 2** |
| `[1]` `CreateFileW` `\\?\umb#umb#1&841921d&0&terminput_bus#{…}` | **OK** | **FAILED: 2** |
| `[2]` `QueryDosDeviceW("root#umbus#0000#{…}")` | **→ `\Device\00000008`** | **NOT IN THIS NAMESPACE: 2** |
| `[2]` `QueryDosDeviceW("umb#umb#1&841921d&0&terminput_bus#{…}")` | **→ `\Device\00000077`** | **NOT IN THIS NAMESPACE: 2** |
| `[3]` `CreateFileW` `\\?\GLOBALROOT\Device\00000008` | **OK** | — |
| `[4]` 扫 `\Device\00000000`–`\Device\000000FF`（0 访问权限打开） | **32 of 256 打开** | **0 of 256 打开** |
| `[5]` **对照**：`\\?\GLOBALROOT\Device\KsecDD` / `\Device\Null` | OK | **OK** |

**`[5]` 是对照组**，它排除了"`\\?\GLOBALROOT` 这种路径形式在容器里不解析"这个假阴性：
容器里 `KsecDD` 和 `Null` 都开得到，所以 `[4]` 的 `0 of 256` 是真的——**容器里一个匿名
PnP 设备对象都没有**。

**`SetupDiGetClassDevs` 在容器里是成功的**——PnP 设备数据库（`Enum`）容器看得见，
`ROOT\UMBUS\0000` 状态 Started、`umbus` 驱动 RUNNING。**但 UMBus 的设备对象是 PnP
枚举出来的匿名对象（`\Device\000000NN`），它们不在容器的对象命名空间里，接口符号链接
（`\??\root#umbus#0000#{…}`）也不在。**


**和镜像、和 DLL 版本都无关。** 交换实验证明过：把宿主机上那份逐字节相同、
RDP 正常工作（`rdpcorets.dll` 1611776 / `802AD462…`，`umb.dll` 78848 / `E28A994E…`）
放进容器层覆盖 CU 层那两份，容器里 `CTiBus::Initialize` 依然失败、3389 依然不监听。
同一台机器、`isolation=process`、同一个内核、`umbus.sys` 同一份。

完整链条：

```
CreateFileW(UMBus 设备接口)  ->  ERROR_FILE_NOT_FOUND (2)
  -> OpenRootUmBusEnumerator 失败 -> 0x80070002
     -> AddBusEnumerator 失败 -> CTiBus::SetupDevice 失败
        -> CTiBus::Initialize 失败 -> FinalConstruct 失败
           -> CoCreateInstance(IWRdsProtocolManager) = 0x80070002
              -> 监听器起不来

会话侧（补丁 1 之后）：
  CreateInputDevices -> CTiDevice::WaitForDeviceReady
     -> WaitForSingleObject(就绪事件, 20000) == 258 -> 0x80070102
        -> NotifySessionId 失败 -> AssignSession 失败
           -> 客户端卡在"正在配置远程会话"约 20 秒后断开
```

**这是容器的对象命名空间行为，镜像侧、注册表侧、DLL 侧都没有办法绕过。**


一个仍然没解的问题：作者当年（Server 1709）是能用的，而他的主机没变。既然这条限制
在宿主机内核/对象命名空间这一层，那"镜像更新导致它坏掉"就解释不通——**除非 1709 的
`rdpcorets.dll` 在 `FinalConstruct` 里根本不调 `CTiBus::Initialize`**。这一点查过：
1709 RTM 确实不直接调（只经 `CTermInputMgr::Initialize`），1709+KB4074588 之后才加了
直接调用。**但 1709 走到会话建立那一步时 `CreateInputDevices` 同样会撞上这堵墙**，
所以这条线索还差一份 1709 的宿主机才能收尾。



### 4.8 弯路：为什么前面十几轮找错了地方

**把 `RemoteConnectionManager/Admin` 通道为空误读成了"RCM 的代码没跑"。**

`_DbgPrintMessage` 的输出默认三条出口全关：

```c
void _DbgPrintMessage(int a1, const char *a2, ...)
{
  if ( CTraceBase::g_bEnabled != 0 && CTraceBase::g_TLSIndex != -1 ) {
    Value = TlsGetValue(g_TLSIndex);
    if ( Value != nullptr ) {
      if ( ((*(...)(Value) & g_DebugTraceComponent) == 0 && a1 != 128 )
        return;                                     // 组件过滤器，来自 DebugFlags
      ...
    }
  }
  ...
  switch ( a1 ) {
    case 8:  if ( (unsigned int)dword_180126060 > 2 )   // provider Level，只有 ETW 会话启用才非 0
               _tlgWriteTemplate(&dword_180126060, ...);
             break;
    ...
  }
  // 内存环形缓冲，无条件写：termsrv.dll RVA 0x127760，128 条 * 80 字节
  // 但全文件没有任何读取者（g_DbgTracePos 只有这一次写入的交叉引用）
  ...
  // OutputDebugStringA —— g_bDebugSpew 在 CUtils::Initialize 里被硬编码为 0，此路不通
}
```

**所以那个通道为空只说明"没人在听"，不说明"没人在说"。** RCM 一直在跑，只是输出被丢掉了。

打开输出的办法：注册表 `HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server` 下的
`Debug` / `DebugFlags` / `DebugLevel`（由 `CWPPLoggerGuidConfig::ReadTraceConfig` 读取），
配合 ETW 会话抓 WPP。`DebugFlags` 对应 `g_DebugTraceComponent`（默认 `-2`，会滤掉大部分消息）；
`DebugLevel` 默认 `16574 = 0x40BE`。

**WPP provider GUID 的编码**：符号名 `WPP_<32 hex>_Traceguids` 里的 hex **就是 GUID 的字符串形式**，按 `8-4-4-4-12` 插横线即可。不要再用 `bytes_le` 转一次——那是双重翻转。

### 4.9 一句话

**RDP 在 Windows 容器里跑不起来。** 第一道封锁（`TemporaryALiC`）是微软自己留的开关，能拆；
第二道是 **UMBus 设备接口的符号链接不在容器的对象命名空间里**——`QueryDosDeviceW` 在容器里
查不到 `root#umbus#0000#{65a9a6cf-…}`（宿主机上它指向 `\Device\00000008`），所以
`CreateFileW` 返回 `ERROR_FILE_NOT_FOUND`。这一道拆不掉，它不在镜像里，在容器的对象命名空间里。

打补丁能把这两道门都撬开一条缝（补丁 1 让监听器起来，补丁 4 能让客户端连上），
但那是**把输入栈整个跳过去**换来的——连上也是黑屏、无输入、随即断开。
`CTiDevice::WaitForDeviceReady` 那 20 秒超时是硬性的：设备就绪事件由 `CTiBus` 提供，
而 `CTiBus` 依赖那个容器打不开的 UMBus 设备。

**验证方式**：`Tools/Probe-UMBus.exe`，宿主和容器各跑一遍，看 `CreateFileW` 那两行。

`Tools/` 下的脚本保留下来，以后再遇到类似的"没有错误但不工作"的情况可以直接复用：

| 脚本 | 用途 |
|---|---|
| `Test-RdpInContainer.ps1` | RDP 服务端各道门的开关与状态 |
| `Diagnose-RdpListener.ps1` | 服务面 + 事件日志 + LSM 宿主定位 |
| `Diagnose-ProtocolManager.ps1` | 任意 CLSID 的注册/加载/激活诊断 |
| `Capture-TermSrvTrace.ps1` | 抓 termsrv/lsm 的 WPP 输出 |
| `Capture-RdpSessionTrace.ps1` | 连会话时抓 WPP + 进程采样 + 补丁自检 |
| `Trace-RdpCoreTS.ps1` | 抓 rdpcorets 的 WPP 输出（GUID 编码待修） |
| `Check-UMBusDriver.ps1` | UMBus / WUDF 驱动状态 |
| `Patch-RdpCoreTS-OnHost.ps1` | 宿主机侧给容器层的 `rdpcorets.dll` 打补丁（含 `-Revert`） |

## 5. 音频：RDP 这条路也不通

`Docs/AUDIO.md` 第 5 节写着「RDP 会话自带音频重定向（`rdpsnd`），如果在容器里跑 RDP 会话，声音是白送的」。**这句话对本镜像不成立。**

扫遍两层，`rdpsnd.dll`、`rdpwd.sys`、`tsaudio` 全都不存在；`System32` 下**没有任何名字里带 audio/snd/mixer/wave 的文件**（`MMDevAPI.dll` 在，但 `audioses.dll` 不在，和前一轮结论一致）。

而且第 4 节的 RDP 路线实测**走不通**，所以连"用 RDP 换音频"这个念头都不成立。AUDIO.md 里那条已经改掉了。

---

## 6. 其他零碎

- **`wmsgapi.dll`**：导出 `WmsgBroadcastMessage` / `WmsgPostMessage` / `WmsgSendMessage` / `WmsgSendPSPMessage`。是容器宿主用来跨容器边界发窗口消息的（WSL/容器集成用）。容器**内部**用不上。
- **`computecore.dll` / `computenetwork.dll` / `computestorage.dll`**：HCS 客户端，容器里跑的工具用得到，POC 用不上。
- **`win32kns.sys`**（30 KB）在 `System32` 下，和 `win32k.sys` / `win32kbase.sys` / `win32kfull.sys` 并列。`win32kfull.sys` 里有 `IsHvsiContainer`、`GetSynthesizedContainerMouseInputEvent` 等容器模式符号，但没有找到任何「容器虚拟显示器尺寸」相关的可配置项。
- **桌面尺寸固定 1024×768**。`Client.cpp` 里注释掉的 `ChangeDisplaySettings` 是唯一能改的入口；容器无显示设备，这条路大概率不通，没有进一步验证。
- **窗口站**：`Init.cpp` 里 `CreateWindowStation("WinSta0")` + `CreateDesktop("Default")` + `SetUserObjectInformation(WSF_VISIBLE)` + `SwitchDesktop`。这套在容器里能work，是 POC 能跑起来的前提。

---

## 7. 一句话总结

| 问题 | 结论 |
|---|---|
| 有没有比 `WM_PRINT` 更好的抓屏办法？ | **没有**。无 DWM、无 WARP、无放大镜，GDI 是唯一途径 |
| 换 WIC 编码会不会更快？ | **不会，慢 1.8 倍**。现有 GDI+ PNG 已是内置最优 |
| PNG 是性能瓶颈吗？ | **不是**，占 33 ms 预算的 21%。瓶颈更可能在 `PrintWindow`，未实测 |
| 最大的优化空间在哪？ | **脏矩形**。81% 的相邻帧完全相同，最坏只变 0.31% 的像素 |
| 有没有可能整个换掉这个 POC？ | **不行**。RDP 服务端栈虽然完整、`TemporaryALiC` 也能拆掉第一道封锁，但协议栈硬依赖 UMBus + PnP 设备枚举，进程隔离容器提供不了。见第 4 节 |
| 音频呢？ | 彻底没有，连 RDP 重定向都没有 |
| 这个调研最大的教训？ | **"事件日志为空" ≠ "代码没跑"**。诊断输出默认全关时，一切看起来都正常。第 4.6 节记了这条弯路 |
