# PLAN.md — MeshCraft Feature Plan & Reference

## Co je MeshCraft

**MeshCraft** je 3D scénový editor pro formát `.mc3.xml` — vlastní XML popis scény
podporující primitiva, extruzi, CSG, instance, světla, kamery a prostředí.
Výstupem je `.glb` via `mc3togltf` konvertoru.

**Architektura:**
- Postaveno na **CNA** — XNA-like C++ framework (SDL3 + OpenGL ES 3.2, EasyGL backend)
- UI panely: `SpriteBatch` + 1×1 bílá `Texture2D` (white pixel trick)
- Bitmap font: 5×7 px glyphs, `BitmapFont.hpp/cpp`, 96 ASCII znaků
- Scénová data: `Mc3::Mc3Document` v subknihovně `mc3/` (čisté C++, bez grafiky)
- CNA jako sourozenecký repo `../cna` přes `add_subdirectory`

---

## Legenda stavu

| Symbol | Význam |
|--------|--------|
| ✅ | hotovo a funkční |
| 🔧 | částečně — základy fungují, chybí části |
| 📋 | plánováno, nezačato |

---

## Soubory & Načítání

| Funkce | Stav |
|--------|------|
| Načtení `.mc3.xml` při startu (cesta přes argv) | ✅ |
| Nová scéna (Ctrl+N) | ✅ |
| Uložit (Ctrl+S) | ✅ |
| Uložit jako (Ctrl+Shift+S) | ✅ |
| Export do GLB přes `mc3togltf` (Ctrl+E) | ✅ |
| Otevřít soubor — GUI dialog | 📋 |
| Otevřít soubor — zatím stdin (Ctrl+O) | 🔧 |
| Seznam naposledy otevřených souborů | 📋 |

---

## 3D Viewport — Renderování

| Funkce | Stav |
|--------|------|
| Box, Sphere, Cylinder, Cone, Plane | ✅ |
| Extrude s Line cestou (Rect→box, Circle/Polygon→cylinder) | ✅ |
| Extrude s Arc, Helix, Polyline, Bezier cestami | 🔧 (placeholder box) |
| Extrude s Custom cross-section | 🔧 (placeholder box) |
| CSG: Union/Difference/Intersection — renderování dětí rekurzivně | ✅ |
| Group — renderování rekurzivně | ✅ |
| Instance — resolve z `definitions`, transformace | ✅ |
| Mesh objekty (external geometry) | 🔧 (placeholder box) |
| XYZ mřížka s barvami os | ✅ |
| Wireframe highlight výběru | ✅ |
| Skutečné CSG boolean operace (mesh evalutace) | 📋 |

---

## Kamera

| Funkce | Stav |
|--------|------|
| Orbit (middle-drag), Pan (right-drag), Zoom (scroll) | ✅ |
| Focus na výběr / reset (F) | ✅ |
| Přednastavené pohledy (top/front/side) | 📋 |
| Ortografický mód | 📋 |

---

## Výběr objektů

| Funkce | Stav |
|--------|------|
| Klik — ray-cast AABB picking (rekurzivně přes skupiny) | ✅ |
| Ctrl+klik — multi-výběr | ✅ |
| Ctrl+A — vybrat vše / děti skupiny | ✅ |
| Box/rectangle drag-select | 📋 |

---

## Transform nástroje

| Funkce | Stav |
|--------|------|
| Move gizmo (G) — šipky os, drag = translate | ✅ |
| Scale gizmo (S) — flat-square tipy, drag = scale po ose | ✅ |
| Rotate gizmo (R) — kruhové oblouky, drag = rotace po ose | 📋 |
| Klávesy šipky: nudge (Shift = 0.1 krok), PageUp/Down = Z | ✅ |
| Pivot point (střed objektu / střed světa / kurzor) | 📋 |

---

## Panel hierarchie (levý)

| Funkce | Stav |
|--------|------|
| Rekurzivní strom s odsazením hloubky | ✅ |
| Expand/collapse pro skupiny (klik trojúhelník) | ✅ |
| Klik = výběr, Ctrl+klik = multi-výběr | ✅ |
| Barevný pruh a ikona podle typu objektu | ✅ |
| Drag-and-drop přeřazení (reparenting) | 📋 |
| Přepínač viditelnosti (ikonka oka) | 📋 |

---

## Panel vlastností (pravý)

| Funkce | Stav |
|--------|------|
| Název objektu — klik pro přejmenování (Enter/Esc) | ✅ |
| POS / ROT / SCL — inline editace (klik pole, čísla, Enter) | ✅ |
| Zobrazení barevného vzorku materiálu | ✅ |
| Přiřazení materiálu (výběr ze seznamu) | 📋 |
| Pole Collision type | 📋 |
| Přepínač Visible | 📋 |
| Seznam Tags | 📋 |

---

## Editační operace

