# Building and running

Build, deployment and troubleshooting for Windows Container View, plus the
things that were established the hard way while getting it to work.

Architecture and use cases are in the `README.adoc` at the repository root.

---

## 1. Requirements

| Component | Notes |
|---|---|
| Visual Studio 2019 or later | The projects use the v142 toolset and need the "Desktop development with C++" workload |
| SDL2 2.32.2 development libraries | See section 2; already in the repository |
| Docker | To run the container |

**x64 only.** The projects have no Win32 configurations.

---

## 2. SDL2

`SDL2/` is a third-party binary SDK (SDL2 2.32.2 plus SDL2_image, about 19 MB
across 108 files) and is **checked in**, so a clone builds without downloading
anything. Only the parts needed to build and run are kept; the upstream
distribution's `docs/`, `cmake/`, `lib/x86/` and top-level files are not included.

```
SDL2/
├── include/SDL2/              92 headers. The source includes <SDL2/SDL.h>, and
│                              there is no flat SDL.h under include/, so the
│                              prefix is required
└── lib/x64/
    ├── SDL2.lib  SDL2_image.lib            to link against
    ├── SDL2.dll  SDL2_image.dll            to run
    ├── SDL2.pdb  SDL2_image.pdb            debug symbols, safe to delete
    └── optional/
        └── libavif-16.dll  libtiff-5.dll  libwebp-7.dll  libwebpdemux-2.dll
            plus their licences
```

`Projects/Sandbox-Server/Sandbox-Server.vcxproj` refers to it through
`$(SolutionDir)SDL2\include` and `$(SolutionDir)SDL2\lib\x64`, so **no absolute
paths** and the repository can live anywhere. A post-build step copies the
runtime DLLs next to `Sandbox-Server.exe`.

To upgrade SDL2, download `SDL2-devel-<version>-VC.zip` and
`SDL2_image-devel-<version>-VC.zip` from
https://github.com/libsdl-org/SDL/releases and copy their `include/SDL2/` and
`lib/x64/` over the existing ones.

---

## 3. Building

Open `Sandbox.sln` in Visual Studio, pick `Debug|x64` or `Release|x64`, and build.
Or from a command line:

```powershell
msbuild Sandbox.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m
```

Both projects write to `Output\x64\<Configuration>\`:

| Project | Output | Role | Needs at runtime |
|---|---|---|---|
| `Sandbox-Server` | `Sandbox-Server.exe` | Host side: RPC server and SDL display | `SDL2.dll`, `SDL2_image.dll` |
| `Sandbox-Client` | `Sandbox-Client.exe` | Container side: capture and input injection | **Nothing but system DLLs** |

Both link the C runtime statically, so no VC++ redistributable is required.

**The client is a single file**, which is the point of splitting them: the
container needs `Sandbox-Client.exe` and nothing else — no SDL2, and no mounting
of the whole output directory.

> This used to be one `Sandbox.exe` that chose a role from its command line, with
> the two roles built as DLLs and `DelayLoadDLLs` keeping SDL out of the
> container. Two executables shrink the container-side deployment from a
> directory to one file and remove the delay-load workaround.

### The client's three parts

`Source/Client/` is divided by what each part is, not by what it happens to touch.
The boundary is the point: each part could be reasoned about, or replaced, without
the other two.

| Part | Files | What it is |
|---|---|---|
| **Desktop** | `Desktop.h`, `Desktop.cpp`, `DesktopCapture.cpp` | The container's graphical environment: window station, screen, cursor, and the input that goes into it. The equivalent of an X server and a window manager, and it would all still make sense with nothing attached to it |
| **Dock** | `Dock.h`, `Dock.cpp` | The strip of window buttons along the bottom, because a container has no shell to restore a minimized window from. Optional, and the option is contained in the part: with `SANDBOX_DOCK` unset every entry point does nothing |
| **Remote** | `Remote.h`, `Remote.cpp`, `FrameProfile.*` | The link to the host — the x11vnc half. The only part that knows about RPC, PNG, or that a host exists |

`main.cpp` brings them up in order: the desktop, then the dock on top of it, then
the program to run, and only then the session with the host. The desktop is not
told the dock exists, and neither is told about the host.

`Source/Common/` holds what both executables share: environment variables and the
`Debug()` stream.

---

## 4. Running

### 4.1 Server (host)

The server **needs a desktop session**, because it creates an SDL window to
display the picture. Server Core has no interactive desktop, so
`SDL_Init(SDL_INIT_VIDEO)` fails and no window appears.

```powershell
cd Output\x64\Debug
.\Sandbox-Server.exe
```

**No command-line arguments.** The window appears and waits for the client.

> A failed start exits **silently** (the exception is caught and a non-zero code
> returned) and prints nothing by default. Do not read "no error" as "it is
> running" — check with `Get-Process Sandbox-Server`, or set
> `$env:SANDBOX_DEBUG=1` and run it again to see why.

### 4.2 Firewall

The server listens on **5001** (changeable with `SANDBOX_PORT`; see section 5).
On the host:

```powershell
New-NetFirewallRule -DisplayName "Sandbox RPC 5001" -Direction Inbound `
    -Protocol TCP -LocalPort 5001 -Action Allow -Profile Any
```

