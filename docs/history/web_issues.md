# Web (Emscripten) build — known issues

_Last updated: 2026-07-09._

Two separate, unresolved problems affect the Emscripten (web) build. Both are
out of this repo's scope to fix directly — root causes are in sibling repos
(`../cna`, `../sharp-runtime`) this project may not modify without explicit
owner permission. This file exists so the findings aren't lost; see
`STABILIZATION_WORKLOG.md`'s "Session: 2026-07-09" entry for the full
investigation narrative and `plan.md` STAB-0553 for the tracked row.

---

## 1. Blank canvas — root cause found, fix not yet possible

**Symptom**: the web build loads, initializes a WebGL2 context cleanly (no
console/GPU errors, `EasyGLGraphicsBackend initialized with OpenGL ES 3.0`),
and the app logic runs (`[MeshCraft] New scene` prints) — but the 3D
viewport renders as a blank/black rectangle. Not a crash; nothing visibly
wrong in the console.

**Root cause (confirmed 2026-07-09)**: the live `<canvas id="canvas">` DOM
element ends up `width="0" height="0"`. Confirmed by serving the last-known
build artifacts (`cmake-build-web/MeshCraft.{html,js,wasm}`, dated
2026-07-06) via `python3 -m http.server` and inspecting the post-init DOM
with headless Chrome's `--dump-dom` flag. A 0×0 canvas backing store cannot
show anything, regardless of whether any GL draw call is correct — this
alone fully explains the symptom, independent of shader/render-loop
correctness.

**Where the size gets lost**: the shell HTML's `<canvas>` tag
(`cmake-build-web/MeshCraft.html:124`, Emscripten's stock
`shell_minimal.html`) has no explicit width/height attribute or CSS rule.
CNA's `GraphicsDevice::createOrAttachWindow()`
(`cna/src/Microsoft/Xna/Framework/Graphics/GraphicsDevice.cpp:1262-1281`)
requests a sane default of 1024×768 from `SDL_CreateWindow` (MeshCraft
itself never sets `PreferredBackBufferWidth/Height` — confirmed no
`GraphicsDeviceManager` usage anywhere in `src/MeshCraft/`). So the 0×0 is
introduced somewhere between that `SDL_CreateWindow(1024, 768, ...)` call
and the canvas's final DOM state — inside SDL3's own Emscripten video
backend (third-party, vendored under `CNA_dep`), not in any file this repo
or CNA's own application-level code controls directly.

**Why it hasn't been fixed yet**: a possible in-scope workaround exists —
MeshCraft's own init code could call `emscripten_set_canvas_element_size()`
explicitly after window creation, without touching CNA/SDL3 source. But
this cannot currently be compiled or verified, because of issue #2 below:
the Emscripten build itself no longer completes from scratch.

**Suggested next step once #2 is fixed upstream**: rebuild
(`./build-web.sh`), then try the `emscripten_set_canvas_element_size()`
workaround from MeshCraft's own code; verify with headless Chrome
`--dump-dom` (canvas should report nonzero width/height) followed by a
screenshot (should show non-black 3D content).

---

## 2. Emscripten build no longer completes from scratch (new regression)

**Symptom**: `./build-web.sh` (or any fresh `cmake --build` of
`cmake-build-web`) now fails deep inside `../sharp-runtime`, before ever
reaching MeshCraft's own object files.

**When it appeared**: not present as of the last verified clean web build
(2026-07-06, 449/449 objects, exit 0 — see `plan.md` STAB-0553). Between
then and now, `../sharp-runtime`'s `develop` HEAD moved to `e5e38db`
(2026-07-07), which includes a `System.Xml.XPath` merge. That merge (or
something in the same range) introduced the failures below.

**Exact failures** — 16 distinct errors, all inside `../sharp-runtime`,
found via `cmake --build cmake-build-web --parallel -- -k` (keep going past
failures to see the full picture in one pass):

`-Werror` warnings-promoted-to-errors (Emscripten's clang is stricter here
than the native-Linux GCC build or the MinGW build, which each surface a
different, disjoint set of warnings in this same codebase):

- `-Werror=unused-private-field`:
  - `System/Net/NetworkInformation/NetworkInterface.hpp:38` (`supportsIPv4_`)
  - `System/Net/NetworkInformation/NetworkInterface.hpp:39` (`supportsIPv6_`)
  - `System/IO/FileStream.hpp:25` (`mode_`)
  - `System/Xml/XPath/XmlDocumentNavigator.hpp:43` (`doc_`)
- `-Werror=unused-parameter`:
  - `System/Runtime/InteropServices/RuntimeInformation.cpp:40` (`osPlatform`)
  - `System/Net/Dns.cpp:81` (`hostNameOrAddress`)
  - `System/Net/Dns.cpp:119` (`hostNameOrAddress`)
- `-Werror=unused-const-variable`:
  - `System/Net/Sockets/UnixDomainSocketEndPoint.cpp:23` (`kNativePathLength`)
- `-Werror=unused-result` (ignored `[[nodiscard]]` return value):
  - `System/Xml/XPath/XPathNavigator.cpp:37`
  - `System/Xml/XPath/XPathNavigator.cpp:47`
  - `System/Xml/XPath/XPathNavigator.cpp:135`
  - `System/Xml/XPath/XPathNavigator.cpp:139`
  - `System/Xml/XPath/XPathNavigator.cpp:148`
  - `System/Xml/XPath/XPathNavigator.cpp:149`
- `-Werror=delete-non-abstract-non-virtual-dtor`:
  - Triggered inside libc++'s `<unique_ptr.h>` by `System::Xml::XmlImplementation`
    having virtual functions but a non-virtual destructor.

A genuine **hard compile error**, not a warning — `-Wno-error` cannot fix
this one:

- `System/IO/FileSystemInfo.cpp:31` and `:90` —
  `error: no member named 'clock_cast' in namespace 'std::chrono'`. This is
  a real libc++/Emscripten-toolchain standard-library gap (the
  `std::chrono::clock_cast` C++20 feature isn't available in the libc++
  version bundled with this emsdk), not something `-Werror` relaxation
  addresses.

**Why this repo can't fix it**: all 16 failures are 100% inside
`../sharp-runtime`. Mesh-craft's own `CMakeLists.txt` does not set
`-Werror` anywhere — it comes from `sharp-runtime/CMakeLists.txt:45`'s
hardcoded `target_compile_options(SHARP_RUNTIME PRIVATE -Wall -Wextra
-Werror ...)`, a `PRIVATE` option on a target this project doesn't own, so
there's no override point from mesh-craft's own `CMakeLists.txt` (unlike
`CNA_ENABLE_NET`, which CNA exposes as an overridable cache variable — see
`NEXT.md` §6 for that precedent). Even if `-Werror` could be suppressed
from outside, the `clock_cast` issue would still hard-block the build.

**What still works**: the last-known-good build artifacts from 2026-07-06
(`cmake-build-web/MeshCraft.html` / `.js` / `.wasm`) are untouched on disk
and still load and run correctly in headless Chrome — this file's issue #1
findings were produced using them. Only a *fresh* rebuild is broken.

**Action needed**: report both failure categories to the `../sharp-runtime`
maintainer(s) — same handoff pattern already used for the pre-existing
MinGW cross-compile failures (`NEXT.md` §4). Nothing further can be done
from this repo until that's fixed.
