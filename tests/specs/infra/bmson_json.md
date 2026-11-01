# bmson の読み込み (infra/bmson)

- 対象: `parseBmson`
- テストコード: `tests/unit/infra/BmsonJsonTest.cpp`
- 根拠: bmson 1.0.0 仕様、docs/SPEC.md §1.3

## 何を保証するか

bmson の JSON から、変換に使う項目を読み込むこと。仕様で必須の項目がないときや形式が違うときは、理由の分かるエラーにすること。

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| BJS-001 | info のすべての項目、lines、bpm_events、stop_events、ノーツ（x が null のもの、c・up・l を持つもの）、bga を含む bmson | 読み込む | すべての値が読み込まれる。x が null のノーツは x = 0（BGM） |
| BJS-002 | info.init_bpm がない | 読み込む | エラー |
| BJS-003 | resolution がない／0／-480 | 読み込む | 240／240／480（負の値は絶対値にして警告を出す） |
| BJS-004 | 先頭に UTF-8 の BOM がある | 読み込む | 読み込める |
| BJS-005 | JSON として壊れている | 読み込む | エラー |
| BJS-006 | mode_hint がない | 読み込む | beat-7k として読む |