`-Profile Any` matters: virtual adapters (VMware host-only, the Docker NAT) are
often classified as Public.

To confirm it is listening:

```powershell
Get-NetTCPConnection -State Listen -LocalPort 5001 | Select LocalAddress, LocalPort
```

You should see `0.0.0.0:5001`.

### 4.3 Client (container)

The image must be **Server Core** or later; Nano Server removes Win32k and cannot
create windows. **This project is for `--isolation process` only**; Hyper-V
isolation is not supported.

Only `Sandbox-Client.exe` has to go into the container. The host address comes
from `SANDBOX_HOST`:

```
docker run --rm -ti --isolation process ^
  -e SANDBOX_HOST=<host address> ^
  -v C:\path\to\Sandbox-Client.exe:C:\Sandbox\Sandbox-Client.exe:ro ^
  mcr.microsoft.com/windows/servercore:ltsc2025 ^
  C:\Sandbox\Sandbox-Client.exe
```

The command-line argument is **the program to start on the container's desktop**.
With none it is `cmd.exe`, as it always was:

```
  C:\Sandbox\Sandbox-Client.exe notepad.exe
  C:\Sandbox\Sandbox-Client.exe "C:\Program Files\App\app.exe"
```

Paths with spaces are re-quoted, so they can be passed directly.

`<host address>` is an address of the machine running `Sandbox-Server.exe` that
the container can reach:

- Both on the same machine — the container's default gateway, which is the host's
  `vEthernet (nat)` address.
- Across machines (container in a VM, server on a physical host) — the host's
  address on a network the container can see.

Check connectivity from inside the container first:

```
powershell -c "Test-NetConnection <host address> -Port 5001"
```

`TcpTestSucceeded : True` is what you want.

> **The image build must match the host's**, or process isolation will refuse to
> start.

### 4.4 What to expect

The host window shows the container's desktop — the background colour plus
whatever program was started — and responds to mouse and keyboard. With
`SANDBOX_DOCK=1` there is a strip of window buttons below it; see section 4.5.

**Neither program prints anything by default.** To see output, set
`SANDBOX_DEBUG=1` on whichever side you are interested in:

```powershell
$env:SANDBOX_DEBUG=1; .\Sandbox-Server.exe
```

```
docker run ... -e SANDBOX_DEBUG=1 ...
```

With it set, the client prints a per-second frame profile (section 7). Without
it:

| What you see | Meaning |
|---|---|
| No output, process keeps running | Normal |
| No output, exit code 2 | The client was not given `SANDBOX_HOST` |
| No output, exit code 1 | Initialisation failed (the window station could not be reached, or the RPC binding); set `SANDBOX_DEBUG` to see why |
| Exits on its own, exit code 0 | The program it started has closed. The desktop has to stay empty for two seconds first, because a window exists before it is shown and one empty look at it is not proof that anything is gone |

The client does not outlive the program it launches: close that program's last
window and the session ends.

### 4.5 The dock

Set **`SANDBOX_DOCK=1`** on the client and a strip of window buttons appears along
the bottom of the desktop: one per top-level window in the container, with its
title. Without it there is no dock.

A container has no shell. There is no taskbar and no Alt+Tab, and a minimized
window has no pixels for the capture to paint — so without the dock, minimizing a
window loses it.

| Click on | Result |
|---|---|
| A minimized window's button (drawn dimmed) | Restores it and brings it forward |
| A background window's button | Brings it forward |
| The button of the window already in front | Minimizes it |

**It behaves the way a real taskbar does, in both directions.**

*Drawn over the desktop.* The frame is exactly the screen, so the host's window
is the same size whether the dock is on or not, and dragging a window down slides
it behind the dock rather than under the pointer.

