# 判定ランクと TOTAL の換算 (core/bms)

- 対象: `judgeHeaderFor` / `bmsTotalFor` / `defaultTotalFor` / `formatDecimal`
- テストコード: `tests/unit/core/HeaderValuesTest.cpp`
- 根拠: docs/SPEC.md §5.8。beatoraja（BMSPlayerRule）の換算

## 何を保証するか

bmson の `judge_rank` と `total` を、beatoraja で読んだときに bmson と同じ判定幅・ゲージになる BMS の値に換算すること。

beatoraja の読み方:
- bmson の `judge_rank` J は、判定幅の割合 J% として使う
- BMS の `#DEFEXRANK` D は、NORMAL の判定幅（75%）に対する割合として使い、D × 75 ÷ 100（整数の割り算）% になる
- bmson の `total` T は、既定の TOTAL に対する割合（T% ）として使う。既定の TOTAL は max(260, 7.605 × n ÷ (0.01 × n + 6.5))（n は総ノーツ数）

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| HDR-001 | judge_rank 100 | 換算する | `#DEFEXRANK 134`（134 × 75 ÷ 100 = 100） |
| HDR-002 | judge_rank 75 | 換算する | `#DEFEXRANK 100`（100 × 75 ÷ 100 = 75） |
| HDR-003 | judge_rank 3 と 0 | 換算する | 3 は `#RANK 3`（beatoraja は 5 未満を #RANK として読む）。0 は書かない |
| HDR-004 | total 100 | 換算する | 書かない（BMS でも既定の TOTAL になる） |
| HDR-005 | total 120、ノーツ 1000 個 | 換算する | 553.0909…（1.2 × 7.605 × 1000 ÷ 16.5） |
| HDR-006 | total 200、ノーツ 50 個 | 換算する | 520（既定が下限の 260 になる） |
| HDR-007 | | 120・150.5・0.1+0.2・1÷3・-0.0 を数値の文字列にする | `120`・`150.5`・`0.3`・`0.333333`・`0`（小数 6 桁まで、末尾の 0 は書かない） |
