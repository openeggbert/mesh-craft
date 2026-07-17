#!/usr/bin/env python3
"""SYS-W5-01: machine-readable cross-layer field matrix for the MC3 format.

Real pass/fail CTest gate (unlike test/xsd_docs_diff.py, which is explicitly
informational-only). Extracts field/attribute names from each layer of the
MC3 stack and cross-checks them, so a field silently added to one layer but
forgotten in another (e.g. "added to the C++ model, forgot the MCB writer")
fails `ctest` locally instead of surviving until someone notices a round-trip
data-loss bug. Real GitHub Actions CI is parked (AUD-052, owner-gated); this
is the actionable local equivalent, following the same pattern already used
by `plan_consistency` (test/validate_plan_consistency.py).

--------------------------------------------------------------------------
Layers extracted, and why each one is (or isn't) part of the hard gate
--------------------------------------------------------------------------

Six layers form the CORE HARD GATE -- the "attribute/scalar field" shape,
where a name plausibly should round-trip identically through every layer:

  xsd_attr   mc3/mc3.xsd            <xs:attribute ... name="...">
  model      mc3/include/.../*.hpp  C++ struct/class data member names
  xml_read   mc3/src/Mc3XmlParser.cpp    literal attribute names read
  xml_write  mc3/src/Mc3XmlWriter.cpp    literal attribute names written
  mcb_read   mcb/src/McbReader.cpp       literal MCB key names read
  mcb_write  mcb/src/McbWriter.cpp       literal MCB key names written

Two more layers are extracted but kept OUT of the hard gate, for reasons
explained where they're computed below (see "informational-only" markers):

  xsd_elem   mc3/mc3.xsd            <xs:element ... name="..."> (structural
             container/type names, not attribute-shaped -- see "Why XSD
             elements aren't in the hard gate" below)
  examples   test/*.mc3.xml         attribute names actually used in fixture
             files (naturally sparse by design -- an example is not supposed
             to exercise every field)
  exporter   mc3togltf/src/GltfExporter.cpp  best-effort `.field`/`->field`
             reads on variables named after known Mc3 types (obj, mat, light,
             cam, env, tex, uv, deform, prim) -- heuristic, no real type
             information, so it is informational-only (see below)

UI (src/MeshCraft/Scene/PropertiesPanel.cpp) and the renderer
(src/MeshCraft/Renderer/SceneRenderer*.cpp) are NOT extracted at all: a
30-second scan of PropertiesPanel.cpp shows its local variable names are
short and generic (sel0, p, st, cs, s, m, o, d, ex, wm, t, cur, csg, ...)
with no reliable name-to-Mc3-type correlation the way GltfExporter.cpp's
`obj`/`mat`/`light`/`cam`/`env`/`tex` naming has; the renderer is split
across 4 files with the same problem. A regex-based extractor there would
mostly match unrelated member accesses on ImGui/GL-context/local aggregate
types, producing more noise than signal without a real C++ AST (out of
scope for this task). Scoping down here rather than shipping an unreliable
gate for those two layers, per this task's own instructions.

--------------------------------------------------------------------------
Normalization rule (READ THIS before treating a flagged gap as real)
--------------------------------------------------------------------------

XSD/XML layers use snake_case ("background_texture", "scale_u"). The C++
model and MCB layers use camelCase ("backgroundTexture", "scaleU"), because
MCB keys are literally the C++ field names (see McbReader.cpp/McbWriter.cpp:
`wFieldStr(o, "scaleU", ...)`, `k == "scaleU"`). Every name is normalized to
snake_case (camelCase -> snake_case via a standard regex conversion, e.g.
"scaleU" -> "scale_u", "subdivisionsX" -> "subdivisions_x") before matching
across layers. This is a purely mechanical rename; it does NOT know that,
say, a field means the same *thing* across layers, only that the identifier
looks the same after case-folding. Two unrelated fields that happen to share
a bare name (e.g. "type" appears on Mc3Object, Mc3Light, Mc3Camera, Mc3Primitive,
Mc3Script, Mc3Trigger, Mc3CrossSection, Mc3ExtrudePath -- all different enums)
are treated as ONE row in this flat, unqualified-by-parent-element namespace,
matching the same simplification test/xsd_docs_diff.py already makes. This
is a known, accepted limitation (documented instead of silently glossed
over) -- true per-element scoping would need a real XML/AST-aware parse of
all six layers, which is out of scope here.

The C++ field extraction itself (model layer) is inherently the fuzziest:
it is a line-oriented heuristic (see extract_model_field_decls()) that looks for
"TYPE identifier(s);" declaration lines inside aggregate bodies (structs/
classes/namespaces), tracking brace depth well enough to skip identifiers
declared inside function bodies (e.g. a local `Mc3LoadPolicy p;` inside a
factory method is correctly excluded). It is not a real C++ parser: template
types with `{`/`=` inside their angle brackets, or multi-line declarations,
would defeat it. Spot-checked against all 21 headers in
mc3/include/MeshCraft/Mc3/ at the time of writing and it extracted exactly
the expected data members with zero leaked methods/locals.

--------------------------------------------------------------------------
Why XSD elements aren't in the hard gate
--------------------------------------------------------------------------

xml_read/xml_write only ever look at *attribute* accessors (`->Attribute(...)`
/ `->SetAttribute(...)`) per this task's own instructions -- they never track
XML *element*/tag names (which are traversed via FirstChildElement/tag
comparisons, a different and much noisier extraction problem). So an XSD
element name like "material" or "objects" will almost never appear in
xml_read/xml_write no matter how correctly implemented the parser/writer
are -- flagging that as a "gap" would be a permanent, unfixable false
positive baked into the tool's own scope, not a real bug. XSD elements are
therefore cross-checked only informationally against model + MCB (which,
being key/value rather than attribute/element, plausibly mirrors both XSD
attributes and XSD elements through the same key mechanism).

--------------------------------------------------------------------------
Allowlist mechanism
--------------------------------------------------------------------------

ALLOWLIST below maps a canonical snake_case field name to a one-line reason
it's an intentional/acceptable asymmetry (internal-only plumbing, disabled-
by-default replacement of the mc3togltf pipeline. It was populated by
literally running this script once with an empty allowlist, then triaging
every reported gap by hand -- see this task's commit history / plan.md
SYS-W5-01 entry for the count of gaps found vs. fixed vs. allowlisted.

Usage: python3 test/field_matrix.py [repo_root]
Exit 0 if no non-allowlisted gap is found in the core 6-layer gate, 1
otherwise (real CTest gate). Informational sections never affect exit code.
"""
import re
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Normalization
# ---------------------------------------------------------------------------

