# 在 Windows 进程隔离容器里运行 RDP 服务端：根因定位

> 结论先说：**进程隔离容器的对象命名空间里没有 PnP 枚举出来的匿名设备对象**，
> 而 RDP 服务端的输入栈要打开 UMBus 的设备接口。打不开 → `ERROR_FILE_NOT_FOUND`。
> 这是容器边界，镜像、DLL、注册表都改不了。

本文记录一条完整的定位路径，每一步都附上排除掉的假设和排除它的实验。
如果你在排查"容器里某个设备/驱动相关的东西不工作"，这套方法可以直接复用。

---

## 1. 现象

在 `mcr.microsoft.com/windows/servercore:20H2` 容器里（`--isolation process`）启动
TermService 之后：

- 3389 完全不监听，事件日志里没有任何错误
- 加上一组二进制补丁之后 3389 能监听，但客户端连上去**卡在"正在配置远程会话"**
  约 20 秒后断开，服务端 `qwinsta` 里会话一闪而过

## 2. 环境

| | |
|---|---|
| 宿主 | Windows Server, version 20H2（Build 19042） |
| 容器 | `mcr.microsoft.com/windows/servercore:20H2`，`isolation=process` |
| 镜像层 | base 层 2020-09-11 + CU 层 2022-08-05 |
| 客户端 | Windows 10 上的 `mstsc` |

**关键前提**：进程隔离容器共享宿主内核，内核驱动来自宿主。

---

## 3. 第一道封锁：容器检测

`termsrv.dll` 里创建监听器的路径上有一个容器检测：检测到容器就直接放弃，
返回 `ERROR_FILE_NOT_FOUND`，**日志里什么都不写**。

反编译出来是：

```c
RegQueryValueExW(hKey, L"TemporaryALiC", ...);   // HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server
```

**非 0 时容器检测直接返回 0**，封锁解除。名字是 "Temporary **A**llow **Li**stener in **C**ontainer"，
微软自己留的测试开关。

```cmd
reg add "HKLM\System\CurrentControlSet\Control\Terminal Server" /v TemporaryALiC /t REG_DWORD /d 1 /f
```

设了之后这道封锁确实解除（这是 2018 年 Rafael Rivera 那篇文章里的机制，**至今仍然有效**）。

---

## 4. 第二道：监听器仍然起不来

解除第一道封锁后，抓 `termsrv` 的 WPP 输出能看到：

```
:Listener RDP-Tcp terminal class (null)
:Creating listener RDP-Tcp
ERR::CoCreateInstance( IWRdsProtocolManager ) failed: 0x80070002 in CProtocolManager::Initialize
ERR::ptrProtocolMgr->Initialize failed: 0x80070002 in CProtocolExMgr::CreateListener
:Protocol failed to create listener RDP-Tcp, error 0x80070002
ERR::No listeners
```

### 怎么打开这些输出

`_DbgPrintMessage` 默认三条出口全关（ETW provider Level 为 0、`g_bDebugSpew` 硬编码为 0、
内存环形缓冲没有读取者）。要看到它必须主动开 ETW 会话：

```powershell
# DebugFlags/DebugLevel 由 CWPPLoggerGuidConfig::ReadTraceConfig 读取
New-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server' -Name Debug      -PropertyType DWord -Value 1          -Force
New-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server' -Name DebugFlags -PropertyType DWord -Value 0xFFFFFFFF -Force
New-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server' -Name DebugLevel -PropertyType DWord -Value 0xFFFFFFFF -Force

# 注意 level 是 UCHAR(0-255)，写 0xFFFFFFFF 会报 "Out of present range."
logman start WcvS1 -p '{5283D5F6-65B5-425F-A30B-F16C057D6B57}' 0xFFFFFFFF 255 -o C:\s1.etl -ets
```

**WPP provider GUID 的编码**：符号名 `WPP_<32hex>_Traceguids` 里的 hex **就是 GUID 的字符串形式**，
按 `8-4-4-4-12` 插横线即可。**不要再用 `bytes_le` 转一次**——那是双重翻转，
会让你找错 provider（我们为此浪费了一轮：抓到 0 事件，因为选的 GUID 属于
`CUMRDPListenerVMBus`，和卡点无关）。

### 顺着 0x80070002 往下

`0x80070002` = `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)`，它从
`CUMRDPProtocolManager::FinalConstruct` 原样透传出来：

```
termsrv  CProtocolManager::Initialize
  └─ CoCreateInstance({5828227c-20cf-4408-b73f-73ab70b8849f} /*IWRdsProtocolManager*/)
       └─ rdpcorets!CUMRDPProtocolManager::FinalConstruct
            └─ CTiBus::Initialize(L"TERMINPUT_BUS")
                 └─ CTiBus::SetupDevice(this, this, L"TERMINPUT_BUS", ...)
                      └─ umb.dll!CUMBusDriver::AddBusEnumerator
                           ├─ CUMBusDeviceInterface::OpenRootUmBusEnumerator   ← 就是这里失败
                           └─ CUMBusDriver::CreateBusEnumerator
                                └─ DeviceIoControl(hUMBus, 0x228000, <DWORD 1><L"TERMINPUT_BUS">, ...)
```

