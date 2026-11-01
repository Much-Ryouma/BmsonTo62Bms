# bmsonto62bms

bmson 形式の譜面を、62進数に対応した拡張 BMS（`#BASE 62`）に変換するソフトウェア。
各ノーツが鳴らす音声の区間を、1 つずつ別の音声ファイルに切り出して書き出す。
対応するキーモードは beat-7k・beat-14k（`.bme`）と popn-9k（`.pms`）。

- 状態: 段階 4（配布用 zip、GitHub のリリース）まで実装済み
- 仕様書: [docs/SPEC.md](docs/SPEC.md)

## ドキュメント

- [仕様書](docs/SPEC.md)
- [コーディング規約](docs/CODING_RULES.md)
- [テスト仕様](tests/specs/README.md)
- [リリース手順](docs/RELEASE.md)

## ビルド

### 必要なもの

- CMake 3.24 以降
- C++20 のコンパイラ（Windows では Visual Studio 2022）
- Qt 6.4 以降（Widgets・Concurrent。画面を作らないときは `-DB2B_BUILD_GUI=OFF` で不要）
- 初回の構成時にインターネット接続（nlohmann/json、CLI11、miniaudio、stb_vorbis、libogg、libvorbis、GoogleTest を取得する）

### 手順（Windows / Visual Studio 2022）

```bat
set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

`build/msvc/src/gui/Release/bmsonto62bms.exe`（画面）と `build/msvc/src/cli/Release/bmsonto62bms-cli.exe`（コマンドライン）ができる。
画面を実行するときは、Qt の DLL を exe の横に置く（`windeployqt build\msvc\src\gui\Release\bmsonto62bms.exe`）。

Qt Creator では、このフォルダの `CMakeLists.txt` を開けばよい。

### 手順（Ninja）

```sh
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug
```

### 配布用の zip を作る

```bat
set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
cpack --preset windows-msvc-release
```

`build/msvc/package/bmsonto62bms-<バージョン>-win64.zip` ができる。中身は exe 2 つ、Qt の DLL（windeployqt で集めたもの）、Visual C++ ランタイム、`README.txt`（[packaging/README.txt](packaging/README.txt)）、ライセンス一式。

`v0.1.0` のようなタグを push すると、GitHub Actions（`.github/workflows/release.yml`）が同じ zip を作り、下書きのリリースに添付する。タグと `CMakeLists.txt` の `VERSION` が違うと止まる。手順は [docs/RELEASE.md](docs/RELEASE.md)。

アプリのアイコンは `resources/icons/` に `app.png` と `app.ico` を置くと組み込まれる（[resources/icons/README.md](resources/icons/README.md)）。置かないときは「62」と書いた仮のアイコンになる。

## 画面の使い方

1. `bmsonto62bms.exe` を起動し、bmson を「開く…」で選ぶか、ウィンドウにドロップする（複数選ぶと譜面セット。下を参照）
2. 左の「書き出しの設定」で、音声の形式・名前のパターン・切り口などを決める。右のスライスの一覧と名前の例がすぐに変わる
3. スライスを選んで「選んだスライスを再生」（またはダブルクリック）、「曲全体を再生」で、書き出す前の音を確かめる
4. 「書き出す」で、音声ファイルと BMS を書き出す
5. 「試聴する音」を「書き出したファイル」にすると、実際に書いたファイルを鳴らせる
6. 「設定 → 外部プレイヤー…」でプレイヤーを設定しておくと、「外部プレイヤーで開く」（F5）で書き出した BMS を開ける

設定は exe の横の `bmsonto62bms.ini` に保存される。

### 複数の難易度（譜面セット）

同じ曲の bmson を 2 つ以上まとめて開くと、基準の譜面を選ぶダイアログが出る（初めはレベルが最も高い譜面。`#DIFFICULTY` は `chart_name` から推し量る）。

- 切り出した音声ファイルはすべての難易度で共有し、同じ音には同じ `#WAV` ID を振る
- 各 BMS には、その譜面で使う ID だけを定義する。36進数か 62進数かは BMS ごとに決まる
- 同じ音を鳴らす区間が譜面どうしでずれているときは、短いほうの切り方に合わせる（長いほうのノーツは短く切り、続きを BGM のノーツで鳴らす）
- それでも基準の譜面にないスライスを使う譜面（先頭からやり直す位置が違うなど）があると、エラーになる。「基準の譜面にないスライスを追加で切り出す」をチェックすると、その音も切り出して書き出す
- 上の「譜面」で表示・試聴する譜面を切り替える。「書き出す」はすべての譜面を書き出す

