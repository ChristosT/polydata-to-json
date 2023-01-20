# Serialize polydata to json

`serialize2json` can be used to serialize a vtk polydata file into a vtk.js compatible json representation.
See  https://kitware.github.io/vtk-js/docs/structures_PolyData.html  for format reference

## Build

```
mkdir build
cd build
cmake -GNinja -DVTK_DIR=<vtk installation path>/lib/cmake/vtk-x.y ../polydata-to-json
cmake --build .
```

## Usage

```
Polydata to vtk.js compatible json representation
Usage: ./serialize2json [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -f,--file TEXT:FILE REQUIRED
                              Polydata file in legacy vtk or XML format
  -t,--format TEXT:{ascii,bson,cbor,messagePack,ubjson}=ascii
                              Serialization format.
  -o,--output TEXT            Output file, if omitted the output will be printed to stdout
```
Examples

```
./serialize2json --file disk.vtk > disk.json
```

## TODO
 - verify doAllCopyOff == ~doAllCopyOn
 - set copyFieldFlags

## License
serialize2json is made available under the Apache License, Version 2.0. For more details, see ./LICENSE.