def camel_to_snake(name: str) -> str:
    s1 = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s1)
    return s2.lower()


# ---------------------------------------------------------------------------
# Layer 1: XSD (element / attribute names)
# ---------------------------------------------------------------------------

def extract_xsd_names(xsd_text: str, tag: str) -> set:
    # Same regex approach as test/xsd_docs_diff.py's extract_names().
    return set(re.findall(rf'<xs:{tag}\b[^>]*\bname="([a-zA-Z0-9_]+)"', xsd_text))


# ---------------------------------------------------------------------------
# Layer 2: C++ model field names (mc3/include/MeshCraft/Mc3/*.hpp)
# ---------------------------------------------------------------------------

_MODEL_SKIP_PREFIXES = (
    "static ", "using ", "typedef ", "enum ", "enum class ", "friend ",
    "template", "namespace ", "public:", "private:", "protected:",
    "return ", "#", "//", "class ", "struct ",
)
_MODEL_DECL_RE = re.compile(r'^(.+?)\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)$')
_IDENT_RE = re.compile(r'^[A-Za-z_]\w*$')


def _mask_template_args(s: str) -> str:
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r'<[^<>]*>', '<~>', s)
    return s


_ENUM_TYPE_RE = re.compile(r'\benum\s+class\s+(\w+)')


def extract_enum_type_names(header_text: str) -> set:
    return set(_ENUM_TYPE_RE.findall(header_text))


def extract_model_field_decls(header_text: str):
    """Line-oriented heuristic extraction of (name, raw_type) pairs for C++
    data members.

    Tracks brace nesting well enough to tell "aggregate body" (struct/class/
    namespace -- fields live here) apart from "function body" (locals do NOT
    count as fields) so that e.g. a local `Mc3LoadPolicy p;` inside a factory
    method is excluded. See the module docstring for the accuracy caveat.
    """
    decls = []
    block_is_function = []  # stack; True = current block is a function body

    for raw in header_text.splitlines():
        line = raw.split('//', 1)[0].rstrip()
        stripped = line.strip()
        if not stripped:
            continue

        in_function_body = bool(block_is_function) and block_is_function[-1]

        is_decl_line = (stripped.endswith(';') and '(' not in stripped
                         and ')' not in stripped)
        if (is_decl_line and not in_function_body
                and not stripped.lstrip().startswith(_MODEL_SKIP_PREFIXES)):
            body = stripped[:-1].strip()
            if body:
                brace_idx = body.find('{')
                eq_idx = body.find('=')
                idxs = [i for i in (brace_idx, eq_idx) if i != -1]
                head = body[:min(idxs)].strip() if idxs else body
                if head:
                    m = _MODEL_DECL_RE.match(_mask_template_args(head))
                    if m:
                        type_part = m.group(1).strip()
                        for name in m.group(2).split(','):
                            name = name.strip()
                            if _IDENT_RE.match(name):
                                decls.append((name, type_part))

        # A line like "foo(...) {" or "foo(...) const {" opens a function
        # body; a bare "{" (struct/class/namespace/if/etc.) does not.
        is_func_opener = bool(re.search(r'\)\s*(?:const\s*)?\{', stripped))
        for _ in range(stripped.count('{')):
            block_is_function.append(is_func_opener)
        for _ in range(stripped.count('}')):
            if block_is_function:
                block_is_function.pop()

    return decls


def classify_model_field(raw_type: str, enum_types: set) -> str:
    """'leaf' (attribute-shaped: bool/float/int/string/array<N>/enum) vs
    'container' (element-shaped: vector/map/set/shared_ptr, or a nested Mc3
    struct held by value or via std::optional<Mc3...>).

    Only 'leaf' fields participate in the core attribute-shaped gate --
    otherwise every container/nested-object field (which by construction has
    no XML *attribute* form, only a child *element* form) would show up as a
    permanent, unfixable false-positive gap against xml_read/xml_write. See
    "Why XSD elements aren't in the hard gate" in the module docstring for
    the same reasoning applied on the XSD side.
    """
    t = raw_type.strip()
    if (t.startswith("std::vector<") or t.startswith("std::map<")
            or t.startswith("std::set<") or t.startswith("std::shared_ptr<")):
        return "container"
    if t.startswith("std::optional<"):
        return "container" if "Mc3" in t else "leaf"
    if t in enum_types:
        return "leaf"
    if re.match(r'^Mc3\w*$', t):
        return "container"
    return "leaf"


