# Performance: where a frame's time goes

Measured inside a process-isolated container, with the frame profiler built into
`Sandbox-Client.exe` (`Source/Client/Client.cpp`). Not estimates — set
`SANDBOX_DEBUG=1` and it prints a breakdown once a second.

Architecture is in `README.adoc`; building and running in [BUILD.md](BUILD.md);
what is actually in the container image in [IMAGE-ANALYSIS.md](IMAGE-ANALYSIS.md).

---

## 1. A frame has three phases

| Phase | Contents |
|---|---|
| `capture` | `PrintScreen()`: the full-screen bitmap, the desktop fill, `PrintWindow` over every window, the dock, the conversion to GDI+ |
| `encode` | GDI+ encoding the whole screen as PNG |
| `send` | The `RpcSendFrame` round trip (**including the server's decode and texture upload**; see section 5) |

`capture` is broken down further:

| Part | Contents |
|---|---|
| `alloc` | `CreateCompatibleBitmap` for the full-screen target |
| `fills` | `FillRect` to the desktop colour |
| `windows` | `EnumDesktopWindows` plus every `PrintWindow` and `BitBlt` |
| `dock` | Drawing the strip of window buttons over the bottom of the desktop |
| `toGdi+` | `Bitmap::FromHBITMAP` |

`dock` is newer than the measurements in section 2, which were taken before it
existed, and it is only there when the client is run with `SANDBOX_DOCK=1`. It
does not scale with the screen or with the number of controls on it.

Measured over 21 runs on an idle `cmd` console, on a 1024x768 desktop:

| | us |
|---|---|
| first frame | 904 (one-off: the brushes and font are created here) |
| mean | **~277** |
| range | 211–398 |

About 0.8% of a 33 ms budget, or 3% of an idle frame — the same order as `fills`,
an order below `windows`. It is a **per-frame** cost, so it scales with the frame
rate: 1.6% of a core at 60 fps.

The enumeration is the suspected bulk of it, and `PrintScreen` already enumerates
the desktop's top-level windows a few lines earlier, so passing that list to
`DrawDock` would remove it. Not done: at 0.8% of a budget, for an opt-in switch,
it is not worth the extra parameter.

`windows` is split again into `toplevel` (the `PrintWindow` on each top-level
window) and `child` (the `PrintWindowTree` re-print of descendants), each reported
with a per-call cost.

---

## 2. Measured

### Idle (a cmd console, one `PrintWindow` call)

| | us | share of the 33 ms budget |
|---|---|---|
| `alloc` | 368 | 1.1% |
| `fills` | **4** | 0.01% |
| `windows` | 4427 | 13% |
| `toGdi+` | 2554 | 7.7% |
| **capture** | **8634** | **26%** |
| encode | 1178 | 3.6% |
| send | 1472 | 4.5% |
| **total** | **11284** | **34%** |

### With a dialog open (15 `PrintWindow` calls)

| | us | share of the 33 ms budget |
|---|---|---|
| `alloc` | 366 | 1.1% |
| `fills` | **4** | 0.01% |
| `windows` | **16170** | 49% |
| `toGdi+` | 1222 | 3.7% |
| **capture** | **18003** | **55%** |
| encode | 8040 | 24% |
| send | 5422 | 16% |
| **total** | **31465** | **95%** |

### What that means

**Capacity: about 3 containers per core idle, about 1 with a dialog open.** A
process-isolated container shares the host's CPU, so N containers is N cores'
worth.

**`windows` is the largest single part** — 51% of capture idle, 90% with a dialog.

**Capture cost is linear in the `PrintWindow` call count**, at roughly
`8.1 ms + 0.72 ms x calls`:

| calls/frame | capture mean | |
|---|---|---|
| 1 | 8.6 ms | a full-screen cmd console |
| 4 | 10.4 ms | |
| 15 | 18.9 ms | a dialog with a dozen children |

Per call: **4427 us** for the full-screen console, **82–121 us** for a dialog's
child controls. So it is roughly "1 ms fixed plus something proportional to area".

---

## 3. Three A/B experiments

Each experiment ran behind an environment-variable switch, so none of them needed
a rebuild to compare. **All of those switches have since been deleted**, along with
section 4's `SANDBOX_DESKTOP_SKIP`, `SANDBOX_CAPTURE_MS` and
`SANDBOX_NO_PAINT_PROBE`. What they measured is recorded below; nothing in the
shipped client is configurable except `SANDBOX_HOST`, `SANDBOX_PORT` and
`SANDBOX_DEBUG`.

### Experiment A: take the cursor out of the frame — **worked, merged**

`DrawCursor` painted the cursor into the composite. Measured, that cost
**1.2–1.5 ms per frame** on an idle screen, from `GetIconInfo` allocating two
bitmaps per call plus `DrawIconEx` and two `DeleteObject` calls.

| | before | after |
|---|---|---|
| capture (idle) | 8634 | **6379** |
| mean frame | 11284 | **9225** |
| capacity | ~3/core | **~3/core (18% less CPU)** |

**The stronger reason has nothing to do with CPU**: a cursor is not a window, so
**moving it raises no window event**. While the cursor was baked into the frame,
mouse movement changed the pixels with nothing to signal it — which would have
made any event-driven capture skip miss it entirely.

The fix: position and visibility ride along with every `RpcSendFrame` (they change
constantly), the bitmap goes separately through `RpcSetCursorShape` (it changes a
few times a minute), and **the host composites it**, hiding its own cursor.

### Experiment B: bare `WM_PRINT` instead of `PrintWindow` — **failed**

The hypothesis was that `PrintWindow` sends `WM_PRINT` with `PRF_CHILDREN`, which
recurses into the whole subtree, and that `PrintWindowTree` then paints it again —
duplicated work.

The numbers looked excellent, but **the frame came back as nothing but the desktop
fill, with no windows at all**:

| | before | bare WM_PRINT |
|---|---|---|
| toplevel | 3173 us/call | **788 us/call** |
| capture | 8634 | **3538** |

Two conclusions, the second of which corrects the reading of the first:

1. **The approach does not work.** An HDC is only meaningful in the calling
   process, so a plain `SendMessage(WM_PRINT)` across processes does not paint.
   `PrintWindow` exists precisely to make the cross-process case work — it is
   backed by win32k, not by a user-mode `SendMessage`.
2. **The speedup was an artefact.** `PrintWindow` at 3000 us was *painting*;
   `SendMessage` at 800 us was *painting nothing*. The 2200 us difference is the
   cost of painting at all.

**So the window pass is close to the real cost of painting the tree, and there is
no redundant pass to remove.** The switch stays in the code only to record the
result, so it is not tried again.

### Experiment C: skip the `PrintWindowTree` re-print — **not worth it**

| | before | re-print off |
|---|---|---|
| capture (dialog) | ~18000 | ~17000 |
| picture | correct | status bar loses its parts, buttons flicker |

**About 1.1 ms saved, at the cost of flicker.** The re-print itself is cheap
(82–121 us per call); it exists because `PrintWindow`'s `PRF_CHILDREN` pass is
unreliable for themed common controls. 1 ms is not worth the flicker.

---

## 4. Event-driven frame skipping: **tried, does not work here**

With `capture` proving irreducible, the remaining lever was **capturing less
often**. Measured, 81% of consecutive frames are byte-for-byte identical, and
every one of them paid for a full capture before being discarded.

**Conclusion: neither change detector fires in a process-isolated container. The
code has been deleted.**

### Detector 1: `SetWinEventHook`

```cpp
SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_END, NULL, WinEventProc,
                0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
```

It installed without error and its callback ran on a dedicated message-pumping
thread (which `WINEVENT_OUTOFCONTEXT` requires).

**Across three runs, while windows were opened, dragged and typed into, it
received zero events.** The `change events` diagnostic line never printed once.

### Detector 2: `GetUpdateRect` per window

Orders of magnitude cheaper than a `WM_PRINT` (**~80 us per frame**, against
7500 us for a capture), and driven by the same invalidation the window's own
painting uses.

**`paint` in the `capture reasons` breakdown was always 0** — no window ever
reported a pending update region.

### The consequence

With both silent, the skip degenerated to the one-second resync alone:

```
capture reasons  none=0 forced=0 event=0 paint=0 baseline=0 resync=2
```

**The picture updated once a second**, which reads as unusable lag, and the cost
it saved was not worth that.

### Why the code was deleted rather than kept disabled

This project is **for process-isolated containers only** and will not be used
anywhere else. Since both detectors are silent in the one environment that
matters, "it might work elsewhere" is not a reason to keep dead code — it would
only lead the next person to try it again.

If it is ever picked up, this section has the two failed measurements, where the
hook was installed, why it needed its own message-pumping thread, and what the
diagnostic looked like (captures counted by reason), which is what makes it
obvious at a glance which detector is working.

### What it bought instead

**Capture every frame at 30 fps = 30% of one core = 3 containers per core.**

No heuristics, and nothing that can silently stop working for a class of
application. **That is a number to plan a product against**: a 16-core host is
about 48 containers.

---

## 5. `send` contains the server's decode

This one is easy to misread:

| | `send` p50 |
|---|---|
| No frame (`Size == 0`, events only) | **622 us** |
| With a frame (a 40 KB PNG) | **~5500 us** |

The difference, about **4.9 ms**, is the server. `RpcSendFrame` is synchronous,
and the server finishes `IMG_LoadPNG_RW` (decoding a 1024x768 PNG) and
`SDL_UpdateTexture` before it replies — **all while the client waits**.

Two consequences:

1. **A real round trip is only about 0.6 ms.** The earlier estimate of 1.6 ms per
   call, derived from dividing 6.5 ms by four calls, was the wrong decomposition.
   Merging the calls saved only about 1.8 ms locally.
2. **The saving over a real network is unchanged**: four round trips become one.
   Over a 30 ms link, 120 ms becomes 30 ms. The change was still necessary.
3. **The client's frame loop blocks on the server's decode.** When several
   containers share a server, a busy one stalls them all.

---

## 6. Frame rate: `Sleep` quantisation

Before the fix, frame time measured 41–46 ms (22–24 fps) against a configured
33 ms. The cause: `Sleep` is quantised to the system timer tick, 15.6 ms by
default. Idle, the work was 11.1 ms, so `Sleep(21.9)` became two ticks (31.2 ms)
for a total of 42.3 ms; on a heavy frame the work alone was 30.9 ms and
`Sleep(2.1)` still became one whole tick, for 46.5 ms. Both match the measurement.

With `CreateWaitableTimerExW(..., CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, ...)`,
measured frame time dropped to **35–39 ms (26–28 fps)**. The remaining ~3 ms is
`WaitForSingleObject` scheduling latency in the VM.

---

## 7. Tried and did not work

Recorded so they are not retried:

| Attempt | Result |
|---|---|
| Bare `WM_PRINT` instead of `PrintWindow` | **Empty picture**; see experiment B |
| Skipping the `PrintWindowTree` re-print | Saves 1.1 ms and brings back flicker |
| Deleting the redundant full-screen `Rectangle()` fill | Saves **4 us**. GDI takes a fast path and `fills` was already only 4 us — the whole step is not a cost |
| Caching the full-screen bitmap, or a zero-copy `toGdi+` | **Not done.** Low priority by the numbers: `alloc` 366 us plus `toGdi+` ~1900 us is under 30% of capture |
| Dirty rectangles | **Not done.** Lower priority than event-driven skipping: it only reduces `encode + send` (13.5 ms with a dialog) and does not touch the 16 ms of `windows` at all |

---

## 8. An anomaly that is explained, and one that is not

Early runs showed `cursor` and `toGdi+` varying by more than an order of magnitude
between scenes:

| calls/frame | `cursor` | `toGdi+` |
|---|---|---|
| 1 (idle) | 806–1105 | 2329–2870 |
| 15 (dialog) | **66–91** | **1232–1315** |

while `alloc` (351 vs 362) and `fills` (4 vs 4) were identical, which rules out a
global effect.

**The explanation is CPU frequency scaling.** A later run made it obvious: the
same `ReadCursor` code measured **41–64 us when the loop was mostly idle and 13 us
when it was busy**. An idle loop lets the CPU downclock, so the few operations
that do run are slower each; `alloc` and `fills` are dominated by fixed GDI costs
rather than CPU-bound loops, which is why they did not move.

This means **the "1.2–1.5 ms cursor cost" is inflated by the idle clock** — the
work is real, since the A/B proved it, but the absolute figure varies with
frequency.

**What is still not explained** is why `toGdi+` differs between the two scenes at
all. The cursor is no longer in that path, so if it matters, the place to look is
GDI batching or deferred flushing shifting work between timers.

---

## 9. Reproducing

A normal run prints a block once a second, provided `SANDBOX_DEBUG=1` is set:

```
docker run --rm -ti --name desktest --isolation process ^
  -e SANDBOX_DEBUG=1 ^
  -e SANDBOX_HOST=<host address> ^
  -v C:\path\to\Sandbox-Client.exe:C:\Sandbox\Sandbox-Client.exe:ro ^
  mcr.microsoft.com/windows/server:ltsc2025-amd64 ^
  C:\Sandbox\Sandbox-Client.exe
```

The experiments above no longer have switches to flip, so reproducing one means
re-introducing it. Their measurements stand on their own.

**Measure three scenes**, because costs differ enough that one gives the wrong
answer:

| Scene | Use |
|---|---|
| Empty desktop | The floor |
| `cmd.exe` console | The cost of one full-screen window |
| **A dialog with a dozen children** | **The ceiling**; open a property sheet with `rundll32` |

Read the `worst frame` line and the `=> ~N containers per core` line.
