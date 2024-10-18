Parts are defined in toml files.

## Mandatory fields
- name : a string with the name of the part
- mass (if not specified in gltf) : a float with the mass of the part
- one of 
  - primitive_shape : one of {"cube", "cone", "cylinder", "sphere"}
  - mesh_file : a path to a gltf file for the part mesh. Path starts at the project root (OSPData/...)

## Optional fields
- scale : either a float, or an object with fields x, y, z containing a scale for the part model. 
- category : name of the category of the part. Defaults to the empty category.