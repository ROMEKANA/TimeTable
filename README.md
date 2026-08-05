# TimeTable

塾の時間割、生徒・講師情報、予定表、給与明細、指導報告書などを管理・印刷するQtデスクトップアプリです。

## 開発環境

- Windows
- Qt 6.11.0
- MinGW 13.1.0 64-bit
- CMake
- C++17

Qtの主なパスは次の場所を前提にしています。

```text
C:\Qt\6.11.0\mingw_64
C:\Qt\Tools\mingw1310_64
C:\Qt\Tools\CMake_64
```

プロジェクト本体：

```text
C:\Users\TO\Documents\jukuTimeTable\TimeTable
```

## 主なファイル

- `mainwindow.cpp` / `mainwindow.h`：メイン画面、設定、共通処理
- `scheduleTab.cpp`：時間割
- `scheduleData.cpp`：時間割データの変換
- `scheduleStorage.cpp`：時間割の読み書き
- `studentTab.cpp`：生徒
- `teacherTab.cpp`：講師
- `exportTab.cpp`：印刷、PDF、予定表、給与明細、指導報告書
- `undo.cpp`：元に戻す・やり直し
- `updaterMain.cpp`：GitHub Releasesからの自動更新
- `make-release-zip.bat`：配布用ZIPの作成
- `AGENTS.md`：Codex向けの作業ルールと既知の注意点

## バージョン

バージョンは `CMakeLists.txt` の次の形式の行で管理します。
以下の `x.y.z` は説明用の表記なので、実際のバージョン番号へ置き換えます。

```cmake
project(TimeTable VERSION x.y.z LANGUAGES CXX)
```

ファイルを変更するときは、バージョンも一緒に更新します。

コミットメッセージは、先頭に変更後のバージョンを入れます。

```text
vx.y.z 変更内容
```

GitHub Releaseのタグも同じバージョンにします。

```text
vx.y.z
```

## Debugビルド

通常の動作確認では、次のDebugビルドだけを実行します。

```powershell
$env:PATH='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.0\mingw_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build 'C:/Users/TO/Documents/jukuTimeTable/TimeTable/build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug' --target all
```

`cc1plus.exe` が `-1073741515` で終了する場合は、上記の2つのQtパスが `PATH` の先頭に入っているか確認します。

## リリース手順

### 1. 変更内容を確認する

- 必要な修正がすべて入っているか確認する
- `CMakeLists.txt` のバージョンを次の番号へ更新する
- Debugビルドが成功することを確認する
- `git status` で意図しないファイルが含まれていないことを確認する

Gitのコミット、タグ、プッシュはユーザーが行います。

### 2. Releaseビルド環境を確認する

Qt Creatorで次のReleaseビルドフォルダが作成済みである必要があります。

```text
build\Desktop_Qt_6_11_0_MinGW_64_bit-Release
```

Release構成がない場合は、Qt Creatorで `Desktop Qt 6.11.0 MinGW 64-bit` のRelease構成を作って、一度ビルドします。

### 3. 配布用ZIPを作る

プロジェクト内の次のファイルをダブルクリックします。

```text
make-release-zip.bat
```

このバッチは次の処理を行います。

1. `TimeTable.exe` と `TimeTableUpdater.exe` をReleaseビルド
2. `windeployqt` で必要なQt DLLとプラグインを配置
3. ビルド用ファイルや利用者データ用フォルダを除外
4. `releases` フォルダへ配布用ZIPを作成

作成されるファイル名：

```text
releases\TimeTable-vx.y.z-win64.zip
```

ZIP名のバージョンは `CMakeLists.txt` から自動取得されます。

### 4. ZIPを確認する

作成したZIPを別の一時フォルダへ展開し、少なくとも次を確認します。

- `TimeTable.exe` が起動する
- `TimeTableUpdater.exe` が入っている
- QtのDLLと `platforms` などの必要なフォルダが入っている
- 開発用ファイルや既存の `data`、`schedules` が入っていない

`data`、`schedules`、`schedulePDF` は利用者の既存データを配布物で上書きしないため、ZIPから除外されます。

### 5. Gitへ反映する

次の内容をユーザーが実行します。

1. 変更をコミット
2. `master` をGitHubへプッシュ
3. バージョンと同じタグを作成
4. タグをGitHubへプッシュ

例：

```text
コミット：vx.y.z 変更内容
タグ：vx.y.z
```

### 6. GitHub Releaseを公開する

GitHubの `ROMEKANA/TimeTable` リポジトリで新しいReleaseを作成します。

- タグ：`vx.y.z`
- Releaseタイトル：`vx.y.z`
- 本文：主な変更内容と注意点
- 添付ファイル：`TimeTable-vx.y.z-win64.zip`

更新アプリは次のGitHub Releases APIから最新版を確認します。

```text
https://api.github.com/repos/ROMEKANA/TimeTable/releases/latest
```

Releaseを下書きのままにせず、公開済みにします。公開後、更新アプリから新しいバージョンとZIPが見つかることを確認します。

## リリース時のチェックリスト

- [ ] `CMakeLists.txt` のバージョンを更新した
- [ ] コミットメッセージの先頭にバージョンを入れた
- [ ] Debugビルドが成功した
- [ ] Releaseビルドが成功した
- [ ] `make-release-zip.bat` が正常終了した
- [ ] ZIPを別フォルダへ展開して起動確認した
- [ ] ZIPに利用者データが含まれていない
- [ ] コミットとタグをGitHubへプッシュした
- [ ] GitHub Releaseへ正しいZIPを添付した
- [ ] GitHub Releaseを公開した
- [ ] 更新アプリから最新版を確認できた

## 注意

- `build`、`releases`、実行ファイル、DLL、ZIPはGit管理対象外です。
- `make-release-zip.bat` にはPC固有の絶対パスが設定されています。Qtやプロジェクトの場所を変えた場合は、バッチ先頭のパスも変更します。
- 通常のコード修正確認では、配布用ZIPを作成しません。リリースするときだけバッチを実行します。
- 既知の問題や完成前の確認事項は `AGENTS.md` に記録されています。
