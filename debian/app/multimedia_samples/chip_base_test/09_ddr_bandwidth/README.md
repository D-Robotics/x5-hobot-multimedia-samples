# Cross-Compiling stream.c

This guide provides instructions on how to cross-compile the `stream.c` program, incorporating the static linking of `libgomp.a`. The necessary library is assumed to be copied from the cross-compilation toolchain, which is installed by default in `/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/lib64`. Users can customize the toolchain installation directory as per their requirements.

## Prerequisites

- Cross-compilation toolchain installed (default: `/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/`)

## Steps

### 1. Copy libgomp.a from the Cross-Compilation Toolchain
Assuming the toolchain is installed in the default directory, copy libgomp.a to the project directory:
```
cp /opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/lib64/libgomp.a .
```
### 2. Cross-Compile stream.c
Run the following command to cross-compile stream.c:
```
/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc -O3 -fopenmp -DNTIMES=100 stream.c -L./ -lgomp -o stream
```
