# プロジェクト固有の作業ルール

- Codexがファイルを変更するたびに、`CMakeLists.txt` のプロジェクトバージョンも更新する。
- 最後の確認では、次のDebugビルドだけを実行する。確認目的で `make-release-zip.bat` は実行しない。
- `TimeTable.exe` の実画面・実操作の確認は基本的にユーザーが行う。CodexはDebugビルドとコード上の確認を行い、ユーザーが確認すべき操作手順や注意点を短く伝える。

```powershell
$env:PATH='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.0\mingw_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build 'C:/Users/TO/Documents/jukuTimeTable/TimeTable/build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug' --target all
```

- `TimeTable.exe` が起動中だと、リンク時に `cannot open output file TimeTable.exe: Permission denied` で失敗する。アプリは勝手に終了せず、起動中であることをユーザーへ伝える。
- リリース方法は `README.md` を参照するようユーザーに伝える。
- 講師の毎日の業務には、授業後の指導報告書のスキャン、PDFの分割・名前付け、生徒への送付までを含む。講師向けマニュアルや業務フローを変更するときは、この一連の作業を省略しない。

## Qt/MinGW環境の注意

- 2026-07-02時点では、Qt/MinGWビルドが診断なしの `FAILED: [code=1]` で停止していた。
- 2026-07-03に、MinGW内部コンパイラが必要DLLを見つけられないことが原因と判明した。`cc1plus.exe --version` が `-1073741515` で落ちる場合は、ビルド前にPATH先頭へ `C:\Qt\Tools\mingw1310_64\bin` と `C:\Qt\6.11.0\mingw_64\bin` を追加する。Qt Creator側のビルド環境にも同じ2パスを追加する。

# タスク管理

- TimeTable.exeのタスク・バグ候補・改善案は [GitHub Issues](https://github.com/ROMEKANA/TimeTable/issues) に集約する。
- 基本は Todo（これから）・In Progress（作業中）・Done（完了）。期限や見積もり、細かな分類は必要になってから追加する。
- 着手時に既存Issueを確認し、関連する小さな指摘は同じIssueのチェックリストにまとめる。古い候補は現行版で再確認する。
- 作業を始めるものを In Progress にし、必要な動作確認が済んだらIssueを閉じて Done にする。
- Notionとこのファイルにはタスク一覧や進捗を重複して記録しない。このファイルには継続的な作業ルールだけを残す。
- 2026-09-09に既存タスクをIssuesへ移行した。移行前の記録は [AGENTS.mdの履歴](https://github.com/ROMEKANA/TimeTable/blob/8b569c031fea73769fd723c7b0ef3af46af1ad65/AGENTS.md) を参照する。