`CTiBus::Initialize` 有四个失败点，只有 `OpenRootUmBusEnumerator` / `CreateBusEnumerator`
这条能产出 `0x80070002`（`CreateBusEnumerator` 里失败路径是
`GetLastError() | 0x80070000`，`GetLastError()==2` 正好得到这个值）。

### 临时绕过（仅为继续分析，不是修复）

在 `FinalConstruct` 里让 `CTiBus::Initialize` 的失败被忽略，监听器就能起来：

```
rdpcorets.dll 文件偏移 0x16617   8b f0  (mov esi,eax)   ->  31 f6  (xor esi,esi)
rdpcorets.dll 文件偏移 0x16619   85 c0  (test eax,eax)  ->  31 c0  (xor eax,eax)
```

打完之后 3389 开始监听、`qwinsta` 出现 `rdp-tcp 65536 Listen`、会话也能创建
（`csrss` 起来、`CsrConnected` 达成）。

---

## 5. 第三道：会话卡在"正在配置远程会话"

客户端连上后卡约 20 秒然后断开。`termsrv` 的 WPP 输出：

```
[termsrv] :Assign session id 33 to terminal Trace ID 0x35
[termsrv] ERR::this->ptrIWRdsProtocolConnection->NotifySessionId failed: 0x80070102 in CConnectionEx::AssignSession
[termsrv] ERR::AssignSession failed: 0x80070102 in CRemoteTerminal::DoConnect
```

`0x80070102` = `HRESULT_FROM_WIN32(258 /*WAIT_TIMEOUT*/)`。它在 `rdpcorets.dll` 里
**只有一个产出点**，而且是个字面量：

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

调用链：

```
CUMRDPConnection::NotifySessionId
  └─ CUMTSInputPlugin::CreateInputDevices
       └─ CTiDevice::WaitForDeviceReady
```

那个"设备就绪事件"由 `CTiDevice::Initialize` 里 `CreateEventW` 创建，
**只在 `CTiBus::SetupDevice` 的成功路径上**由 `CTiDevice::SignalReady` 置位。

**所以两道门是同一个根因**：`CTiBus` 建不起来，导致
①监听器创建阶段 `FinalConstruct` 失败；②会话建立阶段设备就绪事件永远不置位。

---

## 6. 排除"二进制版本差异"

一开始的假设是"微软更新镜像换了 `rdpcorets.dll`"。**这个假设是错的，但排除它的实验很值得做。**

关键发现：**宿主机的 `rdpcorets.dll` 和 `umb.dll` 与镜像 base 层那两份逐字节相同**：

| 文件 | 宿主机 | 20H2 base 层 | 20H2 CU 层 |
|---|---|---|---|
| `rdpcorets.dll` | 1611776 · `802AD462…` | **1611776 · `802AD462…`** | 1639936 · `5C14BFA2…` |
| `umb.dll` | 78848 · `E28A994E…` | **78848 · `E28A994E…`** | 78848 · `A0EE4FA8…` |

也就是说这台 VM 宿主是 Server 20H2，服务级别和镜像 base 层一致，**宿主的 RDP 是好的**。

于是做**交换实验**：把宿主机那两份（能跑的）覆盖进容器层，覆盖掉 CU 层那两份。

```powershell
# 见 Tools/Swap-UmbStack-ToHost.ps1
docker stop <容器>
.\Swap-UmbStack-ToHost.ps1 -Apply      # 需要先 takeown + icacls，windowsfilter 层是 ACL 问题
docker start <容器>
```

结果：

```
rdpcorets.dll  1611776   ← 和宿主机逐字节相同，无补丁
umb.dll          78848   ← 和宿主机逐字节相同
TermService    Running
3389           没监听
```

**同一份二进制，宿主好、容器坏。DLL 版本差异被彻底排除。**

> 顺带一个结论：静态比对本身没错（`SetupDevice` 在 base 和 CU 之间逐字节相同，
> `CTiBus::Initialize` 调用序列相同，`AddBusEnumerator` 结构相同），
> 但**方向是错的**——变量根本不在 DLL 里。

---

## 7. 根因：容器对象命名空间

用一个**纯 Win32** 的探针（`Tools/Probe-UMBus.c`，不调 `umb.dll`、不用虚表）在宿主和容器各跑一遍：

