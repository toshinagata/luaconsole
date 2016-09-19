#  LuaCon （るあこん） for Raspberry Pi and Mac OS X

  Toshi Nagata

  2016.9.19. Ver. 0.1

##  LuaCon（るあこん）とは

LuaCon（るあこん）は、Raspberry Pi で動作する簡易な Lua プログラミング環境です。X Window を使わないため、軽く動作します。昔の BASIC パソコンをイメージして、初心者が OS のファイル操作に煩わされずに、プログラミングの学習に集中できることを目指しています。

普通のパソコン上でも同じ環境でプログラミングができるように、Mac OS X 版もあります。Windows 版は開発中です。

##  仕様

* Lua 5.3.2 内蔵
* ウィンドウは 640x480, 16ビットカラー
* テキスト画面とグラフィック画面の重ね合わせ表示
* 半角 8x16, 全角 16x16 の日本語フォントを内蔵（さざなみフォント）
* ソースコードの日本語は UTF-8 で記述

## リファレンス

* [Raspberry Pi へのインストール](doc/INSTALL.md)
* [使い方](doc/USAGE.md)
* [Lua 文法まとめ](doc/LUA.md)
* [るあこん拡張機能](doc/EXTENSION.md)

## 今後の予定

* 軽量の日本語入力機能
* ラズパイの GPIO など周辺回路へのアクセス
* Windows, Linux への移植

## ライセンス

* GPLv3 とします。

## 作者ウェブサイト

Digitalians' Alchemy (http://blog.d-alchemy.xyz/)
