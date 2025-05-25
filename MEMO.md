# Aki_Pico 覚書

## 基本アイディア

* ROMソケットに下駄を咬ませて SRAM を載せる。
* SRAM は ROM 代わりとする。0000-7FFF アクセス時にデータを返す。
* Z80 起動前に SRAM にデータを積む。Pico を用いる。
* Pico は起動後 RESET をアサートし、Z80 を止めておく。
* SRAM にデータ搭載後、RESET をネゲートして Z80 実行を開始する。
* CLockはAki80内蔵ジェネレータ・水晶を使用する。Dual-84 CPU ボードの場合 19.6606MHz 水晶で、Z80 クロックとしては、9.3303MHz となる。

## ハードウェア

* Dual-84/Aki-80 ボードの ROM ソケットに下駄経由で 1MB SRAM を接続する。D0-D7, A0-A14, CE, OE は ROM の端子をそのまま接続する。1MB SRAM の残り 2 本の制御端子 CE2, WE は Pico と直結する。

* Pico はデータバス8本、MREQ, RFSH, RD, RESET, BUSRQ, BUSAK を接続する。ブートロード機能に使用する。
* それに加えて、IORQ, A0, をつなぐ、上記5本と合わせて、UART 用データレジスタ、ステータスレジスタの I/O 空間を得る。
* 今回の Z80互換CPU、TMPZ84C015 は、いくつかの周辺LSI機能を内蔵しており、それらのレジスタが I/O 空間をすでに占めている。それらと衝突しないように、A7,A6,A5 を監視して、001 (アドレス 20-3F)のみを Pico が使用するようにする。
* Pico の UART0 を使用する端子として、GPIO12(TX), 13(RX)を割り当てる。

<img width=700 src="img/002-TMPZ84C015B_IOmap.png"/>

## SRAM の制御

搭載 SRAM として、32ピン 1Mbit SRAM を用いる。アドレス信号は A17 まである。CEは2本、負論理と正論理(CE2)、あと、OE, WE もある。



CE(負論理)、OEはCPU側に接続されており、Pico から操作できない。CE2, WE を操作することができる、操作するようにする。

Z80 が実行を開始すると、0000-7FFF アクセスは SRAM にアクセスするようになる。A15・MREQ が CS に供給され、MREQ・RD が OE に供給される。SRAM CE2 をアサートしておけば、0000-7FFF 読み出しはそのまま SRAM 読み出しになる。

## bootloading

Z80 実行開始前、Pico は内蔵 Z80 プログラムを SRAM にコピーする。Pico はアドレスバスにアクセスできないので、SRAM 書き込み時のアドレス生成は Z80 に行わせる。

Z80 にリセットをかけ、リセット解除すると、Z80 はアドレスバスに 0000 を出力し、最初の命令フェッチを始める。Picoがあらかじめ WAIT をアサートしているので、Z80 はウェイトサイクルに入っている。このとき、Pico は CE2 をネゲートしておき、代わりに Pico の D0-D7 に最初の命令を出力し、WAIT をネゲートする。するとZ80は最初の命令を実行後、次の命令アドレス(おそらく0001)を出力し、命令フェッチを行う。このときも WAIT がアサートされており、Pico が次の命令をおくチャンスがある。

## BASIC インタプリタ

例によって BASIC インタプリタを載せて ASCIIART.BAS を実行して時間を測る。Aki-80 用 BASIC インタプリタは[電脳伝説さんがかつて公開してくださったもの](https://github.com/vintagechips/aki80basic)を使う。解説は[彼のブログ](https://vintagechips.wordpress.com/2018/09/10/aki-80_basic/)にある。

<img width=700 src="img/003-Aki80BASIC_schematic.png"/>

* CN2-7 と 10, CN2-3 と 6, CN3-22 もつないでおく。
* CN2-5 を RXD(pin3)、CN2-4 を TXD(pin4) につなぐ。
* CN2-9 をGND に落としておく。

