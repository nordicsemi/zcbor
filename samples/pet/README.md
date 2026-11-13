
# Sample with code generation

This sample uses zcbor-generated code to decode 3 instances of a data
structure. The 3 data structures are created in 3 different ways:

1. Converted from YAML using the zcbor script (See [CMakeLists.txt](CMakeLists.txt), [pet1.yml](pet1.yml), and pet1.h).
2. Encoded using the zcbor C API.
3. Encoded using zcbor-generated C code.

The generated code is found in [src](src) and [include](include).
To regenerate the files, invoke the [CMakeLists.txt](CMakeLists.txt) file with `-DREGENERATE_ZCBOR=Y`.

[pet1.yml](pet1.yml) files contain the first three representations. Changing [pet1.yml](pet1.yml) will change the printed output for the first pet.
To change the output for the second and third, change the code in [src/main.c](src/main.c).

### To build:

```
cmake -B build .
cmake --build build
```

### To run:

```
./build/sample_pet
```

### Expected output:

> Name: Carrie Cat
> Birthday: 0x0012003400560078
> Species: Cat
> 
> Name: Danny the Dog
> Birthday: 0x0001020304050607
> Species: Dog
> 
> Name: Gary Giraffe
> Birthday: 0x010203040a0b0c0d
> Species: Other
