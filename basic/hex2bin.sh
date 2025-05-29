#! /bin/sh
objcopy --input-target=ihex --gap-fill 0xFF --output-target=binary MBAKIA983.HEX basic.bin