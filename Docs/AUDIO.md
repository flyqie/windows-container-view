# 音频透传可行性调研

结论先说：**容器里没有音频端点，没有可抓的流。** 唯一可行的路线是**在用户态伪造一个音频端点**，成本是另起一个项目，不是给现有 POC 加模块。

本文记录实测数据和判断依据，避免以后重复调研。

---

## 1. 实测结果

在 `mcr.microsoft.com/windows/servercore:20H2` 容器里：

```
waveOutGetNumDevs()                        → 0
Get-CimInstance Win32_SoundDevice          → 空
Get-Service Audiosrv, AudioEndpointBuilder → 空（服务不存在）

C:\Windows\System32\MMDevAPI.dll           → 存在
C:\Windows\System32\audioses.dll           → 不存在
C:\Windows\System32\winmm.dll              → 存在
C:\Windows\System32\dsound.dll             → 不存在
C:\Windows\System32\XAudio2_9.dll          → 不存在

HKLM\SOFTWARE\Classes\CLSID\{BCDE0395-E52F-467C-8E3D-C4579291692E}  → 已注册
    \InprocServer32                        → 存在
```

`{BCDE0395-E52F-467C-8E3D-C4579291692E}` 是 `MMDeviceEnumerator` 的 CLSID。

---

## 2. 为什么补不上

音频和画面走的是完全不同的机制：

| | 画面 | 声音 |
|---|---|---|
| 依赖 | Win32k 的虚拟桌面 | 内核音频驱动提供的端点 |
| 容器里 | Server Core 自带 Win32k，POC 自建 WinSta0 即可用 | 独立的设备命名空间，**没有硬件就没有端点** |

补不上的具体原因：

- **进程隔离容器共享宿主内核**，装驱动等于在宿主上装驱动，容器平台会拦
- **Hyper-V 隔离容器的内核在 Docker 管理的工具 VM 里**，进不去
- Windows 没有"空音频设备"这种东西，`waveOutGetNumDevs()` 在无硬件机器上就是 0
- 容器镜像的组件集构建时固定，`Install-WindowsFeature` 加不了音频组件（加了也还是没有设备）

所以容器里的程序调用音频 API 会直接失败：`GetDefaultAudioEndpoint` 返回 `E_NOTFOUND`，`waveOutOpen` 返回 `MMSYSERR_NODRIVER`。**没有流可以抓。**

---

## 3. 唯一可行的路线：用户态伪造端点

不去抓设备输出，而是**让程序以为有设备**，把它写进音频缓冲的 PCM 截下来，走 RPC 送到宿主机播放。

### 需要实现的接口

`audioses.dll` 不存在意味着**不能只 hook 枚举层**——即使枚举成功，`IMMDevice::Activate` 也拿不到 `IAudioClient`（实现在 AudioSes 服务里，服务不存在）。所以必须自己把整条链实现出来：

```
IMMDeviceEnumerator   GetDefaultAudioEndpoint, EnumAudioEndpoints, GetDevice,
                      Register/UnregisterEndpointNotificationCallback
IMMDevice             Activate, OpenPropertyStore, GetId, GetState
IAudioClient          Initialize, GetBufferSize, GetMixFormat, IsFormatSupported,
                      GetDevicePeriod, GetCurrentPadding, Start, Stop, Reset,
                      SetEventHandle, GetService, GetStreamLatency
IAudioRenderClient    GetBuffer, ReleaseBuffer
```

约 30 个方法。`QueryInterface` 还要应答 `IAudioClient2` / `IAudioClient3`，不少应用会 QI 更高版本。

**最容易低估的一环是时钟**：没有真实设备就没有硬件时钟，`GetCurrentPadding` 的返回值决定应用的写入节奏，必须按采样率自己推演，否则应用会跑飞或卡死。

### 投递方式：改 COM 注册，不用注入

`MMDeviceEnumerator` 的 CLSID 已注册在注册表里指向 `MMDevAPI.dll`。**容器里本来就没有音频，把这个 `InprocServer32` 改指向自己的 shim DLL 是无损的**：

```
HKLM\SOFTWARE\Classes\CLSID\{BCDE0395-E52F-467C-8E3D-C4579291692E}\InprocServer32
    (Default) → shim DLL 路径
```

这样容器里任何进程做 `CoCreateInstance(CLSID_MMDeviceEnumerator)` 都会加载我们的实现，**不需要 `SetWindowsHookEx`、不需要 Detours、不需要注入**。需要出 32 位和 64 位两份 DLL。

### 不需要覆盖的层

`dsound.dll` 和 `XAudio2_9.dll` 在容器里不存在，任何依赖它们的程序本来就跑不了音频，**不在覆盖范围内**。`winmm.dll` 存在但 0 设备，`waveOutOpen` 直接失败，伪造它是可选的补充（纯 C API，比 WASAPI 简单得多）。

---

## 4. 成本估计

| 部分 | 估计 |
|---|---|
| WASAPI shim（4 个接口 + COM 注册替换 + 格式协商 + 自造时钟） | 1～2 周 |
| waveOut 伪造（纯 C API） | 2～3 天，可选 |
| 传输层（IDL 扩展 + 客户端音频线程 + 宿主机 SDL 播放） | 3～5 天，**技术上是确定的部分** |

传输层与图像那三个 RPC 方法完全对称，不难。真正的不确定点在 shim 的时钟和格式协商。

**客户端要加一个独立线程**：现有客户端是单线程循环（抓屏 → 编码 → 发送 → 拉事件），而音频捕获是回调驱动的，必须单独线程 + 队列，再与视频共用同一个 binding handle 发出。

---

## 5. 什么时候不该做

- 现有用例（IT 管理员跑 GUI 安装程序、导出镜像、轻量测试可疑程序）**都不需要声音**
- 如果目标是"看视频 / 听声音"，**容器不是合适的载体**。虚拟机音频直通成熟得多
- **RDP 这条路也不通**：镜像里有完整的 RDP 服务端栈，但实测**在容器里根本起不来**（协议栈硬依赖 UMBus + PnP，见 [IMAGE-ANALYSIS.md](IMAGE-ANALYSIS.md) 第 4 节）。而且**这个镜像里没有 `rdpsnd.dll`**——扫遍两层，`System32` 下没有任何名字带 audio/snd/mixer/wave 的文件。两条都不成立

---

## 6. 如果要做，先做去风险 spike

**不要先搭管道。** 先写一个最小 spike 验证核心假设：

在容器里放一个 shim DLL，替换 `MMDeviceEnumerator` 的 COM 注册，然后写一个几行的 WASAPI 播放程序当靶子，确认它能：

1. 拿到一个"默认设备"
2. `Initialize` 成功
3. 在 `GetBuffer` 里拿到**连续、节奏正常**的 PCM

**这一步过了，后面才有意义；过不了说明伪造端点的复杂度超出预期，及时止损。** 约一天，是整件事里唯一真正的技术不确定点。

传输层等 spike 过了再写。