# ---------------------------------------------------------------------------
# Layer 3/4: XML reader / writer (attribute names only, not element names)
# ---------------------------------------------------------------------------

_XML_READ_HELPER_RE = re.compile(
    r'\b(?:attr|attrF|attrI|attrB|attrVec3|attrCount|attrCountBudgeted|attrFClamped)\s*\('
    r'\s*[a-zA-Z_]\w*,\s*"([a-zA-Z0-9_]+)"')
_XML_READ_DIRECT_RE = re.compile(r'->Attribute\(\s*"([a-zA-Z0-9_]+)"')
_XML_WRITE_RE = re.compile(r'->SetAttribute\(\s*"([a-zA-Z0-9_]+)"')


def extract_xml_read_attrs(parser_text: str) -> set:
    return (set(_XML_READ_HELPER_RE.findall(parser_text))
            | set(_XML_READ_DIRECT_RE.findall(parser_text)))


def extract_xml_write_attrs(writer_text: str) -> set:
    return set(_XML_WRITE_RE.findall(writer_text))


# ---------------------------------------------------------------------------
# Layer 5/6: MCB reader / writer (key names)
# ---------------------------------------------------------------------------

# expectTag()'s 2nd argument tells us the on-disk shape of a key read by the
# reader: TAG_STR/F32/I32/BOOL/VEC3/VEC4 are leaf (attribute-shaped) values;
# TAG_OBJ/ARR/MAP are containers (element-shaped -- a nested object, array,
# or key/value map), same "leaf vs container" split as the model layer (see
# classify_model_field()). Not every "k ==" match is followed by expectTag()
# on the same line (container reads open a multi-line block instead, e.g.
# `else if (k == "tags") { expectTag(tag, TAG_ARR, "tags"); ... }` sometimes
# spans lines, and a couple of call sites check `tag == TAG_ARR` inline
# instead of calling expectTag at all) -- so this searches a bounded window
# after the match rather than requiring same-line adjacency.
#
# The read key variable is almost always literally named `k`, but at least
# one nested per-step loop (Mc3TriggerStep parsing) uses `sk` instead to
# avoid shadowing the outer `k` -- so this matches any short all-letters
# identifier ending in `k`, not just the bare name.
_MCB_READ_KEY_RE = re.compile(r'\b[a-zA-Z]*k\s*==\s*"([a-zA-Z0-9_]+)"')
_MCB_READ_TAG_WINDOW_RE = re.compile(r'TAG_([A-Z0-9]+)')
_MCB_CONTAINER_TAGS = {"OBJ", "ARR", "MAP"}
_MCB_LEAF_TAGS = {"STR", "F32", "I32", "BOOL", "VEC3", "VEC4"}

_MCB_WRITE_LEAF_RE = re.compile(
    r'\bw(?:Field|If)(?:Str|F32|I32|Bool|Vec3|Vec4)\s*\(\s*[a-zA-Z_]\w*,\s*"([a-zA-Z0-9_]+)"')
_MCB_WRITE_CONTAINER_RE = re.compile(
    r'\bwKey(?:Obj|Arr|Map)\s*\(\s*[a-zA-Z_]\w*,\s*"([a-zA-Z0-9_]+)"')


def extract_mcb_read_keys(reader_text: str):
    """Returns (leaf_keys, container_keys) raw (camelCase) key-name sets."""
    leaf, container = set(), set()
    for m in _MCB_READ_KEY_RE.finditer(reader_text):
        name = m.group(1)
        window = reader_text[m.end(): m.end() + 200]
        tag_m = _MCB_READ_TAG_WINDOW_RE.search(window)
        if tag_m is None:
            continue  # no discoverable shape; skip rather than guess
        tag = tag_m.group(1)
        if tag in _MCB_CONTAINER_TAGS:
            container.add(name)
        elif tag in _MCB_LEAF_TAGS:
            leaf.add(name)
    return leaf, container


def extract_mcb_write_keys(writer_text: str):
    """Returns (leaf_keys, container_keys) raw (camelCase) key-name sets."""
    leaf = set(_MCB_WRITE_LEAF_RE.findall(writer_text))
    container = set(_MCB_WRITE_CONTAINER_RE.findall(writer_text))
    return leaf, container


# ---------------------------------------------------------------------------
# Informational layer: example/fixture .mc3.xml files
# ---------------------------------------------------------------------------

_EXAMPLE_ATTR_RE = re.compile(r'\b([a-zA-Z_][a-zA-Z0-9_]*)="')


def extract_example_attrs(examples_dir: Path) -> set:
    names = set()
    for path in sorted(examples_dir.glob("*.mc3.xml")):
        for line in path.read_text(errors="replace").splitlines():
            if line.strip().startswith("<?xml"):
                continue
            names.update(_EXAMPLE_ATTR_RE.findall(line))
    return names


# ---------------------------------------------------------------------------
# Informational layer: mc3togltf/src/GltfExporter.cpp (heuristic)
# ---------------------------------------------------------------------------