| 探针 | 宿主机 | 容器 |
|---|---|---|
| `[1]` `SetupDiGetClassDevs` | OK | **OK** |
| `[1]` `CreateFileW` `\\?\root#umbus#0000#{65a9a6cf-…}` | **OK** | **FAILED: 2** |
| `[1]` `CreateFileW` `\\?\umb#umb#1&841921d&0&terminput_bus#{…}` | **OK** | **FAILED: 2** |
| `[2]` `QueryDosDeviceW("root#umbus#0000#{…}")` | **→ `\Device\00000008`** | **NOT IN THIS NAMESPACE: 2** |
| `[2]` `QueryDosDeviceW("umb#umb#1&841921d&0&terminput_bus#{…}")` | **→ `\Device\00000077`** | **NOT IN THIS NAMESPACE: 2** |
| `[3]` `CreateFileW` `\\?\GLOBALROOT\Device\00000008` | **OK** | — |
| `[4]` 扫 `\Device\00000000`–`\Device\000000FF`（0 访问权限打开） | **32 of 256 打开** | **0 of 256 打开** |
| `[5]` **对照**：`\\?\GLOBALROOT\Device\KsecDD` / `\Device\Null` | OK | **OK** |

### `[5]` 是对照组，很重要

它排除了"`\\?\GLOBALROOT` 这种路径形式在容器里不解析"这个假阴性。
容器里 `KsecDD` 和 `Null` 都开得到，**所以 `[4]` 的 `0 of 256` 是真的**。

### 怎么读这张表

- **`SetupDiGetClassDevs` 在容器里成功** —— PnP 设备数据库（`Enum`）容器看得见，
  `ROOT\UMBUS\0000` 状态 Started、`umbus` 驱动 RUNNING
- **但匿名设备对象 `\Device\000000NN` 一个都开不到** —— 容器里连它们都不存在
- **接口符号链接 `\??\root#umbus#0000#{…}` 也不在容器的命名空间里**

**UMBus 的设备对象是 PnP 枚举出来的匿名对象，它们不在容器的对象命名空间里。**

于是 `CreateFileW` 返回 `ERROR_FILE_NOT_FOUND`——就是那个 `0x80070002`。

---

## 8. 完整失败链

```
容器对象命名空间里没有 \Device\000000NN
  └─ CreateFileW(UMBus 接口符号链接) → ERROR_FILE_NOT_FOUND (2) = 0x80070002
       └─ OpenRootUmBusEnumerator 失败 → AddBusEnumerator 失败
            └─ CTiBus::SetupDevice 失败 → CTiBus::Initialize 失败
                 └─ CUMRDPProtocolManager::FinalConstruct 失败
                      └─ CoCreateInstance(IWRdsProtocolManager) = 0x80070002
                           └─ 监听器起不来
                                └─（打补丁绕过之后）会话建立时再撞同一堵墙：
                                     CUMTSInputPlugin::CreateInputDevices
                                       └─ CTiDevice::WaitForDeviceReady
                                            └─ WaitForSingleObject(就绪事件, 20000) == 258
                                                 └─ 0x80070102
                                                      └─ NotifySessionId 失败
                                                           └─ AssignSession 失败
                                                                └─ 客户端卡在
                                                                   "正在配置远程会话"
                                                                   约 20 秒后断开
```

---

## 9. 什么能改、什么不能改

| | |
|---|---|
| `TemporaryALiC` 注册表开关 | ✅ 能拆（微软自己留的） |
| 二进制补丁让监听器起来 | ✅ 能（但只是把输入栈整个跳过去） |
| 换镜像 / 换 DLL / 改注册表 | ❌ **没用**——限制在容器对象命名空间 |
| 让容器进程看到 UMBus 设备对象 | ❌ 没有已知办法 |

**绕过去的代价**：补丁 4（把 `NotifySessionId` 整个跳过）能让客户端连上，
但那是**把输入栈整个跳过去**换来的——黑屏、无输入、随即断开。

---

## 10. 一个仍未解的矛盾

2018 年那篇文章的环境（Windows Server 1709）是**能用的**，而且作者确认主机没变过。

既然这条限制在宿主内核/对象命名空间这一层，那"镜像更新导致它坏掉"就解释不通。
能想到的唯一自洽解释是：**1709 的 `rdpcorets.dll` 在 `FinalConstruct` 里不直接调
`CTiBus::Initialize`**——这一点查过了：

| | 1709 RTM | 1709+KB4074588 / 20H2 |
|---|---|---|
| `FinalConstruct` 直接调 `CTiBus::Initialize` | **不调**（只经 `CTermInputMgr::Initialize`） | **直接调** |

但 `CTermInputMgr::Initialize` 那条路同样硬失败、同样透传，**1709 走到会话建立那一步时
`CreateInputDevices` 也会撞同一堵墙**。

**要收尾需要一台 1709 的宿主机**（作者的已经没有了）。没有的话这条只能挂着。

