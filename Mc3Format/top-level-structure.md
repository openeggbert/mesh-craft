# Top-Level Structure

An MC3 file is a valid XML document with a single root element `<mc3>`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.1" model="ExampleModel" unit="meter" coordinate_system="right_handed_y_up">

  <metadata>
    ...
  </metadata>

  <textures>
    ...
  </textures>

  <materials>
    ...
  </materials>

  <definitions>
    ...
  </definitions>

  <objects>
    ...
  </objects>

  <actions>
    ...
  </actions>

</mc3>
```

## Required Attributes on `<mc3>`

| Attribute | Type | Description |
|---|---|---|
| `version` | number/string | MC3 format version. |
| `model` | string | Human-readable model name. |

The `<objects>` child element is required (may be empty).

## Optional Attributes on `<mc3>`

| Attribute | Type | Description |
|---|---|---|
| `unit` | string | Unit scale, e.g. `meter`, `centimeter`, `pixel`, `unit`. |
| `coordinate_system` | string | Recommended: `right_handed_y_up`. |

## Optional Child Elements of `<mc3>`

| Element | Description |
|---|---|
| `<metadata>` | Author, license, notes, etc. |
| `<textures>` | Texture definitions. |
| `<materials>` | Material definitions. |
| `<definitions>` | Reusable object templates/prefabs. |
| `<actions>` | Named actions affecting one or more objects. |

## Metadata

The `<metadata>` element accepts child elements for each key. Recommended elements:

| Element | Type | Description |
|---|---|---|
| `<author>` | string | Name or identity of the model author. |
| `<license>` | string | License identifier, e.g. `CC0`, `MIT`, `CC-BY-4.0`. |
| `<description>` | string | Short human-readable description of the model. |
| `<version>` | string | Model version, e.g. `1.0.0`. |
| `<created>` | string | Creation date in ISO 8601 format, e.g. `2024-01-15`. |
| `<tags>` | string | Space-separated searchable tags for asset libraries. |
| `<source_url>` | string | URL of the original or upstream source. |
| `<notes>` | string | Free-form notes for editors or importers. |

Example:

```xml
<metadata>
  <author>Jane Doe</author>
  <license>CC-BY-4.0</license>
  <description>A simple interactive house model</description>
  <version>1.0.0</version>
  <created>2024-06-01</created>
  <tags>house interactive indoor</tags>
</metadata>
```