*Reserved out of the work area.* The client narrows the container's work area by
the dock's height, which is what a taskbar does with the space it occupies.
Maximized windows stop at the dock instead of disappearing behind it, and
anything that centres itself in the work area is centred in what is left. **The
container sees a smaller usable area, not a smaller screen** — `SM_CYSCREEN` and
`GetDeviceCaps(VERTRES)` are unchanged.

The reservation is read back and checked, and reported under `SANDBOX_DEBUG=1`:

```
Work area reserved: bottom 26 rows
```

If it says `Work area could not be reserved` or `Work area stayed 800x600 rather
than 1024x768`, the system refused it and maximized windows will extend behind the
dock. Everything else about the dock still works.

**The host is not told the dock exists.** The frame is the screen, the desktop
still starts at its top-left corner, and the work area is the container's own
business — so the host's scaling, drawing and pointer mapping all keep working
unchanged. What needs care is the cursor, which is the client's to place: a dock
click never reaches the container, so the container's cursor is never moved
there, and reporting it would strand the pointer while the user aims at a button.
The client reports the host's pointer position instead, and the host draws the
cursor there without knowing why.

> The buttons keep the order the windows first appeared in, not z-order. Z-order
> changes every time a window is activated, and buttons that reshuffle under the
> cursor cannot be clicked.
>
> A minimized window is never drawn as the active one, even when it is still the
> foreground window — which it can be, because minimizing a window does not
> necessarily hand the foreground to anything else.

---

## 5. Configuration

Everything is an **environment variable**. Unset means the default.

| Variable | Default | Read by | Meaning |
|---|---|---|---|
| `SANDBOX_PORT` | `5001` | both | RPC endpoint |
| `SANDBOX_HOST` | none | client | Server address; required |
| `SANDBOX_RESOLUTION` | none | client | Container desktop size, `WIDTHxHEIGHT` |
| `SANDBOX_DOCK` | none | client | Set to `1` to draw the dock below the desktop |
| `SANDBOX_FPS` | `30` | client | Capture rate, 1–240 |
| `SANDBOX_DEBUG` | none | both | Set to `1` to print anything |

The default port is in `Source/Interface/SandboxConfig.h`:

```c
#define SANDBOX_RPC_ENDPOINT L"5001"
```

### SANDBOX_RESOLUTION

The container has no display adapter. win32k falls back to a stub display driver
built into itself, which advertises a fixed set of modes; the client asks for one
of them before starting the program, so it opens at the right size.

```
-e SANDBOX_RESOLUTION=1920x1080
```

Only these are accepted, all at 32bpp and 60 Hz:

```
1024x768   640x480    800x600    1152x864   1280x768   1280x800
1280x1024  1440x900   1600x1200  1680x1050  1856x1392  1920x1200
2560x1600  1600x768   1600x900   1920x1080  1024x600   1366x768
```

Anything else is refused by the driver, and the default — 1024x768 — is used
instead. With `SANDBOX_DEBUG=1` the client says which happened. See
[IMAGE-ANALYSIS.md](IMAGE-ANALYSIS.md), section 4.

### SANDBOX_FPS

The capture cost is roughly fixed per frame, so this trades container CPU for
smoothness: 30 fps is about 30% of one core, 15 fps about half that. See
[PERFORMANCE.md](PERFORMANCE.md).

**Both sides must use the same port.** They each read `SANDBOX_PORT` from their
own environment, so either set it on both or on neither.

> The port used to be a compile-time constant, so changing it meant rebuilding
> both sides — and the two sides are built and deployed separately, the client
> into an image and the server onto a host. The address used to be a
> command-line argument, which crowded out the thing the command line is
> actually needed for: **which program to start**.

### Why a fixed port

The original implementation registered a **dynamic** endpoint with
`RpcServerUseProtseq` plus `RpcEpRegister`, so a client first had to ask the
host's endpoint mapper on TCP 135 which port to use. Three problems:

1. The firewall had to allow 135 *and* a port that changed on every restart.
2. A service with no interactive session cannot raise a firewall prompt, so the
   dynamic port could not be allowed in advance.
3. Across subnets, 135 is frequently blocked.

With `RpcServerUseProtseqEp` and a fixed endpoint the client names the port
directly in its string binding, **so the endpoint mapper is never involved and TCP
135 never has to be open**.

### But the network bottleneck is the call model, not endpoint discovery