_EXPORTER_PREFIXES = (
    "obj", "object", "mat", "material", "light", "cam", "camera",
    "env", "environment", "tex", "texture", "uv", "deform", "prim",
)
_EXPORTER_RE = re.compile(
    r'\b(?:' + "|".join(_EXPORTER_PREFIXES) + r')(?:\.|->)([a-zA-Z_]\w*)\b')
_EXPORTER_STOPLIST = {
    "has_value", "value", "size", "empty", "c_str", "begin", "end",
    "push_back", "emplace_back", "reset", "get", "data", "length",
    "clear", "insert", "erase", "substr", "append", "find", "count",
    "resolvedInstanceDefinitionKey",
}


def extract_exporter_fields(exporter_text: str) -> set:
    return {m for m in set(_EXPORTER_RE.findall(exporter_text))
            if m not in _EXPORTER_STOPLIST}


# ---------------------------------------------------------------------------
# Allowlist: canonical snake_case name -> one-line reason.
# Populated by running this script once and triaging every reported gap.
# ---------------------------------------------------------------------------

_NEAR_FAR_REASON = ("XSD/XML attribute is the short 'near'/'far' (mirrors "
    "OpenGL/glTF convention); the C++ field is qualified as nearPlane/"
    "farPlane instead, most plausibly to avoid colliding with the win32 "
    "<windef.h> near/far calling-convention macros (a well-known C++/Windows "
    "portability gotcha) -- verified both directions round-trip correctly "
    "(Mc3XmlParser.cpp parseCameras(): attrF(c,\"near\",...)/attrF(c,\"far\",...) "
    "-> cam.nearPlane/cam.farPlane; Mc3XmlWriter.cpp writes them back the same way).")

_ARC_HELIX_REASON = ("XSD/XML uses short, context-disambiguated attribute names "
    "('radius'/'angle'/'height'/'turns', shared with other elements and "
    "already matched under those bare names) on the parent <path type=\"arc\"|"
    "\"helix\"> element; the C++ model/MCB use type-prefixed qualified names "
    "(arcRadius/arcAngle/helixRadius/helixHeight/helixTurns) since a single "
    "Mc3ExtrudePath struct holds both arc-only and helix-only parameters "
    "together and needs distinct field names. Verified round-trip in "
    "Mc3XmlParser.cpp parsePath()/Mc3XmlWriter.cpp (writes 'radius'/'angle' "
    "for arc, 'radius'/'height'/'turns' for helix, selected by path.type).")

