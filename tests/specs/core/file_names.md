# ファイル名の整え方 (core/naming)

- 対象: `sanitizeFileName` / `finishSoundNames` / `finishFolderName` / `fileStemOf` / `truncateCodePoints`
- テストコード: `tests/unit/core/FileNamesTest.cpp`
- 根拠: docs/SPEC.md §6.2.3、§6.2.4

## 何を保証するか

パターンで作った名前を、Windows で作れる、ぶつからないファイル名にすること。

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| FNM-001 | `a\b/c:d*e?f"g<h>i\|j` | 整える | `a_b_c_d_e_f_g_h_i_j` |
| FNM-002 | `name. .`・`CON`・`nul` | 整える | `name`・`CON_`・`nul_` |
| FNM-003 | 70 文字の名前 | 名前を決める | 64 文字に切り詰め、警告を出す |
| FNM-004 | `Aa`・`aA`・`Aa` | 名前を決める | `Aa`・`aA_2`・`Aa_3`（大文字と小文字を区別せずに比べる）。警告を出す |
| FNM-005 | `...` | 名前を決める | エラー（空になる） |
| FNM-006 | フォルダ `wav`、拡張子 `ogg`／フォルダなし | 名前を決める | `wav/x.ogg`／`x.ogg` |
| FNM-007 | `sounds/drums.final.wav`・`C:\a\b.flac`・`.hidden` | 拡張子を除いたファイル名を取る | `drums.final`・`b`・`.hidden` |