A fixed port only solves "cannot connect". What actually hurts over a real network
is **the number of round trips**. The original interface spent four synchronous
RPC calls per frame (`RpcBeginImage`, `RpcSendImageChunkData`, `RpcEndImage`,
`RpcPopEvent`), each waiting for a reply.

Measured inside the container over loopback: **a round trip is only about
0.6 ms**, so four of them are about 2.4 ms — and the rest of the `send` figure is
the server's decode. **On loopback this is not a problem.**

But over a real network each round trip also pays the link's RTT:

| Link | RTT | Four round trips per frame |
|---|---|---|
| Same machine | ~0 | ~2.4 ms |
| Same datacentre | 1 ms | ~6 ms |
| Cross-region | 30 ms | **~126 ms** |
| Mobile or intercontinental | 80 ms | **~326 ms** |

**Cross-region is unusable, and no amount of dirty rectangles, H.264 or better
compression fixes it — the cost is per call, not per byte.**

The interface is therefore **one call per frame**: `RpcSendFrame` carries the
whole frame (or `Size == 0` for none) and the accumulated input events together,
and the cursor bitmap goes separately through `RpcSetCursorShape` because it
changes a few times a minute. See the comments in
`Source/Interface/sandbox.idl`.

> This also fixed a defect in the old interface: the server **dropped frames
> silently** when the previous one had not been presented yet, and the client had
> no way to know. `RpcSendFrame`'s return value now distinguishes accepted from
> dropped, and the profile reports a `dropped by server` count.

---

## 6. Troubleshooting

### `Exception 6ba encountered while sending data`

`0x6BA` is `RPC_S_SERVER_UNAVAILABLE`, a network-layer failure. The client reports
it on its first real network call (`RpcSendFrame`), which means the window station
setup and the binding both succeeded and it simply cannot reach the server.

Check in order:

1. On the host, does `Get-NetTCPConnection -State Listen -LocalPort 5001` return
   anything? If not, the server is not up.
2. In the container, `Test-NetConnection <host address> -Port 5001`. False means
   firewall or routing.
3. Is the address right? See section 4.3.

Note that the client's `Done.` line **does not prove connectivity**:
`RpcStringBindingCompose` and `RpcBindingFromStringBinding` are purely local, and
the connection is not made until the first call.

### The picture is there and the mouse moves, but typing does nothing

If Enter, Backspace and the arrow keys work but **letters and digits do not**, the
**host's input method** is swallowing them: an IME takes the character-producing
keys into its composition buffer and passes control keys through.

A host IME has no business composing into a remote session. `Rendering.cpp` calls
`SDL_StopTextInput()` after creating the window, detaching the IME so every
physical key arrives as a raw scan code.

If the problem comes back, that call is what to check.

### Arrow keys, Home/End and numpad Enter do nothing

The host's SDL-to-DIK table (`scanCodeToKeyNum` in `Rendering.cpp`) sets the high
bit on **extended keys** (`DIK_LEFT = 0xCB`, `DIK_UP = 0xC8`, `DIK_DELETE = 0xD3`
and so on). The client has to strip that marker and set `KEYEVENTF_EXTENDEDKEY`
before injecting, or `SendInput` does not recognise the scan code and discards it.
The handling is in `Desktop::Key`, in `Source/Client/Desktop.cpp`.

### The mouse wheel does nothing

Three things have to line up, and only the first is likely:

1. **The host window needs mouse focus.** Windows delivers `WM_MOUSEWHEEL` to
   the focused window, and SDL's Windows backend drops the event outright when
   the pointer is not over its window. Move the pointer onto the host window.
2. **The scroll arrives with the frame**, not on its own, so it is applied at
   `SANDBOX_FPS`. A burst of scrolling arrives in one batch per frame — nothing
   is lost, it arrives chunky. This is true of the pointer and the keyboard too.
3. **Only whole notches cross.** A precision touchpad reports fractions, which
   are accumulated on the host until they make up one notch, so a very slow
   two-finger drag takes a moment to register. A mouse's wheel is unaffected.

If it is still dead, set `SANDBOX_DEBUG=1` on the client: `SendInput for wheel
returned false` means the injection itself was refused, which is a different
problem from the event never arriving.

### A window I minimized has vanished

A minimized window has no pixels, so the capture skips it, and there is no shell
in the container to restore it from. With `SANDBOX_DOCK=1` its button is in the
dock, drawn dimmed; click it to bring the window back. See section 4.5.