| Funkce | Stav |
|--------|------|
| Přidat primitiv (toolbar nebo F1–F5) | ✅ |
| Delete — smazání výběru rekurzivně do libovolné hloubky | ✅ |
| Ctrl+D — deep-copy s příponou `_copy` | ✅ |
| Undo/Redo Ctrl+Z/Y — 20 kroků, deep-copy snímků | ✅ |
| Cut / Copy / Paste (Ctrl+X/C/V) | 📋 |
| Skupinění výběru do nové skupiny | 📋 |
| Rozgrupování (ungroup) | 📋 |

---

## Materiály & Textury

| Funkce | Stav |
|--------|------|
| Zobrazení barevného vzorku materiálu v properties | ✅ |
| Editor materiálů: vytvoření/editace/smazání `Mc3Material` | 📋 |
| Přiřazení textury | 📋 |
| Editace PBR parametrů (metalness, roughness, emissive) | 📋 |

---

## Světla & Kamery (scénová data)

| Funkce | Stav |
|--------|------|
| `Mc3Light` a `Mc3Camera` parsovány z XML | ✅ |
| Světla zobrazena v hierarchii a properties | 📋 |
| Editace světel (typ, barva, intenzita) | 📋 |
| Editace a náhled kamer | 📋 |

---

## Akce & Stavy (animace)

| Funkce | Stav |
|--------|------|
| Datový model `Mc3Action` / `Mc3State` | 📋 |
| Panel editoru akcí | 📋 |
| Vizualizace stavového automatu | 📋 |

---

## UI & Workflow

| Funkce | Stav |
|--------|------|
| Toolbar s přepínáním nástrojů + přidáváním primitiv | ✅ |
| Status bar: počet objektů a výběr | ✅ |
| Titulek okna: nástroj / soubor / stav změn | ✅ |
| F11 — uloží screenshot.ppm | ✅ |
| F12 — vypíše klávesové zkratky do konzole | ✅ |

---

## Developer & Tooling

| Funkce | Stav |
|--------|------|
| Automatický smoke test přes `ctest` | ✅ |
| Auto-screenshot mód (`--screenshot` flag, ukončí po ~2 s) | ✅ |
| Unit testy pro Mc3Document serializaci | 📋 |
| CI pipeline | 📋 |

---

## Architektura — moduly

| Modul | Umístění | Role |
|-------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, stav scény, UI draw |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ mřížka přes `BasicEffect` + `VertexBuffer` |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renderování MC3 objektů + gizma |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus; view+projection matice |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Sleduje vybrané `Mc3Object` shared_ptry |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | GizmoAxis enum + drag stav |
| `BitmapFont` | `include/MeshCraft/Ui/BitmapFont.hpp` | 5×7 font, 96 ASCII znaků, sloupcové kódování |
| `Mc3Document` | `mc3/` sublibrary | Čistá C++ scénová data; load/save XML |
| CNA | `../cna/` sourozenecký repo | SDL3 okno, GL kontext, SpriteBatch, BasicEffect |

**Datový tok:**
1. `LoadContent()` vytvoří renderery; načte `Mc3Document` z XML.
2. `Update()` polluje `Keyboard`/`Mouse`; mutuje kameru, výběr, dokument.
3. `Draw()` vyčistí → renderuje 3D → gizmo → 2D UI overlay přes SpriteBatch.

**Důležité invarianty:**
- `Mc3Document.materials` je `std::map<std::string, Mc3Material>` — iteruj `const auto& [key, mat]`.
- `Mc3Material` používá `baseColor` (4-prvkové float pole), ne `diffuse`.
- `SpriteBatch::Begin()`/`End()` musí ohraničovat všechna 2D `Draw()` volání; nelze vnořovat.
- CNA API: `getCurrentTechniqueProperty()` a `getPassesProperty()` (starší `CurrentTechnique()`/`Passes()` odstraněny v commitu 34ae601).
- `Color` nemá defaultní konstruktor — vždy inicializuj se 4 argumenty.
- Nepřidávat `${meta-gl_SOURCE_DIR}/include` do MeshCraft `CMakeLists.txt` — spustí plnou rekompilaci CNA s existujícími bugy.
- `hierarchyRows_` a `propFieldHits_` se plní v `Draw()` a konzumují v `Update()` (1-frame lag — záměrné, uživatel nevidí).

---

## Příkazy

```bash
# Konfigurace (poprvé)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL

# Build
cd cmake-build-debug && ninja -j$(nproc)

# Spustit
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Spustit s auto-screenshotem (ukončí se po ~2 s)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/editor.ppm

# Smoke test
ctest --test-dir cmake-build-debug -V

# Konverze MC3 → GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml test/house.glb
```

---

## Omezení — nedělat zatím

- **Žádný refaktoring CNA** — opravovat jen to, co blokuje build.
- **Žádný SpriteFont / nativní textový widget** — bitmap font stačí.
- **Žádný GUI file dialog** — mimo scope, dokud neexistuje widget model.
- **Žádné API změny v `Mc3Document`** bez kontroly `mc3togltf` a všech testovacích scén.
- **Žádné hromadné změny include cest** v `CMakeLists.txt` — riziko plné rekompilace CNA.
