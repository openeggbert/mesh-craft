# MeshCraft Stabilization — Podúkoly

Cíl: stabilizovat MC3 formát, export pipeline a build bez přepisování editoru.

Stav auditu (2026-06-20):
- README říká "YAML-based" — zastaralé, MC3 je XML
- `mc3togltf` má vlastní duplicitní `Mc3XmlParser` (uses `source`, core uses `src`)
- Top-level CMake neobsahuje `add_subdirectory(mc3togltf)` — testuje se z hardcoded `mc3togltf/build/`
- XSD: `<plane>` má `size` jako `vec2`, ale core parser čte vec3
- `mc3togltf` není buildovaný z root CMake

---

## Fáze 1: Formát — atribut `src` vs `source`

**S1** — Kanonizovat mesh atribut na `src` v mc3togltf parseru
- Soubor: `mc3togltf/src/Mc3XmlParser.cpp` řádek 264: `attr(el, "source")` → `attr(el, "src")`
- Ověřit že `mc3/src/Mc3XmlParser.cpp:281` již používá `src` ✓
- Ověřit XSD
- Status: 📋

**S2** — Ověřit a sjednotit plane size (vec2 vs vec3)
- XSD má `vec2`, core parser čte vec3 (řádek 177-184 Mc3XmlParser.cpp)
- Rozhodnutí: plane je 2D, size = vec2 (width × depth = X × Z)
- Opravit: parser, writer, XSD, test XML soubory
- Status: 📋

---

## Fáze 2: mc3togltf refaktoring

**S3** — Odstranit duplicitní parser z mc3togltf, použít core mc3
- `mc3togltf/src/Mc3XmlParser.*` nahradit voláním `Mc3Document::loadFromFile()`
- `mc3togltf/CMakeLists.txt`: přidat závislost na `mc3` library
- Ověřit že `GltfExporter.cpp` konzumuje `Mc3Document` model
- Status: 📋

**S4** — Přidat `mc3togltf` do top-level CMake
- `CMakeLists.txt`: přidat `add_subdirectory(mc3togltf)`
- Opravit hardcoded cestu `mc3togltf/build/mc3togltf` v testech
- Status: 📋

---

## Fáze 3: Build a testy

**S5** — Ověřit top-level build: mc3 + mcb + mc3tomcb + mc3togltf + MeshCraft
- Spustit `cmake .. && ninja` z root
- Opravit případné chyby
- Status: 📋

**S6** — Validace test XML souborů proti XSD
- Přidat CMake/CTest target pro `xmllint --schema mc3.xsd`
- Opravit test soubory pokud nevalidují
- Status: 📋

**S7** — Roundtrip test: load → save → reload → porovnat
- Přidat C++ test nebo CMake/ctest skript
- Porovnat sémantická pole (ne byte-for-byte)
- Status: 📋

**S8** — mc3togltf export testy
- Export jednoduchých scén (box, sphere, cylinder...) do .gltf a .glb
- Ověřit že testsuite používá top-level-buildnutý binary
- Status: 📋

---

## Fáze 4: Primitiva a CSG

**S9** — Audit primitive support v mc3togltf MeshBuilder
- Které primitiva MeshBuilder umí: Box, Sphere, Cylinder, Cone, Plane, Torus, Capsule, Disk, Grid, IcoSphere
- Neimplementovaná: jasná chybová hláška místo tiché ignorace
- Status: 📋

**S10** — CSG export audit
- Zjistit zda mc3togltf vyhodnocuje CSG (union/difference/intersection) nebo ignoruje
- Buď implementovat (Manifold?) nebo přidat jasnou error hlášku
- Status: 📋

---

## Fáze 5: Dokumentace

**S11** — Opravit README
- Odstranit "YAML-based" → "XML-based (.mc3.xml)"
- Dokumentovat architekuru: mc3, mcb, mc3tomcb, mc3togltf, MeshCraft editor
- Opravit build instrukce (cmake -S . -B build → fungující příkazy)
- Přidat sekci "Current limitations"
- Status: 📋

**S12** — Zkontrolovat a doplnit MC3_FORMAT.md
- Kanonický atribut `src` pro mesh
- Plane size = vec2
- Supported primitives list
- CSG status
- Status: 📋

---

## Prioritní pořadí

1. **S1** — src/source (malá, bezpečná oprava)
2. **S3** — odstranit duplicitní parser (kritické pro konzistenci)
3. **S4** — mc3togltf do top-level CMake
4. **S5** — ověřit top-level build
5. **S2** — plane size
6. **S6** — XSD validace
7. **S11** — README
8. **S9** — primitiva audit
9. **S7** + **S8** — roundtrip + export testy
10. **S10** — CSG
11. **S12** — MC3_FORMAT.md

---

## Poznámky z auditu

- `mc3/src/Mc3XmlParser.cpp:281` — core používá `src` ✓
- `mc3togltf/src/Mc3XmlParser.cpp:264` — togltf používá `source` ✗
- `mc3.xsd` plane: `vec2` — ale core parser čte vec3 (pozor na zpětnou kompatibilitu)
- Top-level `CMakeLists.txt` neobsahuje `add_subdirectory(mc3togltf)`
- `mc3togltf` se builduje samostatně do `mc3togltf/build/`
