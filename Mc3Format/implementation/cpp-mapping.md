# Recommended C++ Runtime Mapping

Possible Nova-3D mapping:

```text
MC3 object       -> Nova3D::Node / Entity
primitive        -> Nova3D::Mesh
material         -> Nova3D::Material
texture          -> Nova3D::Texture2D
children         -> scene graph children
action           -> Nova3D::Action / Animation / Script command
collision        -> Nova3D::Collider / PhysicsBody
```

Suggested classes:

```text
MC3Document
MC3TextureDef
MC3MaterialDef
MC3ObjectDef
MC3ActionDef
MC3Importer
MC3MeshBuilder
MC3CSGBuilder
MC3ActionSystem
```
