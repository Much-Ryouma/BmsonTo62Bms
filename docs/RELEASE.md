# リリース手順

新しいバージョン（例：`0.1.2`）を GitHub で公開するまでの作業です。
zip の作り方そのものは [README の「配布用の zip を作る」](../README.md#配布用の-zip-を作る) を参照してください。

## 最初のリリース（v0.1.0）の前に一度だけすること

1. GitHub で空のリポジトリ `bmsonto62bms` を作る（README や LICENSE は付けない。手元にあるため）
2. 手元のフォルダを git のリポジトリにして、最初のコミットを push する

```bat
cd D:\ClaudeCode_workspace\bmsonto62bms
git init -b main
git config commit.template .gitmessage
git add -A
git status
git commit
git remote add origin https://github.com/Much-Ryouma/bmsonto62bms.git
git push -u origin main
```

3. `.github/workflows/` に `ci.yml` と `release.yml` を置いてコミットし、push する
   - push すると Actions タブで「CI」（Windows でのビルドとテスト）が動く。緑になることを確かめる
4. `CMakeLists.txt` の `VERSION` はすでに `0.1.0` なので、「1. バージョン番号を上げる」は飛ばして「4. タグを付けて push する」から進める

## 0. 流れ

1. バージョン番号を上げる
2. 手元でビルド・テスト・動作確認
3. コミットして push
4. タグを付けて push（ここで GitHub Actions が zip を作る）
5. GitHub で下書きのリリースを確認して公開

## 1. バージョン番号を上げる

`CMakeLists.txt` の `project(...)` にある `VERSION` を書き換えます。

```cmake
project(bmsonto62bms
    VERSION 0.1.2
```

- タグ（`v0.1.2`）とこの値が一致しないと、リリース用のワークフローが失敗します。
- 「バージョン情報」画面や zip のファイル名も、この値から作られます。

番号の目安：

| 変更の内容 | 上げる位置 | 例 |
|---|---|---|
| 不具合修正・小さな操作の改善 | 3 桁目 | 0.1.1 → 0.1.2 |
| 機能の追加・操作の大きな変更 | 2 桁目 | 0.1.2 → 0.2.0 |

> **注意**：Qt Creator で `CMakeLists.txt` を開いたままの状態でファイルが外部（同期など）で更新されると、古い内容のまま編集・保存して上書きしてしまうことがあります。
> 「外部で変更されました」と聞かれたら **再読み込み** を選んでから編集してください。
> 上書きしてしまうと `Unknown CMake command "ibs_add_windows_resources"` のようなエラーになります。

## 2. 手元で確認する

1. Qt Creator で「ビルド → CMake を再実行」（バージョン番号を反映させるため）
2. Release 構成でビルドし、テストを実行してすべて成功することを確認
3. 起動して「ヘルプ → バージョン情報」に新しい番号が出ることを確認
4. zip を作り、別のフォルダに展開して起動できることを確認（任意）

## 3. コミットして push する

```bat
cd D:\ClaudeCode_workspace\bmsonto62bms
git status
git diff CMakeLists.txt
git add -A
git commit
git push origin main
```

- `git status` に `appendix/`・`build/`・`.qtcreator/` が含まれていないことを確認します（`appendix/` は絶対にコミットしない）。
- `git diff CMakeLists.txt` で、意図した変更（バージョン番号など）だけになっていることを確認します。
- コミットメッセージは「なぜ変えたか」を 3 行程度で書きます。

## 4. タグを付けて push する

```bat
git tag v0.1.2
git push origin v0.1.2
```

- **タグは `git push origin main` だけでは送られません。** 必ずタグ名を指定して push します。
- push すると GitHub の **Actions** タブで「Release」ワークフローが動きます（ビルド → テスト → バージョン確認 → zip 作成 → 下書きのリリース作成）。

タグを付け間違えた・ワークフローが失敗して付け直したいとき：

```bat
git tag -d v0.1.2
git push origin :refs/tags/v0.1.2
```

修正をコミット・push してから、もう一度タグを付けて push します。
GitHub に失敗時の下書きリリースが残っていたら、Releases 画面から削除しておきます。

## 5. GitHub でリリースを公開する

1. **Actions** タブで「Release」が緑（成功）になるのを待つ
2. **Releases** に `v0.1.2` の下書き（Draft）ができているので編集を開く
3. 添付に `bmsonto62bms-0.1.2-win64.zip` があることを確認
4. リリースノートを書く（下のひな形を参照）
5. 「Publish release」を押して公開

リリースノートのひな形：

```markdown
## bmsonto62bms v0.1.2

（今回の変更を一言で）

### 新機能
- 

### 操作の変更
- （前のバージョンから操作方法が変わったものは太字にして目立たせる）

### 不具合の修正
- 

### 更新方法
新しい zip を展開して使ってください。前のバージョンの設定を引き継ぐ場合は、古いフォルダの `bmsonto62bms.ini` を新しい exe の横にコピーしてください。
```

- 利用者から見て変わったことだけを書きます（内部の作り直しや、未公開の途中版で直した不具合は書かない）。
- 前のリリースからの差分は、GitHub のタグ比較（`.../compare/v0.1.1...v0.1.2`）や `git log v0.1.1..v0.1.2 --oneline` で確認できます。

## チェックリスト

- [ ] `CMakeLists.txt` の `VERSION` を上げた
- [ ] CMake を再実行し、Release でビルド・テストが通った
- [ ] バージョン情報に新しい番号が出た
- [ ] `git status` に `appendix/` などが含まれていない
- [ ] コミットして `main` を push した
- [ ] タグを付けて、タグを push した
- [ ] Actions の「Release」が成功した
- [ ] 下書きリリースに zip が添付されている
- [ ] リリースノートを書いて公開した
- [ ] README の「画面の使い方」が今の画面と合っている
