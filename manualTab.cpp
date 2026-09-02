#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
    struct ManualTopic
    {
        QString id;
        QString category;
        QString title;
        QString summary;
        QString keywords;
        QString body;
    };

    constexpr int TopicIdRole = Qt::UserRole;
    constexpr int TopicHtmlRole = Qt::UserRole + 1;
    constexpr int TopicSearchRole = Qt::UserRole + 2;

    // マニュアル本文からメイン画面の各タブへ移動するリンクを作る
    QString tabLink(int tabIndex, const QString &label)
    {
        return QString("<a href=\"timetable://tab/%1\">%2を開く</a>")
            .arg(tabIndex)
            .arg(label.toHtmlEscaped());
    }

    // マニュアル内の別項目へ移動するリンクを作る
    QString topicLink(const QString &topicId, const QString &label)
    {
        return QString("<a href=\"timetable://topic/%1\">%2</a>")
            .arg(topicId.toHtmlEscaped(), label.toHtmlEscaped());
    }

    // 設定画面の指定タブを直接開くリンクを作る
    QString settingsLink(int tabIndex, const QString &label)
    {
        return QString("<a href=\"timetable://settings/%1\">%2を開く</a>")
            .arg(tabIndex)
            .arg(label.toHtmlEscaped());
    }

    // 軽量な組み込みマニュアルの各項目を作る
    QVector<ManualTopic> manualTopics()
    {
        const QString scheduleLink = tabLink(0, "時間割タブ");
        const QString studentLink = tabLink(1, "生徒一覧タブ");
        const QString teacherLink = tabLink(2, "講師一覧タブ");
        const QString exportLink = tabLink(3, "出力タブ");
        const QString guidancePdfLink = tabLink(4, "指導報告書タブ");

        return {
            {
                "teacher-flow",
                "講師向け",
                "授業日の基本フロー",
                "授業前の確認から、授業メモの保存、指導報告書までの基本手順です。",
                "講師 はじめに 初めて 授業前 授業後 当日 流れ 閲覧モード メモ 保存",
                QStringLiteral(R"HTML(
                    <div class="lead">普段の授業では、時間割を<strong>閲覧モードのまま</strong>使えば大丈夫です。生徒や教科を誤って変えずに、授業メモだけを記録できます。</div>
                    <h2>授業前</h2>
                    <ol class="steps">
                      <li><strong>時間割を開く</strong><br>「今週へ」、または「前の週へ」「次の週へ」で対象週を表示します。</li>
                      <li><strong>自分の授業を確認する</strong><br>曜日、時限、講師名を見て、担当するセルを選びます。右側に生徒名・学年・教科・前回までのメモが表示されます。</li>
                      <li><strong>必要なら予定を印刷する</strong><br>「出力」→「講師の予定印刷」から、講師と日付を選びます。</li>
                    </ol>
                    <h2>授業後</h2>
                    <ol class="steps" start="4">
                      <li><strong>授業メモを入力する</strong><br>対象セルを選び、右側の「メモ」へ進度・宿題・注意点などを入力します。</li>
                      <li><strong>メモを反映して保存する</strong><br>「メモを反映」→「この時間割を保存」の順に押します。セルを移動したときもメモは表へ反映されますが、最後に保存ボタンを押すと確実です。</li>
                      <li><strong>必要なら指導報告書を出す</strong><br>対象セルを選んで「指導報告書」を押すとプレビューできます。セルをダブルクリックすると、印刷ダイアログを直接開きます。</li>
                    </ol>
                    <div class="note"><strong>迷ったら：</strong>講師が通常使うのは「時間割」「メモを反映」「この時間割を保存」です。講師列・生徒名・教科を変える必要がある場合は管理者へ確認してください。</div>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(scheduleLink, topicLink("teacher-memo", "メモ入力を詳しく見る"))
            },
            {
                "teacher-memo",
                "講師向け",
                "時間割の見方と授業メモ",
                "週の移動、セルの選択、閲覧モードで安全にメモを残す方法です。",
                "時間割 見方 セル 曜日 時限 講師 閲覧 編集 メモ 反映 保存 週",
                QStringLiteral(R"HTML(
                    <h2>時間割の見方</h2>
                    <ul>
                      <li>横方向：日付・曜日と講師</li>
                      <li>縦方向：時限。同じ時限の複数行は、そのコマで担当する生徒枠です。</li>
                      <li>セルを選ぶと、右側に登録内容と授業メモが表示されます。</li>
                    </ul>
                    <h2>閲覧モードでできること</h2>
                    <p>青い「閲覧モード」表示のままでも、授業メモは入力できます。「メモを反映」を押すと、選択中セルのメモだけが更新されます。生徒名・教科・講師名などは変更されません。</p>
                    <h2>保存までの安全な手順</h2>
                    <ol class="steps">
                      <li>対象セルが合っていることを確認する</li>
                      <li>メモを入力して「メモを反映」を押す</li>
                      <li>「この時間割を保存」を押す</li>
                      <li>画面下部に「○年○月○日の週を保存しました」と出れば完了</li>
                    </ol>
                    <div class="warning"><strong>注意：</strong>「メモを反映」は画面上の時間割へ反映する操作です。ファイルへ確実に残すには「この時間割を保存」まで行ってください。</div>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(scheduleLink, topicLink("qa-edit-save", "編集・保存のQ&A"))
            },
            {
                "teacher-output",
                "講師向け",
                "予定表と指導報告書",
                "講師予定の印刷、選択授業の指導報告書、スキャン済みPDFの整理方法です。",
                "講師予定 印刷 指導報告書 プレビュー PDF スキャン 分割 名前変更",
                QStringLiteral(R"HTML(
                    <h2>自分の予定を印刷する</h2>
                    <ol class="steps">
                      <li>「出力」タブで「講師の予定印刷」を押します。</li>
                      <li>講師名と対象日を選び、印刷プレビューを確認します。</li>
                      <li>内容が合っていれば、プレビュー画面から印刷します。</li>
                    </ol>
                    <h2>1授業分の指導報告書を出す</h2>
                    <p>「時間割」タブで対象セルを選び、「指導報告書」を押します。生徒・教科・教材を確認して印刷できます。セルのダブルクリックは印刷ダイアログへの近道です。</p>
                    <h2>記入済みPDFを生徒別に整理する</h2>
                    <p>「指導報告書」タブは、スキャン済みの複数ページPDFへ生徒名・教科を対応付け、1ページずつ分割して名前を付ける管理作業用です。通常の授業メモ入力だけなら、この操作は不要です。</p>
                    <p class="actions">%1　%2　%3</p>
                )HTML")
                    .arg(exportLink, scheduleLink, guidancePdfLink)
            },
            {
                "admin-start",
                "管理者向け",
                "初回設定とおすすめの順番",
                "曜日・時限・候補一覧・保存先を整えてから使い始める手順です。",
                "管理者 初回 設定 曜日 時限 最大人数 学年 性別 教科 保存先 給与",
                QStringLiteral(R"HTML(
                    <div class="lead">最初は、基本データ → 講師 → 生徒 → 時間割の順に登録すると、各画面の選択肢が自然につながります。</div>
                    <ol class="steps">
                      <li><strong>基本データを確認</strong><br>「管理」→「設定...」の「基本データ」で、曜日・時限・1コマ最大生徒数・学年・性別・教科を確認します。</li>
                      <li><strong>出力先と表示を確認</strong><br>「時間割」「予定表・印刷」「指導報告書」「給与」の各設定を必要に応じて調整します。</li>
                      <li><strong>講師を登録</strong><br>講師名、給与単価、交通費、メモを登録します。</li>
                      <li><strong>生徒を登録</strong><br>学年、氏名、性別、学校、教科と教材、メモを登録します。</li>
                      <li><strong>時間割を作成</strong><br>編集モードへ切り替え、曜日ごとの講師列と各授業を登録して保存します。</li>
                    </ol>
                    <div class="warning"><strong>構成変更の注意：</strong>曜日・時限・最大生徒数の変更は、現在開いている時間割の構成へ影響します。運用開始後は、対象週と内容を確認してから変更してください。</div>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(settingsLink(0, "基本データ設定"), topicLink("admin-master", "登録作業を詳しく見る"))
            },
            {
                "admin-master",
                "管理者向け",
                "生徒・講師・学校の登録",
                "マスターデータを追加・変更・削除するときの要点です。",
                "生徒 講師 学校 登録 追加 変更 削除 教科 教材 給与 交通費 メモ",
                QStringLiteral(R"HTML(
                    <h2>講師を登録する</h2>
                    <ol class="steps">
                      <li>「講師一覧」タブで講師名を入力します。</li>
                      <li>1:2コマ給、1:1コマ給、高校生手当、一日の交通費、必要なメモを入力します。</li>
                      <li>「追加・変更」を押します。既存講師を直すときは、先に左の一覧から選択します。</li>
                    </ol>
                    <h2>生徒を登録する</h2>
                    <ol class="steps">
                      <li>「生徒一覧」タブで氏名・学年・性別・学校を選びます。</li>
                      <li>「教科と教材」を <strong>英語: 教材A, 教材B</strong> の形式で入力します。教科ごとに改行します。</li>
                      <li>必要なメモを入力して「追加・変更」を押します。</li>
                    </ol>
                    <h2>学校名を増減する</h2>
                    <p>上部メニューの「管理」→「学校の追加」または「学校の削除」を使います。</p>
                    <div class="warning"><strong>削除前に確認：</strong>生徒や講師を削除しても、すでに保存済みの時間割にある文字列が自動で消えるわけではありません。関連する週の時間割も確認してください。</div>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(teacherLink, studentLink)
            },
            {
                "admin-schedule",
                "管理者向け",
                "時間割の作成・編集・週コピー",
                "編集モード、講師列、授業セル、週の切り替えとコピーを詳しく説明します。",
                "時間割 作成 編集モード 講師列 授業 セル コピー 貼り付け 週 来週 保存 Undo Redo",
                QStringLiteral(R"HTML(
                    <h2>新しい週を作る</h2>
                    <ol class="steps">
                      <li>「時間割」タブで対象週へ移動します。保存済みファイルがない週は空の時間割として開きます。</li>
                      <li>青い「閲覧モード」を押し、確認後に編集モードへ切り替えます。</li>
                      <li>曜日内のセルを選び、「講師を追加」で列を作ります。講師を選んで「講師の名前を変更」で列名を設定します。</li>
                      <li>授業セルを選び、学年 → 生徒名 → 教科の順に選択し、「表に反映」を押します。</li>
                      <li>必要に応じて「このコマ最大」を調整し、最後に「この時間割を保存」を押します。</li>
                    </ol>
                    <h2>繰り返し入力を減らす</h2>
                    <ul>
                      <li>セル単位：コピー、ペースト、切り取りを使用できます。</li>
                      <li>週単位：「この週を来週にコピー」または「時間割を選択してこの週にコピー」を使います。</li>
                      <li>誤操作：編集中は「一つ戻る」「やり直す」を使えます。</li>
                    </ul>
                    <div class="warning"><strong>コピー先の日付を必ず確認：</strong>週コピーは既存の保存内容へ影響することがあります。確認ダイアログに表示される元週・コピー先週を読んでから実行してください。</div>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(scheduleLink, topicLink("qa-edit-save", "編集・保存のQ&A"))
            },
            {
                "admin-output",
                "管理者向け",
                "印刷・PDF・予定表・給与",
                "出力タブにある各機能の使い分けと、事前確認のポイントです。",
                "出力 印刷 PDF 時間割 生徒予定 講師予定 給与明細 指導報告書 クリップボード",
                QStringLiteral(R"HTML(
                    <table>
                      <tr><th>機能</th><th>用途</th><th>確認ポイント</th></tr>
                      <tr><td>時間割印刷</td><td>開いている週全体を印刷プレビュー</td><td>対象週と未保存変更</td></tr>
                      <tr><td>時間割表PDF出力</td><td>開いている週全体をPDF保存</td><td>ファイル名と保存先</td></tr>
                      <tr><td>講師の予定印刷</td><td>講師1人・1日分の予定</td><td>講師名と日付</td></tr>
                      <tr><td>生徒予定を出力</td><td>予定文を表示しクリップボードへコピー</td><td>対象生徒・教科・期間</td></tr>
                      <tr><td>給与明細書</td><td>講師・対象月を選び、明細を印刷またはPDF保存</td><td>講師の単価、日別支給、控除</td></tr>
                      <tr><td>指導報告書を印刷</td><td>生徒・教科・教材入りの白紙様式</td><td>生徒と教材</td></tr>
                    </table>
                    <h2>出力前の基本確認</h2>
                    <ol class="steps">
                      <li>時間割タブで正しい週を開く</li>
                      <li>変更中なら時間割を保存する</li>
                      <li>プレビューで日付・氏名・改ページを確認する</li>
                      <li>PDFの場合は保存先と作成されたファイルを確認する</li>
                    </ol>
                    <p>見た目や保存先は「管理」→「設定...」の「予定表・印刷」「指導報告書」「給与」から調整できます。</p>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(exportLink, settingsLink(2, "予定表・印刷設定"))
            },
            {
                "admin-guidance-pdf",
                "管理者向け",
                "スキャン済み指導報告書PDFの整理",
                "複数ページPDFを授業順に確認し、生徒別ファイルへ分割保存します。",
                "指導報告書 PDF スキャン 最新 ファイル 教師 日付 自動入力 分割 名前変更 qpdf",
                QStringLiteral(R"HTML(
                    <h2>事前設定</h2>
                    <p>「管理」→「設定...」→「指導報告書」で、分割前PDFの検索先と分割後PDFの保存先を設定します。</p>
                    <h2>操作手順</h2>
                    <ol class="steps">
                      <li>「指導報告書」タブで授業日を選びます。</li>
                      <li>「当日の教師」から担当講師を選びます。時間割をもとに生徒名・教科の候補が作られます。</li>
                      <li>「最新のファイル」または「ファイルを選ぶ」でスキャン済みPDFを開きます。</li>
                      <li>プレビューと名前・教科を確認します。候補がずれている場合は ◀ ▶ で候補を切り替えるか、直接入力します。</li>
                      <li>「次のページ」で全ページを確認します。最終ページの「分割して名前を変更」で保存します。</li>
                    </ol>
                    <div class="note">保存名は、生徒名・教科・日付を組み合わせて作られます。同名ファイルがある場合は連番が付きます。</div>
                    <div class="warning"><strong>作業途中の切り替えに注意：</strong>日付や講師を変える前に、現在のページ割り当てが不要になってもよいか確認してください。</div>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(guidancePdfLink, settingsLink(3, "指導報告書設定"))
            },
            {
                "admin-data",
                "管理者向け",
                "データの場所・保存・バックアップ",
                "利用者データの種類と、安全にバックアップするときの考え方です。",
                "データ ファイル 場所 保存 バックアップ 復元 data schedules schedulePDF JSON schedule",
                QStringLiteral(R"HTML(
                    <h2>主な保存場所</h2>
                    <table>
                      <tr><th>場所</th><th>内容</th></tr>
                      <tr><td><code>data</code></td><td>生徒、講師、学校、設定、画面状態など</td></tr>
                      <tr><td><code>schedules</code></td><td>週ごとの時間割（<code>.schedule</code>）</td></tr>
                      <tr><td><code>schedulePDF</code></td><td>既定設定での時間割・給与明細PDF</td></tr>
                    </table>
                    <p>相対パスのフォルダは、基本的に <code>TimeTable.exe</code> がある場所を基準に作られます。設定で絶対パスへ変更している場合は、その保存先も確認してください。</p>
                    <h2>バックアップ手順</h2>
                    <ol class="steps">
                      <li>入力中の内容を保存する</li>
                      <li>TimeTableを終了する</li>
                      <li><code>data</code> と <code>schedules</code> を、日付を付けた別フォルダへコピーする</li>
                      <li>必要なら出力PDFの保存先もコピーする</li>
                    </ol>
                    <div class="warning"><strong>直接編集しない：</strong>JSONや <code>.schedule</code> をテキストエディターで直接書き換えると読み込めなくなることがあります。復元するときは元データを別名で残し、アプリを閉じた状態で行ってください。</div>
                    <p class="actions">%1</p>
                )HTML")
                    .arg(topicLink("qa-data", "データ・復旧のQ&A"))
            },
            {
                "qa-edit-save",
                "困ったとき（Q&A）",
                "編集・保存で困ったとき",
                "編集できない、メモが残らない、週を変えたいときの確認項目です。",
                "Q&A 編集できない 保存できない メモ 消えた 閲覧モード 週 切り替え 戻る",
                QStringLiteral(R"HTML(
                    <h2>Q. 生徒名や教科、講師を変更できません</h2>
                    <p>時間割が閲覧モードになっています。時間割タブの青い「閲覧モード」ボタンを押し、確認後に編集モードへ切り替えてください。</p>
                    <h2>Q. 閲覧モードなのにメモ欄へ入力できます</h2>
                    <p>正常です。閲覧モードは、生徒・教科・講師などを保護しながら授業メモだけを更新できるモードです。「メモを反映」後に「この時間割を保存」を押してください。</p>
                    <h2>Q. 入力したメモが残っていません</h2>
                    <p>別セルへ移る前の反映、またはファイル保存が完了していない可能性があります。対象セルを選び直し、「メモを反映」→「この時間割を保存」を行い、画面下部の完了表示を確認してください。</p>
                    <h2>Q. 週を変えると確認が表示されます</h2>
                    <p>未保存の変更や、元に戻す・やり直し履歴があるためです。内容を残す場合は保存を選びます。判断できない場合はキャンセルして、現在の週を確認してください。</p>
                    <h2>Q. 間違えて編集しました</h2>
                    <p>同じ週を開いたままなら「一つ戻る」を試します。保存前で、変更をすべて破棄してよい場合は、いったん時間割を開き直す方法もあります。破棄の確認内容をよく読んでください。</p>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(scheduleLink, topicLink("admin-schedule", "時間割編集を詳しく見る"))
            },
            {
                "qa-output",
                "困ったとき（Q&A）",
                "印刷・PDFで困ったとき",
                "予定が出ない、PDFが見つからない、指導報告書を分割できない場合の確認項目です。",
                "Q&A 印刷できない PDF ない 見つからない 予定 0件 プリンター qpdf 100ページ",
                QStringLiteral(R"HTML(
                    <h2>Q. 生徒・講師の予定が出ません</h2>
                    <p>正しい週・日付を選んでいるか、時間割セルに生徒名・教科・講師名が登録されているか確認します。期間指定の場合は開始日と終了日も確認してください。</p>
                    <h2>Q. 時間割PDFの保存先が分かりません</h2>
                    <p>保存時に表示されるファイル選択画面の場所を確認してください。初期フォルダは「管理」→「設定...」→「予定表・印刷」の「保存先フォルダ」で変更できます。</p>
                    <h2>Q. 「最新のファイル」でPDFが見つかりません</h2>
                    <p>「指導報告書」設定の「分割前PDFの検索先」が、スキャンデータの保存先と一致しているか確認します。見つからない場合は「ファイルを選ぶ」から直接指定できます。</p>
                    <h2>Q. 指導報告書PDFを分割できません</h2>
                    <p>名前・教科の空欄、保存先フォルダへの書き込み権限、配布物内の <code>qpdf12.3.2</code> を確認してください。100ページ以上のPDFには対応していないため、先に小分けにします。</p>
                    <h2>Q. 印刷結果が詰まる・文字が小さいです</h2>
                    <p>「予定表・印刷」設定で、表全体のサイズ・文字サイズ・1ページのコマ数などを調整し、必ずプレビューで確認します。</p>
                    <p class="actions">%1　%2</p>
                )HTML")
                    .arg(exportLink, settingsLink(2, "予定表・印刷設定"))
            },
            {
                "qa-data",
                "困ったとき（Q&A）",
                "データ・復旧で困ったとき",
                "ファイルが見つからない、読み込めない、別PCへ移したい場合の確認項目です。",
                "Q&A データ 消えた 読み込み エラー 破損 復元 移行 別PC data schedules",
                QStringLiteral(R"HTML(
                    <h2>Q. 前に作った時間割が見つかりません</h2>
                    <p>「前の週へ」「次の週へ」で対象週へ移動するか、「時間割表を開く」で <code>schedules</code> 内の対象日ファイルを選びます。ファイル名の日付は、その週の月曜日です。</p>
                    <h2>Q. 読み込みエラーが表示されます</h2>
                    <p>同じファイルへの上書きや直接編集は避け、まず問題のファイルを別名でコピーして保全してください。その後、直近のバックアップから復元します。</p>
                    <h2>Q. 別のPCへ移したいです</h2>
                    <p>TimeTableを終了してから、アプリ本体と一緒に <code>data</code>・<code>schedules</code> をコピーします。設定でアプリ外のPDF保存先を使っている場合は、そのフォルダも別途移します。</p>
                    <h2>Q. 生徒や講師を登録したのに候補へ出ません</h2>
                    <p>「追加・変更」が完了しているか確認します。時間割側では、選択セルをいったん切り替えて候補を読み直してください。学年を先に選ぶと、その学年の生徒が表示されます。</p>
                    <div class="warning"><strong>復旧時の原則：</strong>原因が分からないまま元ファイルへ上書きしないでください。現在のファイルとバックアップの両方を残した状態で確認します。</div>
                    <p class="actions">%1</p>
                )HTML")
                    .arg(topicLink("admin-data", "保存場所とバックアップを見る"))
            },
            {
                "quick-reference",
                "困ったとき（Q&A）",
                "画面とショートカット早見表",
                "やりたいことから使う画面を逆引きできます。",
                "早見表 ショートカット Ctrl O P L S E C V X Z Y Delete",
                QStringLiteral(R"HTML(
                    <h2>やりたいことから探す</h2>
                    <table>
                      <tr><th>やりたいこと</th><th>画面</th></tr>
                      <tr><td>授業の確認・メモ</td><td>時間割</td></tr>
                      <tr><td>生徒・教材の登録</td><td>生徒一覧</td></tr>
                      <tr><td>講師・給与単価の登録</td><td>講師一覧</td></tr>
                      <tr><td>予定表・給与・白紙報告書の印刷</td><td>出力</td></tr>
                      <tr><td>スキャン済み報告書PDFの分割</td><td>指導報告書</td></tr>
                      <tr><td>曜日・表示・保存先などの変更</td><td>管理 → 設定...</td></tr>
                    </table>
                    <h2>主なショートカット</h2>
                    <table>
                      <tr><th>キー</th><th>操作</th></tr>
                      <tr><td><kbd>Ctrl+S</kbd></td><td>時間割を保存</td></tr>
                      <tr><td><kbd>Ctrl+O</kbd></td><td>時間割表を開く</td></tr>
                      <tr><td><kbd>Ctrl+P</kbd></td><td>時間割表を印刷</td></tr>
                      <tr><td><kbd>Ctrl+L</kbd></td><td>時間割表をPDF出力</td></tr>
                      <tr><td><kbd>Ctrl+E</kbd></td><td>設定を開く</td></tr>
                      <tr><td><kbd>Ctrl+C / V / X</kbd></td><td>セルをコピー／ペースト／切り取り</td></tr>
                      <tr><td><kbd>Delete</kbd></td><td>選択セルを空にする</td></tr>
                      <tr><td><kbd>Ctrl+Z / Y</kbd></td><td>一つ戻る／やり直す</td></tr>
                    </table>
                    <p class="actions">%1　%2　%3　%4　%5</p>
                )HTML")
                    .arg(scheduleLink, studentLink, teacherLink, exportLink, guidancePdfLink)
            }
        };
    }

    // アプリの配色へ合わせた読みやすいHTML文書を作る
    QString manualDocument(const ManualTopic &topic, const QPalette &palette)
    {
        QString document = QStringLiteral(R"HTML(
            <!doctype html>
            <html><head><meta charset="utf-8"><style>
              body { color: {{TEXT}}; background: {{BASE}}; font-family: sans-serif; font-size: 14px; line-height: 1.65; margin: 24px 30px 42px; }
              .role { color: {{MUTED}}; font-size: 12px; font-weight: 700; letter-spacing: 0.08em; margin-bottom: 4px; }
              h1 { font-size: 25px; line-height: 1.3; margin: 0 0 8px; }
              .summary { color: {{MUTED}}; font-size: 14px; margin: 0 0 24px; }
              h2 { border-bottom: 1px solid {{BORDER}}; font-size: 18px; margin: 28px 0 12px; padding-bottom: 6px; }
              p { margin: 8px 0 12px; }
              ul, ol { margin-top: 8px; padding-left: 25px; }
              li { margin-bottom: 8px; }
              .steps li { margin-bottom: 14px; padding-left: 4px; }
              .lead, .note, .warning { background: {{ALT}}; border: 1px solid {{BORDER}}; border-radius: 5px; margin: 14px 0 20px; padding: 13px 15px; }
              .lead { border-left: 4px solid {{ACCENT}}; font-size: 15px; }
              .warning { border-left: 4px solid {{WARNING}}; }
              .actions { border-top: 1px solid {{BORDER}}; margin-top: 30px; padding-top: 14px; }
              a { color: {{LINK}}; font-weight: 600; text-decoration: none; }
              table { border-collapse: collapse; margin: 12px 0 22px; width: 100%; }
              th, td { border: 1px solid {{BORDER}}; padding: 8px 10px; text-align: left; vertical-align: top; }
              th { background: {{ALT}}; }
              code, kbd { background: {{ALT}}; border: 1px solid {{BORDER}}; border-radius: 3px; font-family: monospace; padding: 1px 5px; }
            </style></head><body>
              <div class="role">{{CATEGORY}}</div>
              <h1>{{TITLE}}</h1>
              <p class="summary">{{SUMMARY}}</p>
              {{BODY}}
            </body></html>
        )HTML");

        const QColor textColor = palette.color(QPalette::Text);
        const QColor baseColor = palette.color(QPalette::Base);
        const QColor alternateColor = palette.color(QPalette::AlternateBase);
        const QColor borderColor = palette.color(QPalette::Mid);
        const QColor mutedColor = palette.color(QPalette::PlaceholderText);
        const QColor accentColor = palette.color(QPalette::Highlight);
        const QColor linkColor = palette.color(QPalette::Link);

        document.replace("{{TEXT}}", textColor.name());
        document.replace("{{BASE}}", baseColor.name());
        document.replace("{{ALT}}", alternateColor.name());
        document.replace("{{BORDER}}", borderColor.name());
        document.replace("{{MUTED}}", mutedColor.name());
        document.replace("{{ACCENT}}", accentColor.name());
        document.replace("{{LINK}}", linkColor.name());
        document.replace("{{WARNING}}", "#d97706");
        document.replace("{{CATEGORY}}", topic.category.toHtmlEscaped());
        document.replace("{{TITLE}}", topic.title.toHtmlEscaped());
        document.replace("{{SUMMARY}}", topic.summary.toHtmlEscaped());
        document.replace("{{BODY}}", topic.body);
        return document;
    }

    // IDに対応するマニュアル項目を探す
    QTreeWidgetItem *findTopicItem(QTreeWidget *tree, const QString &topicId)
    {
        for (int categoryIndex = 0; categoryIndex < tree->topLevelItemCount(); ++categoryIndex)
        {
            QTreeWidgetItem *categoryItem = tree->topLevelItem(categoryIndex);

            for (int topicIndex = 0; topicIndex < categoryItem->childCount(); ++topicIndex)
            {
                QTreeWidgetItem *topicItem = categoryItem->child(topicIndex);

                if (topicItem->data(0, TopicIdRole).toString() == topicId)
                {
                    return topicItem;
                }
            }
        }

        return nullptr;
    }

    // 検索結果内で最初に表示されている項目を返す
    QTreeWidgetItem *firstVisibleTopicItem(QTreeWidget *tree)
    {
        for (int categoryIndex = 0; categoryIndex < tree->topLevelItemCount(); ++categoryIndex)
        {
            QTreeWidgetItem *categoryItem = tree->topLevelItem(categoryIndex);

            if (categoryItem->isHidden())
            {
                continue;
            }

            for (int topicIndex = 0; topicIndex < categoryItem->childCount(); ++topicIndex)
            {
                QTreeWidgetItem *topicItem = categoryItem->child(topicIndex);

                if (!topicItem->isHidden())
                {
                    return topicItem;
                }
            }
        }

        return nullptr;
    }
}

// 講師向け・管理者向け・Q&Aを検索できる操作マニュアルを作成する
void MainWindow::setupManualTab()
{
    auto *manualTab = new QWidget(ui->mainTabWidget);
    manualTab->setObjectName("manualTab");

    auto *mainLayout = new QVBoxLayout(manualTab);
    mainLayout->setContentsMargins(14, 12, 14, 14);
    mainLayout->setSpacing(10);

    auto *titleLabel = new QLabel("操作マニュアル", manualTab);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    auto *descriptionLabel = new QLabel(
        "講師向けの基本操作、管理者向けの詳しい手順、困ったときのQ&Aをまとめています。",
        manualTab);
    descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(descriptionLabel);

    auto *searchLayout = new QHBoxLayout();
    auto *searchEdit = new QLineEdit(manualTab);
    searchEdit->setObjectName("manualSearchEdit");
    searchEdit->setPlaceholderText("操作名や困りごとを検索（例：保存、印刷、PDF）");
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setAccessibleName("マニュアルを検索");
    auto *resultLabel = new QLabel(manualTab);
    resultLabel->setMinimumWidth(70);
    resultLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(resultLabel);
    mainLayout->addLayout(searchLayout);

    auto *splitter = new QSplitter(Qt::Horizontal, manualTab);
    splitter->setChildrenCollapsible(false);

    auto *topicTree = new QTreeWidget(splitter);
    topicTree->setObjectName("manualTopicTree");
    topicTree->setHeaderHidden(true);
    topicTree->setUniformRowHeights(true);
    topicTree->setRootIsDecorated(true);
    topicTree->setIndentation(16);
    topicTree->setMinimumWidth(210);
    topicTree->setMaximumWidth(340);
    topicTree->setSelectionMode(QAbstractItemView::SingleSelection);
    topicTree->setAccessibleName("マニュアル目次");

    auto *browser = new QTextBrowser(splitter);
    browser->setObjectName("manualBrowser");
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);
    browser->setFrameShape(QFrame::StyledPanel);
    browser->setAccessibleName("マニュアル本文");

    splitter->addWidget(topicTree);
    splitter->addWidget(browser);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({245, 760});
    mainLayout->addWidget(splitter, 1);

    const QVector<ManualTopic> topics = manualTopics();
    QHash<QString, QTreeWidgetItem *> categoryItems;

    for (const ManualTopic &topic : topics)
    {
        QTreeWidgetItem *categoryItem = categoryItems.value(topic.category, nullptr);

        if (categoryItem == nullptr)
        {
            categoryItem = new QTreeWidgetItem(topicTree, {topic.category});
            QFont categoryFont = categoryItem->font(0);
            categoryFont.setBold(true);
            categoryItem->setFont(0, categoryFont);
            categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);
            categoryItems.insert(topic.category, categoryItem);
        }

        auto *topicItem = new QTreeWidgetItem(categoryItem, {topic.title});
        topicItem->setToolTip(0, topic.summary);
        topicItem->setData(0, TopicIdRole, topic.id);
        topicItem->setData(0, TopicHtmlRole, manualDocument(topic, palette()));
        topicItem->setData(
            0,
            TopicSearchRole,
            QString("%1 %2 %3 %4 %5")
                .arg(topic.category, topic.title, topic.summary, topic.keywords, topic.body)
                .toCaseFolded());
    }

    topicTree->expandAll();
    resultLabel->setText(QString("全%1件").arg(topics.size()));

    connect(
        topicTree,
        &QTreeWidget::currentItemChanged,
        this,
        [browser](QTreeWidgetItem *current, QTreeWidgetItem *)
        {
            if (current == nullptr)
            {
                return;
            }

            const QString html = current->data(0, TopicHtmlRole).toString();

            if (!html.isEmpty())
            {
                browser->setHtml(html);
            }
        });

    connect(
        searchEdit,
        &QLineEdit::textChanged,
        this,
        [topicTree, browser, resultLabel](const QString &query)
        {
            const QStringList terms = query.simplified().toCaseFolded().split(
                ' ',
                Qt::SkipEmptyParts);
            int visibleTopicCount = 0;

            for (int categoryIndex = 0;
                 categoryIndex < topicTree->topLevelItemCount();
                 ++categoryIndex)
            {
                QTreeWidgetItem *categoryItem = topicTree->topLevelItem(categoryIndex);
                int visibleInCategory = 0;

                for (int topicIndex = 0;
                     topicIndex < categoryItem->childCount();
                     ++topicIndex)
                {
                    QTreeWidgetItem *topicItem = categoryItem->child(topicIndex);
                    const QString searchText =
                        topicItem->data(0, TopicSearchRole).toString();
                    bool matches = true;

                    for (const QString &term : terms)
                    {
                        if (!searchText.contains(term))
                        {
                            matches = false;
                            break;
                        }
                    }

                    topicItem->setHidden(!matches);

                    if (matches)
                    {
                        ++visibleInCategory;
                        ++visibleTopicCount;
                    }
                }

                categoryItem->setHidden(visibleInCategory == 0);
                categoryItem->setExpanded(true);
            }

            resultLabel->setText(
                terms.isEmpty()
                    ? QString("全%1件").arg(visibleTopicCount)
                    : QString("%1件").arg(visibleTopicCount));

            QTreeWidgetItem *currentItem = topicTree->currentItem();

            if (visibleTopicCount == 0)
            {
                browser->setHtml(
                    "<html><body style=\"font-family:sans-serif; margin:28px\">"
                    "<h2>該当する項目がありません</h2>"
                    "<p>言葉を短くするか、「保存」「印刷」「PDF」など別の言葉で検索してください。</p>"
                    "</body></html>");
                return;
            }

            if (currentItem == nullptr || currentItem->isHidden())
            {
                QTreeWidgetItem *firstItem = firstVisibleTopicItem(topicTree);

                if (firstItem != nullptr)
                {
                    topicTree->setCurrentItem(firstItem);
                }

                return;
            }

            const QString currentHtml =
                currentItem->data(0, TopicHtmlRole).toString();

            if (!currentHtml.isEmpty())
            {
                browser->setHtml(currentHtml);
            }
        });

    connect(
        browser,
        &QTextBrowser::anchorClicked,
        this,
        [this, topicTree](const QUrl &url)
        {
            if (url.scheme() != "timetable")
            {
                return;
            }

            const QString value = url.path().mid(1);

            if (url.host() == "topic")
            {
                QTreeWidgetItem *topicItem = findTopicItem(topicTree, value);

                if (topicItem != nullptr)
                {
                    topicTree->setCurrentItem(topicItem);
                    topicTree->scrollToItem(topicItem);
                }

                return;
            }

            bool converted = false;
            const int index = value.toInt(&converted);

            if (!converted)
            {
                return;
            }

            if (url.host() == "tab" &&
                index >= 0 &&
                index < ui->mainTabWidget->count())
            {
                ui->mainTabWidget->setCurrentIndex(index);
                return;
            }

            if (url.host() == "settings")
            {
                showSettingsDialog(index);
            }
        });

    QTreeWidgetItem *defaultItem = findTopicItem(topicTree, "teacher-flow");

    if (defaultItem != nullptr)
    {
        topicTree->setCurrentItem(defaultItem);
    }

    ui->mainTabWidget->addTab(manualTab, "マニュアル");
}
