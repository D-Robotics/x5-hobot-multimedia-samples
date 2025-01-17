#!/bin/bash
echo "set bpu limit."
devmem 0x2052000c
devmem 0x2052000c 32 1
devmem 0x20520010 32 0xd6
devmem 0x2052000c