
# Simple 'hello world' sample

This sample encodes and then decodes the string "Hello World", and prints the decoded string.
It shows how to instantiate zcbor state variables, and how to use them with the encoding and decoding API.
This sample does not use the zcbor script tool.

### To build:

```
cmake -B build .
cmake --build build
```

### To run:

```
./build/sample_hello_world
```

### Expected output:

> Decoded string: 'Hello World'