If a window has no button at all, it is either not visible, has no title, or is a
tool window (`WS_EX_TOOLWINDOW`), which the dock filters out the way a taskbar
does.

### A maximized window has a strip of bare desktop down its right edge

Windows gives a maximized window an **invisible resize border**, so
`GetWindowRect` on one returns a rect *larger than the display* —
`(-8, -8, 1032, 750)` on a 1024x768 screen.

`PrintWindow` paints a window into the device context by its rect, so the part
that is actually on screen starts at the border offset rather than at the
context's corner. A scratch bitmap the size of the screen therefore clips the far
edge away **before the copy out**, and no amount of adjusting the `BitBlt`
afterwards can recover it — the columns were never painted.

`DesktopCapture.cpp` allocates the scratch at the screen size plus twice
`SM_CXSIZEFRAME + SM_CXPADDEDBORDER`, clamps the destination to the screen, and
offsets the source by the same amount. The descendants are still positioned
against the **unclamped** rect origin, because their coordinates are relative to
it.

> The bottom edge is affected identically and is easy to miss: without the dock
> the work area is the whole screen, so a maximized rect's bottom runs past it
> and the last rows are lost too. The dock covers them.

### Every window has a thick border around it

The border is real, and it cannot be removed.

Windows gives every resizable window an invisible resize border,
`SM_CXSIZEFRAME + SM_CXPADDEDBORDER` — 8 px at 100% DPI. On a normal desktop that
is the grab area, and DWM draws a hairline frame on top of it, so it is never
seen. **There is no DWM in the container**, and `PrintWindow`'s `PRF_NONCLIENT`
pass goes through the classic, non-composited non-client painting, which does not
know that and fills all 8 px in as a visible frame.

The alternatives are worse. `PW_CLIENTONLY` skips the non-client area entirely,
which takes the title bar with it — nothing left to drag a window by, and no close
button. Drawing the frame and title bar by hand means reimplementing themed
controls, which this container already cannot paint reliably.

### The status bar flickers, or dialog buttons do

There is **no DWM in the container** (`tasklist /fi "imagename eq dwm.exe"`
returns nothing), so there is no compositor and no screen surface to read.
`BitBlt` from a window DC produces nothing usable, and **`WM_PRINT` is the only
way to get pixels** — which is why the whole capture is built on it and on
nothing else (`Source/Client/DesktopCapture.cpp`).

`PrintWindow`'s `PRF_CHILDREN` pass is unreliable for themed common controls:

| Control | Symptom | Status |
|---|---|---|
| Status bar `msctls_statusbar32` | Background only; parts and text missing | Fixed: `PrintWindowTree()` in `DesktopCapture.cpp` re-prints every descendant explicitly |
| Dialog buttons `Button` | Intermittently blank or missing their text, on about 12% of frames | **Unresolved**; a limitation of `WM_PRINT` with no DWM |

To tell whether a given control is affected, capture a run of frames and diff
consecutive ones: the changed region lands exactly on that control's rectangle.

```
frame018 -> frame019 : 811 px  bbox=(591,572)-(690,599)   <- the OK button
```

Directions that were tried and **did not work**, so they need not be retried:

| Attempt | Result |
|---|---|
| Clearing the scratch bitmap `ghTempBitmap` | No effect (but it removed stale-pixel artefacts, so it stayed) |
| Capping capture at 20 fps | No effect on flicker (but it cut CPU a lot, so it stayed) |
| `BitBlt` from the window DC | Picture completely unusable |
| `PW_RENDERFULLCONTENT` | Unusable without DWM |
| `RedrawWindow(RDW_UPDATENOW)` then `PrintWindow` | Made every child `PrintWindow` fail |
| Printing each child twice | No effect |
| Re-printing only status-bar classes | Buttons still flickered |

Also note that **`PrintWindow`'s return value cannot be trusted**:
`PrintWindow failed for child Edit` appears every frame while the edit control
renders perfectly.

### Dragging a window works but clicking does nothing

Dragging relies on motion events; clicking goes through
`MOUSEEVENTF_*DOWN/UP`. Positioning is done by `SetCursorPos`, and the `dx`/`dy`
in `SendInput` are ignored because `MOUSEEVENTF_ABSOLUTE` is not set. That is
intended: the former is what positions the cursor.

---

## 7. Debug output and switches

### SANDBOX_DEBUG

**Neither program prints anything by default.** With `SANDBOX_DEBUG=1` set, the
client prints a per-second frame profile and the server prints its startup
progress.

