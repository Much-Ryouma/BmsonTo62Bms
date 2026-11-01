# テスト仕様

このフォルダには、テストが「何を検証しているか」を人が読める形で書いた markdown を置く。
テストコード（`tests/unit/`）は、ここに書いた各項目の ID と 1 対 1 で対応させる。

## フォルダ構成

ソースのレイヤーに合わせる。

```
tests/specs/
 ├ core/   変換ロジック（スライス、ID、BMS の書き出し）
 ├ infra/  ファイルの読み込み、音声、文字コード
 ├ app/    変換のユースケース、読み戻しによる確認、試聴
 └ gui/    画面の自動テスト（画面を出さずに実行）
```

## 書き方

1 ファイル＝1 つの機能。[TEMPLATE.md](TEMPLATE.md) をコピーして使う。

- **ID**: `<機能の略称3文字>-<3桁の連番>`（例: `SLC-001`）。一度振った ID は変えない。項目を削除したら番号は欠番にする
- **前提**: テストを始める前の状態・入力
- **操作**: テストで行うこと
- **期待結果**: 確かめること。数値は単位まで書く。計算の根拠も括弧で書く

## テストコードとの対応

| 仕様 | テストコード |
|---|---|
| `tests/specs/core/slice_plan.md` の `SLC-001` | `tests/unit/core/SlicePlanTest.cpp` の `TEST(SlicePlan, SLC001_...)` |

- テスト名は `<ID からハイフンを除いたもの>_<何を保証するか>`
- 仕様にない項目のテストは書かない。先に仕様へ項目を追加する

## 仕様一覧

| ファイル | 対象 | ID |
|---|---|---|
| [core/object_id.md](core/object_id.md) | 36進数・62進数の ID の変換 | OID |
| [core/timing.md](core/timing.md) | パルスから秒への変換 | TIM |
| [core/measure_layout.md](core/measure_layout.md) | 小節の区切りと小節内の位置 | MEA |
| [core/slice_plan.md](core/slice_plan.md) | スライスの割り出しとまとめ方 | SLC |
| [core/header_values.md](core/header_values.md) | 判定ランク・TOTAL の換算、数値の書き方 | HDR |
| [core/bms_writer.md](core/bms_writer.md) | BMS の書き出し | BMW |
| [core/pcm.md](core/pcm.md) | PCM のサンプルの読み書きと切り出し | PCM |
| [core/fade.md](core/fade.md) | 切り口のフェード | FAD |
| [core/name_pattern.md](core/name_pattern.md) | 名前のパターン | NMP |
| [core/file_names.md](core/file_names.md) | ファイル名の整え方とぶつかったときの扱い | FNM |
| [core/mix.md](core/mix.md) | 試聴用のミックス | MIX |
| [core/launch_command.md](core/launch_command.md) | 外部プレイヤーのコマンドライン | CMD |
| [infra/bmson_json.md](infra/bmson_json.md) | bmson の読み込み | BJS |
| [infra/text_encoding.md](infra/text_encoding.md) | Shift_JIS / UTF-8 への変換 | ENC |
| [infra/audio_probe.md](infra/audio_probe.md) | 音声ファイルの長さとサンプルレートの取得 | AUD |
| [infra/audio_files.md](infra/audio_files.md) | 音声の読み込みと WAV / OGG の書き出し | ACD |
| [app/convert_service.md](app/convert_service.md) | 変換の流れ、メッセージ、書き出し | CNV |
| [app/round_trip.md](app/round_trip.md) | 書き出した BMS を読み戻して bmson と比べる | RTP |
| [app/sound_naming.md](app/sound_naming.md) | 音声ファイルの名前付け | NAM |
| [app/sound_export.md](app/sound_export.md) | 音声ファイルの書き出し | EXP |
| [app/preview.md](app/preview.md) | 試聴と外部プレイヤーの起動 | PRV |
| [app/converter_session.md](app/converter_session.md) | 画面が使う変換のセッション | SES |
| [gui/main_window.md](gui/main_window.md) | 画面の操作 | GUI |
