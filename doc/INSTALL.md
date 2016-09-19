# るあこん を Raspberry Pi にインストールする

## 必要要件

Raspbian Jessie (カーネルバージョン 4.4 以降) が必要です。NOOBS や Raspbian Jessie Lite で動作するかどうかは未確認です（情報歓迎します）。

## 手順

* コンソールモードでログイン。X は使いません。
* [binaries/luacon_raspi.tar.gz](../binaries/luacon_raspi.tar.gz) をダウンロード。
* 適当なディレクトリで展開する。
* `luacon` というディレクトリが生成されるので、*そのディレクトリに移動して*、`./luacon` で起動。

```
$ wget https://github.com/toshinagata/luaconsole/raw/master/binaries/luacon_raspi.tar.gz
$ tar xvfz luacon_raspi.tar.gz
$ cd luacon
$ ./luacon
```

* *注意！* 起動時に `/dev/fb1` が存在していると、そのデバイスが出力先として選択されます。起動しても何も表示されない時は、出力デバイスの設定を確認してください。

次のような表示が出るはずです。

    Lua Console [るあこん] v0.1
    Lua 5.3, Copyright ©  1994-2006 Lua.org, PUC-Rio
    Running startup.lua...
    Ready.

## サンプルプログラム

    load "samegame.lua"
    run

「さめがめ」が起動します。あまり出来のいいサンプルではありませんが、動作確認には使えるでしょう。

    load "shapes.lua"
    run

グラフィック描画のサンプルです。1..6 のキーを押すと、線分・四角形・角丸四角形・楕円・円弧・扇形が描画されます。グラフィック画面は 16 ビットカラーですが、描画色にはアルファ値を含めた 32 ビットカラーを指定できます。

    load "test.lua"
    run

スプライト風の描画サンプルです。ちらつきが激しいのはご勘弁ください。また、このサンプルでは `con.gmode(1)` を指定しています。`con.gmode()` は、`/dev/fb0` と `/dev/fb1` を切り替える機能を持っています（パソコン版では、単に画面を拡大表示するだけです）。このへんの機能はあまり整理されていないので、そのうち仕様を変更するかもしれません。

[\[トップページに戻る\]](../README.md)
