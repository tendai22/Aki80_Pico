# Aki80 用 BASIC をC言語バイト配列に変換する。

電脳伝説さんのリポジトリから Aki-80 用 BASIC を入手する。

```
$ git clone https://github.com/vintagechips/aki80basic.git
```

9.883MHz 用Intel HEX 形式のファイルから Aki80_Pico でロードするバイナリデータを生成する。バイナリと言っても C 言語のバイト配列である。

Intel HEX 形式をバイナリに変換するには、`objcopy` を用いる。

```
$ objcopy --input-target=ihex --gap-fill 0xFF --output-target=binary MBAKIA983.HEX basic.bin
```

シェルスクリプト `hex2bin.sh` を作っておいた。

バイナリを C 言語配列形式に変換するプログラム `bin2c.c` をさらっと書いておいた。

```
$ cc -o bin2c bin2c.c
$ ./bin2c < basic.bin  > basic.h
```

先頭と末尾に `uint8_t rom_image[] = {`, `};` を手で足しておく。

