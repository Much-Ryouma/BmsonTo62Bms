# コーディング規約

bmsonto62bms の開発で守るルール。IgusaBmsonStudio と同じルールを使う。仕様は [SPEC.md](SPEC.md) を参照。

## 1. 何をどこに書くか

| 書く場所 | 書く内容 | 例 |
|---|---|---|
| コード | **How**（どうやっているか）。名前と構造で読めるようにする | `secondsAt(pulse)`、`segmentContaining(y)` |
| テストコード | **What**（何を保証しているか）。テスト名で読めるようにする | `TIM003_NoteOnStopPulseActivatesAtStopStart` |
| コミットログ | **Why**（なぜ変更したか） | 「プレビューの小節番号が変拍子でずれるため、拍子トラックから計算するようにした」 |
| コードコメント | **Why not**（なぜ別の方法をとらなかったか） | `// Why not resumeSeconds を返す: …` |

### 1.1 コードコメントのルール

- 書いてよいのは **Why not** だけ。ほかの方法を選ばなかった理由で、コードを読むだけでは分からないものに限る
- `// Why not <採らなかった方法>: <理由>` の形で書く
- 書かないもの:
  - 処理の説明（「BPM を更新する」など）。名前を直してコードで伝える
  - 変更履歴、TODO、作者名。履歴は git に、TODO は Issue に書く
  - コメントアウトしたコード
- 例外: ライセンスヘッダが必要な場合のみ

```cpp
// NG: 何をしているかの説明
// STOP の位置なら到達時刻を返す
if (y == segment.startPulse) { return segment.reachSeconds; }

// OK: 別の方法をとらなかった理由
// Why not resumeSeconds を返す: bmson 仕様では STOP と同じ位置のノーツは「停止の開始時」に判定されるため
if (y == segment.startPulse) { return segment.reachSeconds; }
```

### 1.2 コミットメッセージ

```
<種類>(<範囲>): <変更の要約>

<なぜこの変更が必要だったか>
<ほかの方法ではなくこの方法にした理由（あれば）>

Refs: #<Issue 番号>
```

- 種類: `feat` `fix` `refactor` `test` `docs` `build` `ci` `chore`
- 範囲: `core` `infra` `app` `gui` `cli` `tests` など
- 本文には **Why** を書く。「何をしたか」は差分を見れば分かるので要約行だけにする
- テンプレート: `git config commit.template .gitmessage`

## 2. 設計（MVC とレイヤー）

```
gui (View) ─┐
            ├─> app (Controller) ─> core (Model) <─ infra (アダプタ)
cli ────────┘
```

| レイヤー | 置くもの | 依存してよいもの |
|---|---|---|
| `core` | ドメインモデル、計算ロジック（スライス、ID、BMS の組み立て） | 標準 C++ のみ |
| `infra` | ファイル形式と OS とのやりとり（bmson/JSON、音声、文字コード、ファイル） | core、外部ライブラリ |
| `app` | ユースケース（変換の流れ）、ports（インタフェース）とその標準の実装 | core、infra |
| `gui` | Qt の画面、ViewModel | app |
| `cli` | コマンドライン引数の解析と出力 | app |

- `core` から Qt、JSON ライブラリ、ファイル I/O、OS API を呼ばない。外部とのやりとりは `app` の ports を通す
- `gui` にロジックを書かない。GUI でできることは、`app` のユースケースとして CLI からも呼べる形にする（将来の CLI 実行のため）

## 3. 命名とスタイル

- C++20
- 名前空間: `b2b::core` `b2b::infra` `b2b::app` `b2b::gui` `b2b::cli`
- 型: `PascalCase`、関数・変数: `camelCase`、メンバ変数: `camelCase_`、定数: `kPascalCase`
- ファイル名: 型名と同じ `PascalCase.hpp` / `PascalCase.cpp`
- bmson の用語（`y`、`x`、`pulse`、`resolution` など）は仕様の名前をそのまま使う
- 単位を名前に入れる（`seconds`、`pulses`、`samples`、`ms`）。型だけでは区別できないため
- 例外: 入力が不正なときは `std::invalid_argument`。ユーザー操作で起こりうる失敗は戻り値（エラー型）で返す

## 4. テスト

- テストを書く前に、`tests/specs/` に markdown でテスト仕様を書く（書式は [tests/specs/README.md](../tests/specs/README.md)）
- テスト名は `<仕様ID>_<何を保証するか>`（英語、PascalCase）
- 1つのテストで1つのことだけを確かめる
- テストデータはすべて自作または自由ライセンスのものにする。`appendix/` の作品データはリポジトリに入れない

## 5. リポジトリに入れないもの

- `appendix/`（ユーザーの作品データ。`.gitignore` 済み）
- ビルド成果物（`build/`）
- 他者の著作物（ゲームの公式画像、楽曲など）。アイコンや UI 画像は自作する
