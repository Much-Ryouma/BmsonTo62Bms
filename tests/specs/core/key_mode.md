# キーモード (core/bms)

- 対象: `findKeyMode` / `channelForLane`
- テストコード: `tests/unit/core/KeyModeTest.cpp`
- 根拠: docs/SPEC.md §5.4。beatoraja（jbms-parser）の bmson と BMS のレーンの割り当て

## 何を保証するか

bmson のレーン番号（`x`）を、モードごとに beatoraja が同じ鍵盤として読む BMS のチャンネルに変換すること。

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| KEY-001 | mode_hint が空／beat-7k | モードを探す | beat-7k（レーン 8 本） |
| KEY-002 | beat-14k | レーン 1〜16 のチャンネルを求める | 11・12・13・14・15・18・19・16・21・22・23・24・25・28・29・26。ロングノーツはレーン 8 が 56、16 が 66。`#PLAYER 3`、拡張子 bme |
| KEY-003 | popn-9k | レーン 1〜9 のチャンネルを求める | 11・12・13・14・15・22・23・24・25。ロングノーツはレーン 9 が 65。`#PLAYER 1`、拡張子 pms、NORMAL の判定幅 70% |
| KEY-004 | keyboard-24k／popn-9k のレーン 10 | モードを探す／チャンネルを求める | 見つからない／例外 |
