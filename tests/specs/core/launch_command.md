# 外部プレイヤーのコマンドライン (core/player)

- 対象: `splitCommandLine` / `buildLaunchCommand`
- テストコード: `tests/unit/core/LaunchCommandTest.cpp`
- 根拠: docs/SPEC.md §9.2

## 何を保証するか

コマンドラインのテンプレートを引数に分け、`{exe}` と `{file}` を置き換えること。パスに空白があっても 1 つの引数のままにすること。

## テスト項目

| ID | 前提 | 操作 | 期待結果 |
|---|---|---|---|
| CMD-001 | `a "b c" d` | 区切る | `a`・`b c`・`d` |
| CMD-002 | 既定のテンプレート、実行ファイル `C:/My Player/play.exe`、ファイル `D:/曲 A/song.bme` | 組み立てる | プログラムは `C:/My Player/play.exe`、引数は `D:/曲 A/song.bme` の 1 つ |
| CMD-003 | `"{exe}" -a -s {file}`、ファイルに空白を含む | 組み立てる | 引数は `-a`・`-s`・ファイルのパス（空白を含めて 1 つ） |
| CMD-004 | `java -jar "{exe}" {file}` | 組み立てる | プログラムは `java`、引数は `-jar`・実行ファイル・ファイル |
| CMD-005 | 空のテンプレート | 組み立てる | プログラムが空 |
