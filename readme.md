## ゲーム説明
-	リズム天国に収録されている「しわけ」というゲームを基に、実際に手を動かして遊ぶリズムゲームを作りました。
-	左から飛んでくるオブジェクトに対してリズムよく、クラップ(キャッチ)またはスラップ(はたき落とす)しましょう。
-	技術的な話は readme.pdf をダウンロードしてご覧ください。

## 対応OS
Ubuntu (Ubuntu 18.04.5 LTS)

## 必要となるライブラリ
-	OpenCV 3.2.0
	```bash
	sudo apt-get update
	sudo apt-get install libopencv-dev
	```
	OS のバージョンによっては OpenCV4.2 がインストールされてしまうため注意
-	OpenGL
	```bash
	sudo apt-get install freeglut3 freeglut3-dev
	```
-	libpulse
	```bash
	sudo apt-get install libpulse-dev
	```

## コンパイル
```bash
make main
```

## 実行
```bash
./main [opitons]
```

## オプション
-	`-l <filename>`
	-	読み込む曲を選択する(拡張子不要)
	-	選べるのは bo-tto_hidamari, original, shining_star, sirius の４種類
	```bash:sample
	./main -l original
	```
-	`-d <device name>`
	-	カメラを指定する(指定しない場合はデフォルトのカメラが使用される)
	-	楽な体勢でできるようにWebカメラがおすすめ
	-	Linux の場合 /dev/video0, /dev/video1 など
	```bash:sample
	./main -d /dev/video1
	```
-	`-s <time>`
	-	秒数(整数)を指定するとその秒数だけスキップされた状態で曲が始まる
	-	sirius は一部分しか作っていないので必須
	```bash:sample
	./main -l sirius -s 110
	```
-	`-t <time>`
	-	手を動かしてからゲーム内の手が動くまでのタイムラグをミリ秒単位(整数)で指定する(デフォルト:130)。
	-	キーボード操作ではずれないが実際に手で操作するとずれる場合はこの値を変更して補正する必要がある。
	-	ゲーム中、標準出力でどれだけタイミングがずれているか表示されるのでその値を参考にすると良い。

	例えば平均 -90 ミリ秒のズレであれば、適正は 130 - 90 = 40 ミリ秒である可能性があるので
	```bash:sample
	./main -t 40
	```
-	`-g`
	-	カメラを起動せず、キーボードだけで操作する場合に使う
	```bash:sample
	./main -g
	```

## キー操作
-	Space	開始
-	i		中止
-	c		クラップ(キャッチ)
-	s		スラップ(はたき落とす)

## カメラ側操作

### パラメータ
-	Zeta
	-	小さくしすぎると手の周りも前景として認識される
	-	大きくしすぎると手の一部が背景として認識される
-	T
	-	小さくすると背景の更新が速くなる
	-	背景が変わったり、カメラの向きがずれたときなどに一度小さくしてから大きくすると良い
	-	また、背景が手の色に近い場合はなるべく大きくすると良い(そうしないと止まっている手が背景として認識されるまでの時間が短くなるため)
-	Hand
	-	これ以上の面積を持つ物体のみを手として認識する
	-	手が小さくて認識されない場合はこの値を小さくする必要がある
-	Slap
	-	どの程度速く手を動かせばスラップとして認識されるかを決める値
	-	大きいほどより速く手を動かす必要がある

### 操作
-	横向きで飛んでくる抵抗(4.7 kΩ)をクラップでキャッチし、縦向きで飛んでくる無駄な抵抗(0 Ω)をスラップではたき落とす

## コマンド
-	標準入力からコマンドを入力することもできます。
	-	load <filename>
		-	起動後に曲を変えたい場合に使う
		-	この後スペースキーを押して開始する
		-	詳しくは実行時オプション -l を参照

## カメラの設定
-	guvcview をインストールする
	```bash
	sudo apt-get install guvcview
	```
-	カメラの設定画面を開く (Webカメラなら video1, 内蔵カメラなら video0 であることが多い)
	```bash
	guvcview -r 2 -d /dev/video1
	```
-	メニューバーから「Video」->「Video Codec」->「MJPG - 圧縮」を選択する
-	「イメージコントロール」タブで、自動補正系のオプションがあればすべて無効にする(誤作動の原因になるため)
	-	「ホワイトバランス温度,自動」のチェックを外す
	-	「鮮明度」を 0 にする
	-	「バックライト補正」を 0 にする
	-	「露出,自動」を Manual Mode にして「露出,自動優先」のチェックを外す

## チュートリアル
-	まずはオプションはつけずに起動しましょう(./main)。
-	起動したら初めの数秒は背景を認識するためカメラに映らないように待ちます(もし映ってもパラメータを変更すれば直せるので問題ありません)。
-	次にカメラの前でクラップやスラップをしてきちんと動作するか確認しましょう。
-	もしきちんと動かない場合はパラメータを変更する必要があります。
-	動くのを確認したら main ウインドウの方でスペースを押してゲーム開始です。
-	慣れたら他の曲もやってみましょう。

## 曲
-	bo-tto_hidamari
	-	ぼーっと陽だまり / OtoLogic (https://otologic.jp/free/bgm/pop-music01.html)
-	original
	-	しわけ / みんなのリズム天国
-	shining_star
	-	シャイニングスター / 魔王魂
-	sirius
	-	シリウスの心臓 
	-	作詞・作曲・編曲：傘村トータ
	-	歌：ヰ世界情緒 (Twitter：@isekaijoucho)
	-	(歌: https://www.youtube.com/watch?v=UKZt1vq8bKI)
	-	(MIDI: https://kongmu.myds.me/dkcc/shed/Sirius_no_Shinzou_Piano.html)

## 注意
-	しわけのオリジナル曲は同梱していません。
-	以下の動画から音声を抽出してwavファイルにしたものをoriginal.wavとしてmusicフォルダに入れると遊ぶことができます。
-	https://www.youtube.com/watch?v=ljszIwB3TWI
-	方法は何でも大丈夫ですが、一例を紹介します。
	1.	http://offliberty.io/ で、動画のURLを入力して OFF ボタンを押す
	2.	Extract Audio で音声を抽出する
	3.	music フォルダ内に original.mp3 として保存
	4.	ターミナルから ffmpeg をインストールする
		```bash
		sudo apt update
		sudo apt -y upgrade
		sudo apt install ffmpeg
		```
	5.	music フォルダに移動する
		```bash
		cd /path/to/music
		```
	6.	wav ファイルに変換する(最初の 10 秒はカット)
		```bash
		ffmpeg -ss 10 -i original.mp3 original.wav
		```