ALLOWLIST = {
    # --- Group A: qualified model/MCB name vs. a shorter/shared XML attribute name ---
    "far": _NEAR_FAR_REASON, "near": _NEAR_FAR_REASON,
    "far_plane": _NEAR_FAR_REASON, "near_plane": _NEAR_FAR_REASON,
    "arc_angle": _ARC_HELIX_REASON, "arc_radius": _ARC_HELIX_REASON,
    "helix_height": _ARC_HELIX_REASON, "helix_radius": _ARC_HELIX_REASON,
    "helix_turns": _ARC_HELIX_REASON, "turns": _ARC_HELIX_REASON,
    "aspect": "XSD/XML attribute 'aspect' on <camera> maps to the qualified "
              "model field Mc3Camera::orthoAspect (STAB-0695); qualified to "
              "avoid ambiguity with a bare 'aspect' in the same flat C++ "
              "namespace. Verified: Mc3XmlParser.cpp parseCameras() "
              "attrF(c,\"aspect\",1.0f) -> cam.orthoAspect; writer emits it "
              "back as \"aspect\" (Mc3XmlWriter.cpp).",
    "ortho_aspect": "see 'aspect' -- same pair, other direction.",
    "ortho_size": "XSD/XML attribute 'size' on <camera> (shared generic name, "
                  "already matched under 'size' itself) maps to the qualified "
                  "model field Mc3Camera::orthoSize. Verified: "
                  "attrF(c,\"size\",10.0f) -> cam.orthoSize, written back as "
                  "\"size\" unconditionally.",
    "target_object": "XSD/XML attribute 'target' (shared with Mc3Camera::target, "
                      "already matched under that name) maps to the qualified "
                      "model field Mc3Channel::targetObject. Verified: "
                      "Mc3XmlParser.cpp attr(ce,\"target\") -> ch.targetObject "
                      "(and back via SetAttribute(\"target\", ch.targetObject)).",
    "mesh_source": "XSD/XML attribute is the generic 'src' (shared across "
                   "texture/mesh/svg/embed/sound/music elements, already "
                   "matched under 'src'); the model field is qualified as "
                   "Mc3Object::meshSource since Mc3Object also has "
                   "'definition'/'materialOverride' string fields that would "
                   "otherwise collide in the same flat struct. Verified: "
                   "Mc3XmlParser.cpp obj->meshSource = attr(el,\"src\").",
    "interp": "XSD/XML attribute is the short 'interp' on <keyframe>; the "
              "model/MCB field is the qualified Mc3Keyframe::interpolation. "
              "Verified: Mc3XmlParser.cpp ke->Attribute(\"interp\") -> "
              "kf.interpolation, round-trips via Mc3XmlWriter.cpp.",
    "interpolation": "see 'interp' -- same pair, other direction.",

    # --- Group B: represented as a child XML *element* (text or its own "
    #     "attributes), not an attribute on the parent -- out of scope for "
    #     "this tool's attribute-only xml_read/xml_write extraction (and "
    #     "correctly absent from xsd_attr, which only tracks XSD attributes) ---
    "background_color": "Mc3Environment::backgroundColor is written/read via "
                         "a nested <background color=\"...\"/> child element "
                         "(Mc3XmlWriter.cpp: bg->SetAttribute(\"color\",...); "
                         "Mc3XmlParser.cpp: attrVec3(bg,\"color\")), not a "
                         "'background_color' attribute on the parent -- so "
                         "the combined name never appears as a literal "
                         "attribute string in this tool's scope.",
    "background_texture": "Mc3Environment::backgroundTexture is the *text "
                           "content* of a <background_texture> child element "
                           "(bt->SetText(...) / bt->GetText()), not an "
                           "attribute -- outside this tool's attribute-only "
                           "xml_read/xml_write extraction by design (see "
                           "module docstring).",
    "skybox_texture": "same reason as background_texture (element text, "
                       "verified via st->SetText()/st->GetText()).",
    "base_color": "Mc3Material::baseColor is the text content of a "
                  "<base_color> child element (bc->SetText(vec4Str(...))), "
                  "not an attribute.",
    "base_color_texture": "Mc3Material::baseColorTexture is read/written via "
                           "childText(c,\"base_color_texture\") / a child "
                           "element's text, not an attribute on <material>.",
    "normal_texture": "same reason as base_color_texture (childText-based "
                       "child element).",
    "metallic_roughness_texture": "same reason as base_color_texture.",
    "occlusion_texture": "same reason as base_color_texture.",
    "emissive_texture": "same reason as base_color_texture.",
    "emissive_color": "Mc3Material::emissiveColor is the text content of an "
                       "<emissive_color> child element, written only when "
                       "non-zero (Mc3XmlWriter.cpp), not an attribute.",

    # --- Group C: enum inferred from the XML *element tag name* (dispatch), "
    #     "never stored as a separate attribute at all; MCB needs an "
    #     "explicit field since it has no equivalent tag-name dispatch ---
    "csg_type": "Mc3CsgOperation::csgType is inferred from which XML tag is "
                "used (<union>/<difference>/<intersection>), not read from "
                "or written as a separate attribute -- verified in "
                "Mc3XmlParser.cpp's tag-dispatch (tag==\"union\" etc.) and "
                "Mc3XmlWriter.cpp's tag selection by ObjectType. MCB has no "
                "'tag name' concept so it must store this explicitly.",
    "primitive_type": "same reasoning as csg_type: Mc3Primitive::primitiveType "
                       "is inferred from the element tag (<box>, <sphere>, "
                       "<cylinder>, ...), never a separate XML attribute.",

    # --- Group D: model/MCB combine several separate per-axis XSD/XML "
    #     "attributes into one vec3/struct field ---
    "control_in": "Mc3PathPoint::controlIn (array<float,3>) is assembled from "
                  "three separate XML attributes cx/cy/cz on a <point> child "
                  "element (Mc3XmlParser.cpp parsePath(): "
                  "{attrF(p,\"cx\",0), attrF(p,\"cy\",0), attrF(p,\"cz\",0)}), "
                  "not one combined attribute -- unlike position/rotation/"
                  "scale elsewhere, which use one space-separated attribute.",
    "cx": "see control_in -- per-axis XML attribute, no separate model/MCB leaf field.",
    "cy": "see control_in.", "cz": "see control_in.",
    "x": "path-point position (Mc3PathPoint::position, combined into one "
         "vec3 field -- see control_in) and cross-section polygon points "
         "(Mc3CrossSection::Point2D{x,y}) both use separate x/y(/z) XML "
         "attributes on a <point> child element. Point2D is additionally a "
         "single-line nested struct ('struct Point2D { float x, y; };') "
         "that this tool's line-oriented model extractor does not parse "
         "(documented limitation -- see extract_model_field_decls()'s "
         "docstring); Mc3PathPoint.position covers the 3-D case as one "
         "vec3, so 'x' has no matching bare leaf field in the model layer "
         "either way.",
    "y": "see 'x'.", "z": "see 'x' (3-D point case only; Point2D is 2-D).",

    # --- Group E: MCB flattens a small nested struct's fields into "
    #     "prefixed keys instead of a nested object ---
    "dt": "Mc3BezierHandle::dt exists in XSD/XML/model as an attribute on "
          "<handle_left>/<handle_right> child elements, but MCB flattens "
          "Mc3Keyframe::handleLeft.dt/handleRight.dt into top-level "
          "'leftDt'/'rightDt' keys instead of a nested object -- see "
          "left_dt/right_dt. Verified: McbWriter.cpp wIfF32(o,\"leftDt\",...); "
          "McbReader.cpp k==\"leftDt\". Data is not lost, just addressed "
          "under a different key.",
    "dv": "same reasoning as 'dt' (-> leftDv/rightDv).",
    "left_dt": "MCB-only flattened key for Mc3Keyframe::handleLeft.dt -- see 'dt'.",
    "left_dv": "MCB-only flattened key for Mc3Keyframe::handleLeft.dv -- see 'dt'.",
    "right_dt": "MCB-only flattened key for Mc3Keyframe::handleRight.dt -- see 'dt'.",
    "right_dv": "MCB-only flattened key for Mc3Keyframe::handleRight.dv -- see 'dt'.",

    # --- Group F: the XML attribute name is a generic/structural mechanism "
    #     "name, not a per-field name -- the model/MCB store the same data "
    #     "as bare container elements/native map keys with no field name ---
    "file": "XML-only attribute name on <include file=\"...\"/>; the path "
            "string is stored as a bare element of Mc3Document::includes "
            "(vector<string>), and equivalently a bare MCB array entry -- "
            "there is no per-item 'file' field name in model/MCB to match.",
    "key": "XML-only attribute name for generic key/value entries "
           "(<metaentry key=\"...\" value=\"...\"/>, <property name=\"...\" "
           "value=\"...\"/>); stored as native std::map keys in the model "
           "and as a native MCB TAG_MAP in MCB, neither of which has a "
           "'key' field name to match against.",

    # --- Group G: boolean model field vs. a string 'role' enum attribute ---
    "is_cutter": "Mc3Object::isCutter (bool) is round-tripped via a generic "
                 "string 'role' XML attribute (role=\"cutter\"), not a "
                 "dedicated boolean attribute -- verified in "
                 "Mc3XmlWriter.cpp (obj.isCutter -> SetAttribute(\"role\", "
                 "\"cutter\")) and the parser's inverse check.",
    "role": "see is_cutter -- same pair, other direction (model/MCB have no "
            "separate 'role' field; it's folded into the isCutter bool).",

    # --- Group H: XSD/XML stores a list as one delimited string attribute; "
    #     "model/MCB store it as a native vector/array container ---
    "tags": "XSD/XML attribute 'tags' is a single space-delimited string on "
            "the object element; Mc3Object::tags is a vector<string> in the "
            "model and a native MCB array -- different shape per format, "
            "not a data-loss bug (verified split/join round-trips in "
            "Mc3XmlParser.cpp/Mc3XmlWriter.cpp).",
    "variants": "XSD/XML attribute 'variants' is a single space-delimited "
                "string of definition IDs on <instance>; the model stores "
                "it as Mc3Object::variantDefinitions (vector<string>) and "
                "MCB as the array key 'variantDefs' -- same "
                "delimited-string-vs-array shape difference as 'tags'.",

    # --- Group I: STAB-0653 alternate/legacy spelling of default_camera ---
    "default_camera": "STAB-0653: the writer always emits the canonical "
                       "per-camera <camera default=\"...\"/> attribute "
                       "(name \"default\", not \"default_camera\") for "
                       "Mc3Document::defaultCamera; a root-level "
                       "'default_camera' attribute is an alternate spelling "
                       "the *reader* also accepts for back-compat, but the "
                       "writer intentionally never re-emits it. Data is not "
                       "lost -- it round-trips via the canonical attribute "
                       "either way. Verified in Mc3XmlParser.cpp (both "
                       "attr(root,\"default_camera\") and the per-camera "
                       "<cameras default=\"...\"> / <camera default=\"...\"> "
                       "forms) and Mc3XmlWriter.cpp (canonical form only).",
    "default": "the per-camera XML attribute name for the same "
               "Mc3Document::defaultCamera concept as 'default_camera' -- "
               "see that entry. No separate 'default' field exists in "
               "model/MCB; both XML spellings fold into the one "
               "defaultCamera field.",

    # --- Group J: Mc3Script::source is element text, not an attribute; "
    #     "separately, an unrelated legacy 'source=' attribute is accepted "
    #     "as a back-compat synonym for mesh 'src' ---
    "source": "two unrelated things converge on this name: (1) "
              "Mc3Script::source is the *text content* of the <script> "
              "element (Mc3XmlWriter.cpp: xml.NewText(sc.source.c_str())), "
              "not an attribute, so it is correctly absent from xsd_attr/"
              "xml_write's attribute-only scope; (2) Mc3XmlParser.cpp "
              "separately accepts a legacy 'source=' attribute on <mesh> as "
              "a back-compat synonym for the canonical 'src' attribute "
              "('accept legacy source= attribute'), which is why xml_read "
              "(but not xml_write, which only ever emits 'src') sees it.",

    # --- Group K: SYS-W1-04 field_matrix closure (R111 assetMetadata / "
    #     "R110/R101 library+imports / R103 script -- newly wired into "
    #     "xsd_attr+mcb_read+mcb_write; these 4 residual rows are naming-"
    #     "convention asymmetries, not missing data ---
    "max_visibility_distance": "XSD/XML/MCB wire name is the bare "
        "'maxVisibilityDistance' (McbWriter.cpp writeAssetMetadata / "
        "Mc3XmlWriter.cpp 'max_visibility_distance'); the C++ model field is "
        "qualified as Mc3AssetMetadata::maxVisibilityDistanceM (an explicit "
        "unit suffix), so it canonicalizes to a different name "
        "('max_visibility_distance_m') and never matches here. Verified "
        "round-trip in Mc3XmlParser.cpp/McbReader.cpp readAssetMetadata().",
    "namespace": "XSD/XML/MCB wire attribute name 'namespace' is shared by "
        "two distinct, separately-qualified model fields -- "
        "Mc3LibraryInfo::libraryNamespace (<library namespace=\"...\"/>) and "
        "Mc3Import::importNamespace (<import namespace=\"...\"/>) -- neither "
        "of which literally canonicalizes to 'namespace'. Same shape as the "
        "'aspect'/'far'/'near' qualified-name group above. Verified in "
        "Mc3XmlParser.cpp/Mc3XmlWriter.cpp and the new "
        "McbReader.cpp/McbWriter.cpp readLibraryInfo/readImport pair.",
    "script": "XSD/XML/JSON/MCB wire attribute name is the bare 'script' "
        "(Mc3XmlWriter.cpp el->SetAttribute(\"script\", obj->scriptId...); "
        "Mc3JsonWriter.cpp j[\"script\"]); the C++ model field is qualified "
        "as Mc3Object::scriptId to avoid colliding with the unrelated "
        "<script> element (scriptElementType, a Lua script body) in the "
        "same flat namespace this tool uses. Verified round-trip in the new "
        "McbReader.cpp/McbWriter.cpp 'script' key (deliberately not "
        "'scriptId', to agree with XML/JSON's wire name).",
    "tier": "XML/MCB attribute/map-key name for Mc3AssetMetadata::lods "
        "(map<tier, definitionId>) entries -- <lod tier=\"...\" "
        "definition=\"...\"/> in XML, a native MCB TAG_MAP key in MCB "
        "(McbWriter.cpp/McbReader.cpp readAssetMetadata's 'lods' map, same "
        "as the already-allowlisted 'key' entry above). There is no "
        "separate 'tier' field in the model to match -- the tier name is "
        "map-key data, not a field name.",

    # --- MCB-only internal fields with no XML/XSD equivalent by design ---
    "base64_content": "Mc3EmbedGltf::base64Content -- MCB-only inline "
                       "payload; the XML layer stores embeds by external "
                       "file reference (src) or inline element text, not an "
                       "attribute, so this is expected to be absent from "
                       "xsd_attr/xml_read/xml_write.",
    "inline_content": "Mc3SvgTexture::inlineContent -- same reason as "
                       "base64_content (inline SVG markup stored as element "
                       "text/MCB string, not an XML attribute).",
}