```
-e SANDBOX_DEBUG=1        # to docker run
$env:SANDBOX_DEBUG=1      # in host PowerShell
```

Everything goes through `Debug()` in `Source/Common/SandboxLog.h`, which discards
when the variable is unset. That keeps noise out of whatever log collects the
output when the programs are started by a script, a container runtime or a service
manager.

> Failures are **not** reported by printing. A failure the caller can act on
> travels as a return value and an exit code (section 4.4), which a script can
> see and which does not depend on the environment.

### The frame profile

With `SANDBOX_DEBUG=1`, the client prints once a second. A frame is split into
`capture`, `dock`, `cursor`, `encode` and `send`; `capture` is split into `alloc`,
`fills`, `windows` and `toGdi+`; `windows` is split into `toplevel` and `child`
with per-call costs. The last two lines give the capacity answer directly.

What the parts mean, the measured numbers, and the A/B experiments behind the
current design are in [PERFORMANCE.md](PERFORMANCE.md).

> **Every frame is captured; there is no frame skipping and no capture switches.**
>
> Both were built, measured and removed. Skipping when the desktop appeared
> unchanged needed a change detector, and neither `SetWinEventHook` nor
> `GetUpdateRect` received anything at all in this container, so the skip
> degenerated to the one-second resync and the picture updated once a second.
>
> The switches that the capture experiments ran behind are gone too — the
> descendant re-print traded 1.1 ms for flicker, and the bare `WM_PRINT` variant
> produced an empty picture. [PERFORMANCE.md](PERFORMANCE.md) sections 3 and 4
> keep the measurements.
>
> The cost of capturing every frame is known and fixed: **30 fps of capture is
> about 30% of one core, or roughly 3 containers per core**, with no heuristics
> involved.

### Measuring

Costs vary enough between window contents that measuring only one case gives the
wrong answer:

| Scene | `PrintWindow calls per frame` | Notes |
|---|---|---|
| Empty desktop | 1 | The floor |
| `cmd.exe` console | 1 | The cost of one full-screen window |
| **A dialog with a dozen children** | **~15** | **The ceiling**; open a property sheet with `rundll32` |

### Debugging key injection

A `cout << "scan:..."` line used to sit in the key branch and was removed once the
values had been checked. To bring it back temporarily, add it to `Desktop::Key`,
in `Source/Client/Desktop.cpp`:

| Key | Log |
|---|---|
| A | `scan:30 vk: 65` |
| Enter | `scan:28 vk: 13` |
| Left arrow | `scan:75 vk: 37` (0x4B once 0xCB's high bit is stripped) |

---

## 8. Known limitations

- **No authentication.** The server registers with
  `RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH`, so anything that can reach port 5001 can
  read the picture and inject input. Suitable for a trusted network only.
- **Dialog buttons flicker.** With no DWM, `WM_PRINT` does not fully support
  themed controls. Status bars are fixed by re-printing descendants; buttons are
  not. Turning the descendant re-print off saves about 1.1 ms and brings the
  flicker back, so it is a trade rather than a saving.
- **One session.** The server's state — texture, cursor, event queue — is global,
  so it serves one container at a time. Serving several means moving that state
  into a per-session structure.
- **Frame rate.** `FrameIntervalMs` in `Remote.cpp` is 33 ms (~30 fps), paced by a
  high-resolution waitable timer. Measured frame time is 35–39 ms; the remainder
  is scheduling latency in the VM.
- **Capacity: about 3 containers per core on an idle desktop, about 1 with a
  dialog open.** The window pass is the largest cost and is close to the real
  cost of painting the tree, so there is nothing redundant to remove; the only
  lever left is capturing less often, which was tried and did not work here.
  Numbers and experiments in [PERFORMANCE.md](PERFORMANCE.md).
- **The client blocks on the server's decode.** `RpcSendFrame` is synchronous and
  the server finishes decoding the PNG and uploading the texture before replying
  (about 4.9 ms measured). With several containers, a busy server stalls all of
  them.
- **NAT hides the source address.** Docker's NAT rewrites container traffic, so
  the server sees every connection coming from the host and cannot tell containers
  apart by address.
- **No audio.** Containers have no audio endpoint and no stream to capture; see
  [AUDIO.md](AUDIO.md).
- **`IMAGE-ANALYSIS.md` was written against `servercore:20H2`**, while the project
  now uses `windows/server:ltsc2025`. The two differ substantially. That document
  opens with a correction.
