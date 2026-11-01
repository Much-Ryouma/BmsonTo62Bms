bmsonto62bms
============

bmson の譜面を、62進数に対応した拡張 BMS（#BASE 62）に変換するツールです。
各ノーツが鳴らす音声の区間を、1 つずつ別の音声ファイル（WAV / OGG）に切り出して書き出します。
対応するキーモードは beat-7k・beat-14k（.bme）と popn-9k（.pms）です。

■ 動作環境
  Windows 10 / 11（64bit）

■ 使い方
  1. この zip をすべて展開します（日本語を含むフォルダに置いてもかまいません）
  2. bmsonto62bms.exe をダブルクリックします
  3. 「開く…」で bmson を選ぶか、ウィンドウに bmson をドロップします
  4. 左の「書き出しの設定」を決め、右の一覧で試聴して確かめます
  5. 「書き出す」で、bmson の横に BMS（.bme / .pms）と音声ファイルができます
  6. 「設定 → 外部プレイヤー…」でプレイヤーを設定すると、「外部プレイヤーで開く」（F5）で確かめられます

  同じ曲の複数の難易度は、bmson をまとめて選ぶ（またはドロップする）と「譜面セット」になります。
  基準の譜面（ふつうは最も難しい譜面）を選ぶと、音声ファイルを共有した BMS を難易度ごとに書き出します。
  譜面どうしで切り出し位置がずれているときは、短いほうの切り方に合わせます（続きは BGM で鳴らします）。
  それでも基準の譜面にない音を使う譜面があるときは、
  「基準の譜面にないスライスを追加で切り出す」をチェックすると書き出せます。

  詳しい説明: https://github.com/Much-Ryouma/bmsonto62bms

  インストーラーはありません。レジストリにも何も書きません。
  設定は exe の横の bmsonto62bms.ini に保存します。消すときはフォルダごと削除してください。
  ※ Program Files など、書き込めないフォルダに置くと設定を保存できません。

■ 62進数について
  定義（音声・画像・BPM・STOP）がどれも 1295 個以内なら、従来の 36進数の BMS として書き出します。
  1296 個以上になると #BASE 62 を付けた 62進数の BMS になります。
  62進数の BMS は beatoraja 0.8.7 以降など、対応したプレイヤーでだけ正しく鳴ります。

■ 同梱物
  bmsonto62bms.exe        アプリ本体
  bmsonto62bms.ini        設定（初めて設定を変えたときに作られます。消すと初期状態に戻ります）
  bmsonto62bms-cli.exe    コマンドラインツール（bmsonto62bms-cli --help で使い方が出ます）
  Qt6*.dll, platforms\ など   Qt のライブラリ（動作に必要です）
  msvcp140.dll など        Visual C++ ランタイム
  LICENSE.txt             bmsonto62bms のライセンス（MIT License）
  THIRD_PARTY_NOTICES.txt 同梱しているソフトウェアのライセンス
  licenses\               Qt のライセンス（LGPLv3）の全文

■ コマンドラインツールの例（コマンドプロンプトで）
  bmsonto62bms-cli slices 譜面.bmson
  bmsonto62bms-cli convert 譜面.bmson
  bmsonto62bms-cli convert 譜面.bmson --format ogg --edge fade --fade-out 3
  bmsonto62bms-cli convert normal.bmson hyper.bmson another.bmson --allow-extra

■ うまく動かないとき
  - 「VCRUNTIME140.dll が見つからない」と出る: zip の中身をすべて同じフォルダに展開したか確認してください
  - 外部プレイヤーが起動しない: 「設定 → 外部プレイヤー…」で実行ファイルの場所を確認してください
  - 不具合の報告: https://github.com/Much-Ryouma/bmsonto62bms/issues

■ ライセンス
  MIT License（LICENSE.txt）。同梱の Qt などのライセンスは THIRD_PARTY_NOTICES.txt を見てください。