# ---------------------------------------------------------------------------
# Matrix construction / reporting
# ---------------------------------------------------------------------------

CORE_GATE_LAYERS = ["xsd_attr", "model", "xml_read", "xml_write", "mcb_read", "mcb_write"]


def build_matrix(names_by_layer: dict, layers: list) -> dict:
    all_names = set()
    for layer in layers:
        all_names |= names_by_layer[layer]
    matrix = {}
    for name in all_names:
        matrix[name] = {layer: (name in names_by_layer[layer]) for layer in layers}
    return matrix


def format_row(name: str, presence: dict, layers: list) -> str:
    marks = " ".join(f"{layer}{'✓' if presence[layer] else '✗'}"
                      for layer in layers)
    return f"  {name}: {marks}"


def main():
    repo = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent

    xsd_text = (repo / "mc3" / "mc3.xsd").read_text()
    model_dir = repo / "mc3" / "include" / "MeshCraft" / "Mc3"
    parser_text = (repo / "mc3" / "src" / "Mc3XmlParser.cpp").read_text()
    writer_text = (repo / "mc3" / "src" / "Mc3XmlWriter.cpp").read_text()
    mcb_reader_text = (repo / "mcb" / "src" / "McbReader.cpp").read_text()
    mcb_writer_text = (repo / "mcb" / "src" / "McbWriter.cpp").read_text()
    examples_dir = repo / "test"
    exporter_path = repo / "mc3togltf" / "src" / "GltfExporter.cpp"

    xsd_attr_raw = extract_xsd_names(xsd_text, "attribute")
    xsd_elem_raw = extract_xsd_names(xsd_text, "element")

    enum_types = set()
    header_texts = {h: h.read_text() for h in sorted(model_dir.glob("*.hpp"))}
    for text in header_texts.values():
        enum_types |= extract_enum_type_names(text)

    model_leaf_raw, model_container_raw = set(), set()
    for text in header_texts.values():
        for name, raw_type in extract_model_field_decls(text):
            shape = classify_model_field(raw_type, enum_types)
            (model_leaf_raw if shape == "leaf" else model_container_raw).add(name)

    xml_read_raw = extract_xml_read_attrs(parser_text)
    xml_write_raw = extract_xml_write_attrs(writer_text)
    mcb_read_leaf_raw, mcb_read_container_raw = extract_mcb_read_keys(mcb_reader_text)
    mcb_write_leaf_raw, mcb_write_container_raw = extract_mcb_write_keys(mcb_writer_text)
    example_raw = extract_example_attrs(examples_dir)
    exporter_raw = extract_exporter_fields(exporter_text=exporter_path.read_text()) \
        if exporter_path.exists() else set()

    # Normalize every layer to canonical snake_case. Only LEAF-shaped model/
    # MCB fields feed the core attribute-shaped gate; container-shaped ones
    # feed the informational element-shaped comparison instead (see
    # classify_model_field()'s docstring for why).
    names_by_layer = {
        "xsd_attr": xsd_attr_raw,                                   # already snake_case
        "xsd_elem": xsd_elem_raw,                                   # already snake_case
        "model": {camel_to_snake(n) for n in model_leaf_raw},
        "model_container": {camel_to_snake(n) for n in model_container_raw},
        "xml_read": xml_read_raw,                                   # already snake_case
        "xml_write": xml_write_raw,                                 # already snake_case
        "mcb_read": {camel_to_snake(n) for n in mcb_read_leaf_raw},
        "mcb_write": {camel_to_snake(n) for n in mcb_write_leaf_raw},
        "mcb_read_container": {camel_to_snake(n) for n in mcb_read_container_raw},
        "mcb_write_container": {camel_to_snake(n) for n in mcb_write_container_raw},
        "examples": example_raw,                                    # already snake_case
        "exporter": {camel_to_snake(n) for n in exporter_raw},
    }

    print("=" * 78)
    print("SYS-W5-01 field matrix -- per-layer extraction counts")
    print("=" * 78)
    for layer in ["xsd_attr", "xsd_elem", "model", "model_container",
                  "xml_read", "xml_write", "mcb_read", "mcb_write",
                  "mcb_read_container", "mcb_write_container",
                  "examples", "exporter"]:
        print(f"  {layer:20s}: {len(names_by_layer[layer])} unique canonical names")
    print()

    # ---- CORE HARD GATE: xsd_attr / model / xml_read / xml_write / mcb_read / mcb_write ----
    core_matrix = build_matrix(names_by_layer, CORE_GATE_LAYERS)

    print("=" * 78)
    print("Core gate matrix (attribute-shaped fields; the 'forgot a layer' bug class)")
    print("=" * 78)
    for name in sorted(core_matrix):
        print(format_row(name, core_matrix[name], CORE_GATE_LAYERS))
    print()

    gaps = []
    allowlisted = []
    for name in sorted(core_matrix):
        presence = core_matrix[name]
        present_count = sum(presence.values())
        missing_layers = [l for l in CORE_GATE_LAYERS if not presence[l]]
        if present_count >= 2 and missing_layers:
            if name in ALLOWLIST:
                allowlisted.append((name, missing_layers))
            else:
                gaps.append((name, missing_layers))

    print("=" * 78)
    print("Core gate results")
    print("=" * 78)
    if allowlisted:
        print(f"ALLOWLISTED ({len(allowlisted)}) -- documented intentional asymmetries, not a failure:")
        for name, missing in allowlisted:
            print(f"  {name}: missing from {missing} -- {ALLOWLIST[name]}")
    else:
        print("ALLOWLISTED (0)")
    print()

    if gaps:
        print(f"GAPS ({len(gaps)}) -- field present in >=2 core layers but missing from >=1:")
        for name, missing in gaps:
            print(f"  FAIL: {name}: missing from {missing}")
    else:
        print("GAPS (0) -- no unallowlisted cross-layer gap found in the core 6-layer matrix.")
    print()

    # ---- Informational: XSD element names vs model / MCB (container-shaped) ----
    elem_layers = ["xsd_elem", "model_container", "mcb_read_container", "mcb_write_container"]
    elem_matrix = build_matrix(names_by_layer, elem_layers)
    elem_missing = sorted(
        name for name, presence in elem_matrix.items()
        if presence["xsd_elem"] and not (presence["model_container"]
                                          or presence["mcb_read_container"]
                                          or presence["mcb_write_container"])
    )
    print("=" * 78)
    print("Informational: XSD ELEMENT names with no model/MCB match at all (non-gating)")
    print("=" * 78)
    print("(Not gated -- xml_read/xml_write only track XML *attributes* in this tool, "
          "never element/tag names, by design; see module docstring 'Why XSD elements "
          "aren't in the hard gate'. A miss here just means no struct/MCB-key shares "
          "the element's name, which is expected for many pure-structural wrapper "
          "elements, e.g. plural container elements like <lights>/<cameras>.)")
    for name in elem_missing:
        print(f"  {name}")
    print(f"  ({len(elem_missing)} of {len(xsd_elem_raw)} XSD element names)")
    print()

    # ---- Informational: examples/fixtures coverage ----
    core_names = set(core_matrix)
    example_gate_names = core_names & (names_by_layer["xsd_attr"] | names_by_layer["model"])
    uncovered_examples = sorted(example_gate_names - names_by_layer["examples"])
    print("=" * 78)
    print("Informational: fields never used in any test/*.mc3.xml fixture (non-gating)")
    print("=" * 78)
    print("(Not gated -- an example file is not expected to exercise every field; this "
          "is purely informational, per this task's own scoping instructions.)")
    print(f"  {len(uncovered_examples)} of {len(example_gate_names)} core-layer field names "
          f"never appear as an attribute in any test/*.mc3.xml fixture.")
    print()

    # ---- Informational: exporter coverage ----
    print("=" * 78)
    print("Informational: mc3togltf/src/GltfExporter.cpp field references (non-gating, heuristic)")
    print("=" * 78)
    print("(Not gated -- extracted via a variable-name-prefix heuristic with no real type "
          "information; known false-positive source: local variables named 'prim' in "
          "GltfExporter.cpp are glTF-JSON-side primitive objects, NOT Mc3Primitive, so "
          "'attributes'/'indices'/'mode' etc. showing up here are not real Mc3 fields.)")
    exporter_hits = sorted(names_by_layer["exporter"] & core_names)
    print(f"  {len(exporter_hits)} core-layer field names referenced in GltfExporter.cpp "
          f"via the {sorted(_EXPORTER_PREFIXES)} variable-prefix heuristic.")
    print()

    print("=" * 78)
    if gaps:
        print(f"FAIL: {len(gaps)} non-allowlisted cross-layer gap(s) found in the core "
              f"6-layer gate (xsd_attr/model/xml_read/xml_write/mcb_read/mcb_write).")
        return 1
    print("PASS: no non-allowlisted cross-layer gap in the core 6-layer gate.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