## コマンドラインツール

```bat
rem スライスの一覧を見る
bmsonto62bms-cli slices song.bmson

rem BMS に変換する（song.bme と音声ファイルが bmson の横にできる）
bmsonto62bms-cli convert song.bmson

rem 複数の難易度をまとめて変換する（基準は another、基準にない切り出し位置の音も切り出す）
bmsonto62bms-cli convert normal.bmson hyper.bmson another.bmson --base another.bmson --allow-extra

rem OGG で書き出し、切り口に 3 ms のフェードアウトをかける
bmsonto62bms-cli convert song.bmson --format ogg --quality 6 --edge fade --fade-out 3

rem 音声ファイルの名前を決める（パラ音は「元の音声名_レーン_連番」、BGM は「小節_ID」）
bmsonto62bms-cli convert song.bmson --key-name "{src}_{lane:02}_{index:03}" --bgm-name "bgm_{measure:03}_{id}"
```

| コマンド | 内容 |
|---|---|
| `slices <bmson>... [--json]` | スライスの一覧（ID、パラ音／BGM 音、ファイル名、元の音声、区間のフレーム位置。譜面セットでは使う譜面も） |
| `convert <bmson>... [-o <出力先>] [--bms-only]` | 音声ファイルと BMS を書き出す（`--bms-only` は BMS だけ。`-o` は 1 譜面のときだけ） |

主なオプション（すべての一覧は [仕様書 §9.3](docs/SPEC.md)）:

| オプション | 内容 | 既定 |
|---|---|---|
| `--encoding sjis\|utf8` | BMS の文字コード | sjis |
| `--difficulty 1〜5` | `#DIFFICULTY`（1 譜面のとき。譜面セットでは `chart_name` から推し量る） | 書かない |
| `--base <bmson>` / `--allow-extra` | 譜面セットの基準の譜面／基準にないスライスを追加で切り出す | レベルが最も高い譜面／切り出さない |
| `--format wav\|ogg` / `--quality 0〜10` | 音声ファイルの形式と OGG の品質 | wav / 5 |
| `--folder` / `--key-name` / `--bgm-name` | 音声ファイルを書き出すサブフォルダ（`#WAV` にはファイル名だけを書く）とファイル名のパターン | 空（BMS と同じフォルダ） / `{song:.16}_k{index:04}` / `{song:.16}_b{index:04}` |
| `--edge cut\|fade` / `--fade-out` / `--fade-in` | 切り口の処理（ms） | cut / 2 / オフ |

名前のパターンで使える変数は `{song}`（曲名）・`{src}`（元の音声ファイル名）・`{lane}`・`{measure}`・`{index}`（連番）・`{id}`（BMS の ID）。`{index:04}` のように printf と同じ書き方で桁数を、`{song:.16}` のように文字数を指定できる。

終了コード: 0 成功、1 変換のエラー、2 引数の誤り

## ディレクトリ構成

```
src/
 ├ core    変換ロジック（スライス、ID、小節、BMS の組み立て、PCM、フェード、名前のパターン）。標準 C++ のみ
 ├ infra   bmson の読み込み、音声の読み込みと WAV / OGG の書き出し、文字コードの変換、ファイル
 ├ app     変換の流れ（ConvertService）、画面が使うセッション、試聴、外部プレイヤー、ports
 ├ gui     Qt の画面
 └ cli     コマンドライン
tests/
 ├ specs   テスト仕様（markdown）
 ├ unit    テストコード
 └ support テスト用の補助（読み戻し用の BMS リーダーなど）
docs/      仕様書、コーディング規約、リリース手順
packaging/ 配布用 zip に入れる説明とライセンス、Windows のバージョン情報
resources/ アプリのアイコン（任意）
```

## ライセンス

[MIT](LICENSE)

依存ライブラリ: Qt（LGPL v3、動的リンク）、nlohmann/json（MIT）、CLI11（BSD-3-Clause）、miniaudio（Public Domain / MIT-0）、stb_vorbis（Public Domain / MIT）、libogg・libvorbis（BSD-3-Clause）、GoogleTest（BSD-3-Clause、テストのみ）