---

## 附录 A：关键函数与偏移

均在 `rdpcorets.dll` 10.0.19041.x（`servercore:20H2` 镜像）上。

| 函数 | 说明 |
|---|---|
| `CUMRDPProtocolManager::FinalConstruct` | 建监听器时调 `CTiBus::Initialize` |
| `CUMRDPConnection::NotifySessionId` | 会话建立；输入块 + 图形块 |
| `CUMTSInputPlugin::CreateInputDevices` | 建输入设备 |
| `CTiDevice::WaitForDeviceReady` | 20 秒等待，超时返回 `0x80070102` |
| `CTiDevice::Initialize` | `CreateEventW` 创建那个就绪事件 |
| `CTiDevice::SignalReady` | 只在 `SetupDevice` 成功路径上置位 |
| `CTiBus::Initialize` / `SetupDevice` | 四个失败点；`SetupDevice` 调 `AddBusEnumerator` |
| `umb.dll!CUMBusDriver::AddBusEnumerator` | 槽位 6（虚表偏移 48） |
| `umb.dll!CUMBusDriver::CreateBusEnumerator` | `DeviceIoControl(hUMBus, 0x228000, <DWORD 1><名字>, ...)` |

### 实验用补丁（**不是修复**，仅供继续分析）

| 偏移 | 原始 | 替换 | 作用 |
|---|---|---|---|
| `0x16617` | `8b f0` | `31 f6` | `FinalConstruct` 忽略 `CTiBus::Initialize` 失败 |
| `0x16619` | `85 c0` | `31 c0` | 同上 |
| `0x464b0` | `48 89 5c` | `31 c0 c3` | `NotifySessionId` 整个跳过（最粗） |
| `0x4711d` | `8b c7` | `31 c0` | `NotifySessionId` 跑完但返回 0 |
| `0x46a53` | `0f 85 49 05 00 00` | `e9 4a 05 00 00 90` | 做输入初始化、跳过图形块 |

`file_offset = VA - 0x180000C00`（该版本镜像）。

### 有用但容易踩坑的 WPP provider

| GUID | 组件 |
|---|---|
| `{A3BDED7C-8BEE-3E5A-6499-D41A36E139A4}` | `FinalConstruct` / `CTermInputMgr::Initialize` |
| `{01F19864-60E9-3C8F-C234-B92505F16052}` | `CTiBus::Initialize` / `CPNPPropertyStore` |
| `{36ED36ED-D378-351D-064C-923CC188FD7C}` | `CTiBus::Initialize` / `CTiDevice` |
| `{FD3EE561-44F4-339B-A20A-AFC708B1F8D0}` | `CUMTSInputPlugin::Add*Device` |
| `{68A65793-594D-393D-5222-0CCF4D4C2E51}` | `CUMRDPListenerVMBus` —— **和本文卡点无关** |

## 附录 B：工具

| 文件 | 用途 |
|---|---|
| `Probe-UMBus.c` | **定案探针**。纯 Win32，宿主/容器各跑一遍对比。`/MT` 静态链接 |
| `Probe-TiBus-Pnp.ps1` | 容器内 PnP 状态查询（`Get-PnpDevice` + 注册表 + `pnputil` 退路） |
| `Swap-UmbStack-ToHost.ps1` | 交换实验：把宿主机文件覆盖进容器层，含 `-Revert` |
| `Capture-TermSrvTrace.ps1` | 抓 termsrv / lsm / rdpcorets 的 WPP 输出 |
| `Patch-RdpCoreTS-OnHost.ps1` | 宿主机侧给容器层打补丁，含 `-Revert` |

## 附录 C：踩过的坑

1. **WPP provider GUID 选错** —— 抓到 0 事件，浪费一轮。见 §4 的编码说明。
2. **`logman` 的 level 是 UCHAR** —— 写 `0xFFFFFFFF` 报 "Out of present range."，要写 `255`。
3. **`CM_PROB_PHANTOM` 不是故障** —— `TERMINPUT_BUS` 枚举器是**会话建立时**才创建的，
   连接 RDP 之前它一直是 phantom，连接之后就变成 OK。把它当故障会带偏方向。
4. **从外部进程调 `umb.dll` 的 COM 对象容易崩** —— 虚表槽位和调用约定的假设很容易错，
   而且崩了没有信息。**能用 Win32 系统调用验证的，就不要去调内部函数。**
5. **`windowsfilter` 层写文件是 ACL 问题，不是只读属性** —— 需要先 `takeown` + `icacls`。
6. **进程隔离容器共享宿主内核** —— 所以"容器里的驱动状态"和"宿主机的驱动状态"可能不是一回事，
   而"镜像里的 `.sys` 文件"根本不会被加载。

---

*本文记录的是在一台自有虚拟机上对公开镜像和公开系统组件做的分析。*
