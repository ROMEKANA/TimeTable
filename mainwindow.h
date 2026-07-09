#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDate>
#include <QMainWindow>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QCloseEvent;
class QColor;
class QEvent;
class QJsonObject;
class QPainter;
class QPrinter;
class QRectF;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

struct LessonData
{
    QString studentName;
    QString studentGrade;
    QString subject;
    QString memo;
    int maxStudents = 0;
};

struct LessonRecord
{
    QDate date;
    QString day;
    QString period;
    QString teacherName;
    QString studentName;
    QString studentGrade;
    QString subject;
    QString memo;
    int dayIndex;
    int teacherIndex;
    int periodIndex;
    int studentIndex;
};

struct TeacherScheduleBlock
{
    QDate date;
    QString day;
    QString period;
    int teacherIndex = -1;
    int periodIndex = -1;
    QVector<LessonRecord> entries;
};

struct CellEditCommand
{
    int row;
    int column;
    LessonData before;
    LessonData after;
};

struct TeacherColumn
{
    QString teacherName;
    QVector<QVector<LessonData>> lessons;
};

struct StudentSubjectData
{
    QString subjectName;
    QStringList materials;
};

struct StudentData
{
    QString Name;
    int Grade;
    int gender;
    QString memo;
    QVector<StudentSubjectData> subjects;
    QString school;
};

struct GradeStudents
{
    QString Grade;
    QVector<StudentData> students;
};

struct TeacherData
{
    QString name;
    QString memo;
    int oneOnTwoRate = 0;
    int oneOnOneRate = 0;
    int transportPay = 0;
    int highSchoolAllowance = 0;
};

struct TeacherDailyPayData
{
    QString teacherName;
    QDate date;
    int lessonCount = 0;
    int highSchoolStudentCount = 0;
    int businessPay = 0;
    int transportPay = 0;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    bool eventFilter(QObject *object, QEvent *event) override; // 入力欄や一覧のキー操作を共通処理するイベントフィルター
    void closeEvent(QCloseEvent *event) override; // 終了時に未保存の時間割を確認して保存する

public:
    explicit MainWindow(QWidget *parent = nullptr); // メイン画面を生成して各タブを初期化する
    explicit MainWindow(const QString &startupScheduleFilePath, QWidget *parent = nullptr); // 指定された時間割ファイルを開いてメイン画面を生成する
    ~MainWindow() override; // UIリソースを解放する

private:
    Ui::MainWindow *ui;

    // master data
    QStringList days = {"月", "火", "水", "木", "金", "土"};
    QStringList periods = {
        "1限目",
        "2限目",
        "3限目"};

    QStringList grades = {
        "小1", "小2", "小3", "小4", "小5", "小6",
        "中1", "中2", "中3", "高1", "高2", "高3", "既卒"};
    QStringList genders = {"男性", "女性", "その他"};
    QStringList subjects = {
        "英語", "数学", "算数", "国語", "理科", "社会"};

    QStringList schools;

    QVector<GradeStudents> allStudents;
    QVector<TeacherData> teachers;

    QVector<QVector<TeacherColumn>> schedule;

    QVector<CellEditCommand> undoStack;
    QVector<CellEditCommand> redoStack;
    QDate startupScheduleMonday;
    QString startupScheduleFilePath;

    int MaxStudentPerTeacher = 2;
    float scrollSpeed = 0.01f;
    int cellSectionSize = 115;
    int scheduleDisplayFontPointSize = 12;
    int scheduleDisplayHeaderFontPointSize = 10;
    int scheduleDisplayCellHeight = 0;
    int scheduleDisplayHeaderHeight = 44;
    int scheduleDisplayTimeHeaderWidth = 64;
    QString scheduleOddRowColor = "#f4f4f4";
    QString scheduleEmptyCellColor = "#4a4a4a";
    QString scheduleOverCapacityCellColor = "#ff6b6b";
    QString scheduleTextColor = "#000000";
    QString scheduleOddRowTextColor = "#000000";
    QString scheduleVerticalLineColor = "#7d7d7d";
    int scheduleVerticalLineWidth = 1;
    QString scheduleHorizontalLineColor = "#7d7d7d";
    int scheduleHorizontalLineWidth = 1;
    QString scheduleVerticalSectionLineColor = "#373737";
    int scheduleVerticalSectionLineWidth = 2;
    QString scheduleHorizontalSectionLineColor = "#373737";
    int scheduleHorizontalSectionLineWidth = 2;

    int schedulePrintDarknessPercent = 115;
    int schedulePrintLineWidthPercent = 100;
    int schedulePrintSizePercent = 96;
    int schedulePrintFontPointSize = 9;
    int schedulePrintHeaderFontPointSize = 11;
    int schedulePrintTimeColumnPadding = 100;
    int schedulePrintDayHeaderHeight = 100;
    int schedulePrintTeacherHeaderHeight = 100;
    int schedulePrintAutoShrinkText = 0;

    int studentHonorificEnabled = 1;
    QString studentHonorificDefaultSuffix = "さん";
    QString studentHonorificSpecialGender = "男性";
    QString studentHonorificSpecialSuffix = "くん";
    int teacherScheduleBlocksPerPage = 5;
    int teacherScheduleOneLessonPerLine = 1;
    int teacherScheduleFontPointSize = 9;
    int teacherScheduleIncludeEmptyStudentSlots = 1;
    int teacherScheduleIncludeEmptySlots = 0;
    QString schedulePdfOutputDir = "schedulePDF";
    int studentSelectionVisibleRowCount = 10;
    
    int guidanceReportTitleFontPointSize = 15;
    int guidanceReportInfoFontPointSize = 18;
    int guidanceReportOuterLineWidth = 3;
    int guidanceReportLineWidth = 1;
    int guidanceReportGridLineWidth = 1;
    int guidanceReportBoldFontPointSize = 9;
    int guidanceReportTextFontPointSize = 9;
    QString guidanceReportTitleColor = "#000000";
    QString guidanceReportInfoColor = "#000000";
    QString guidanceReportOuterLineColor = "#000000";
    QString guidanceReportLineColor = "#000000";
    QString guidanceReportGridLineColor = "#cdcdcd";
    QString guidanceReportBoldTextColor = "#000000";
    QString guidanceReportTextColor = "#000000";

    int defaultSalaryOneOnTwoRate = 2000;
    int defaultSalaryOneOnOneRate = 1000;
    int defaultSalaryTransportPay = 0;
    int defaultSalaryHighSchoolAllowance = 500;

    // General
    QString dataFilePath(QString data); // dataフォルダ内のJSONファイルパスを作る
    void loadMasterData(); // マスターデータと各種設定を読み込む
    void showMasterDataDialog(); // マスターデータ編集ダイアログを表示する
    void showScheduleColorDialog(); // 時間割の表示色設定ダイアログを表示する
    QJsonObject loadMasterJson(); // master.jsonを読み込んでJSONオブジェクトで返す
    QStringList masterListDefaultValues(const QString &key) const; // マスター項目の初期値一覧を返す
    void normalizeMasterJson(QJsonObject *root) const; // master.jsonの不足値や不正値を補正する
    bool saveMasterJson(const QJsonObject &root); // マスターデータをmaster.jsonへ保存する
    void refreshAfterMasterDataChanged(); // マスターデータ変更後に画面と時間割構造を更新する
    void editMasterListValues(const QString &key, const QString &label); // 曜日や学年などの一覧項目を編集する
    void setupActions(); // メニューアクションと処理を接続する
    void loadApplicationState(); // 前回終了時の週やウィンドウ状態を復元する
    bool saveApplicationState(); // 現在の週やウィンドウ状態を保存する

    // schedule Tab
    int selectedRow = -1;
    int selectedColumn = -1;
    bool isLoadingCell = false;

    QDate scheduleMonday;

    void setupScheduleTab(); // schedule tabの初期表示とデータ読み込みを行う
    void scheduleTabConnects(); // schedule tabのボタンや選択変更を接続する

    void renderTable(); // 時間割テーブル全体を再描画する

    void addTeacherColumn(); // 選択中の曜日に講師列を追加する
    void removeTeacherColumn(); // 選択中の講師列を削除する
    void renameTeacherColumn(); // 選択中の講師列の講師名を変更する

    bool isValidCellIndex(int row, int column); //schedule tabで使う、表示用のインデックスの正当性チェック

    void loadCell(int row, int column); // 選択セルの内容を編集欄へ読み込む
    void updateCell(); // 編集欄の内容を選択セルへ反映する
    void renderCell(int row, int column); // 指定セルの表示文字と背景色を更新する
    void clearCell(); // 選択セルの授業データを空にする
    void renderEntry(); // 選択セルの授業データを編集欄へ表示する

    void copyCell(); // 選択セルの授業データをクリップボードへコピーする
    void pasteCell(); // クリップボードの授業データを選択セルへ貼り付ける
    void cutCell(); // 選択セルの授業データを切り取る

    void updateStudentComboBox(QComboBox *comboBox, const QString &grade); // 学年に応じた生徒候補を更新する
    void updateSubjectComboBoxForStudent(
        QComboBox *comboBox,
        const QString &grade,
        const QString &studentName); // 生徒に登録された教科候補を更新する
    void updateTeacherComboBox(QComboBox *comboBox); // 講師候補のコンボボックスを更新する

    bool saveScheduleToFile(); // 現在の時間割を週別ファイルへ保存する
    void loadScheduleButton(); // 読み込みボタンから指定週の時間割を読み込む

    void showLastWeek(); // 表示週を前週へ切り替える
    void showThisWeek(); // 表示週を今週へ切り替える
    void showNextWeek(); // 表示週を翌週へ切り替える
    void copyCurrentWeekToThisWeek(); // 現在表示中の週を今週としてコピー保存する
    void copySelectedWeekToCurrentWeek(); // 選択した週の内容を現在の週へコピーする

    // schedule data
    void initializeTeacherLessons(TeacherColumn &teacher); // 講師列の授業枠を現在の時限数と最大人数で初期化する
    void normalizeTeacherLessonMaxStudents(TeacherColumn &teacher); // 授業枠の最大人数設定を有効範囲に補正する
    void initializeTable(); // 空の時間割データを曜日ごとに作成する

    int tableRowCount() const; // 時間割テーブルの表示行数を返す
    int periodIndexFromTableRow(int tableRow) const; // 表示行から時限インデックスを求める
    int studentIndexFromTableRow(int tableRow) const; // 表示行から生徒枠インデックスを求める
    int tableRowOf(int periodIndex, int studentIndex) const; // 時限と生徒枠から表示行を求める
    int lessonMaxStudentsAt(int dayIndex, int teacherIndex, int periodIndex) const; // 指定コマの表示上の最大生徒枠数を返す

    int firstColumnOfDay(int dayIndex) const; // 曜日の先頭表示列を返す
    int columnCountOfDay(int dayIndex) const; // 指定曜日の講師列数を返す
    int dayIndexFromColumn(int column) const; // 表示列から曜日インデックスを求める
    int teacherIndexFromColumn(int column) const; // 表示列から講師インデックスを求める

    QString cellTextFromData(const LessonData &lesson) const; // 授業データからセル表示用テキストを作る
    bool lessonDataIsEmpty(const LessonData &lesson) const; // 授業データが空かどうかを判定する
    QVector<LessonRecord> scheduleEntries() const; // 現在の時間割から空でない授業一覧を作る
    QVector<LessonRecord> scheduleEntriesFor(
        const QDate &monday,
        const QVector<QVector<TeacherColumn>> &scheduleData) const; // 指定週データから授業一覧を作る
    QVector<LessonRecord> scheduleEntriesFor(
        const QDate &monday,
        const QVector<QVector<TeacherColumn>> &scheduleData,
        const QStringList &scheduleDays,
        const QStringList &schedulePeriods) const; // 指定週データと見出しから授業一覧を作る

    QString lessonToJson(const LessonData &lesson) const; // 授業データをコピー用JSONへ変換する
    QString lessonToJson(int row, int column) const; // 指定セルの授業データをコピー用JSONへ変換する
    LessonData jsonToLesson(const QString &json) const; // コピー用JSONから授業データを復元する

    // schedule storage
    bool jsonToScheduleData(
        const QString &json,
        QDate *monday,
        QVector<QVector<TeacherColumn>> *loadedSchedule,
        QStringList *loadedDays = nullptr,
        QStringList *loadedPeriods = nullptr) const; // 時間割JSONを週情報と時間割データへ変換する
    bool loadScheduleDataFromFile(
        const QDate &monday,
        QDate *fileMonday,
        QVector<QVector<TeacherColumn>> *loadedSchedule,
        QStringList *loadedDays = nullptr,
        QStringList *loadedPeriods = nullptr) const; // 指定週の時間割ファイルを読み込む
    void applyScheduleHeaders(
        const QStringList &loadedDays,
        const QStringList &loadedPeriods,
        bool saveAsMasterDefaults); // 読み込んだ曜日と時限の見出しを反映する

    QString scheduleToJson() const; // 現在の時間割全体を保存用JSONへ変換する
    bool jsonToSchedule(const QString &json); // 保存用JSONを現在の時間割へ読み込む

    void loadLatestSchedule(); // 起動時に前回または今週の時間割を読み込む
    bool loadScheduleFromFile(const QDate &monday); // 指定週の時間割を画面へ読み込む
    bool loadScheduleFromFilePath(const QString &filePath); // 指定パスの時間割ファイルを画面へ読み込む

    void switchScheduleWeek(const QDate &date); // 指定日の週へ時間割を切り替える

    QDate mondayOf(const QDate &date) const; // 指定日を含む週の月曜日を返す
    QString schedulesDirPath() const; // 時間割ファイル保存フォルダのパスを返す
    QString scheduleFilePath(const QDate &monday) const; // 指定週の時間割ファイルパスを作る

    // undo
    void undoCellEdit(); // セル編集を1つ元に戻す
    void redoCellEdit(); // 元に戻したセル編集をやり直す
    bool lessonDataEquals(
        const LessonData &a,
        const LessonData &b) const; // 2つの授業データが同じか比較する

    bool setLessonAtCell(
        int row,
        int column,
        const LessonData &lesson); // 指定セルへ授業データを直接設定する

    void pushCellEdit(
        int row,
        int column,
        const LessonData &before,
        const LessonData &after); // セル編集履歴をundoスタックへ追加する

    void clearCellEditHistory(); // undoとredoの履歴を消す
    bool confirmClearCellEditHistory(const QString &operationName); // 履歴が消える操作の確認を取る
    void updateUndoRedoButtons(); // undoとredoボタンの有効状態を更新する

    bool scheduleMatchesSavedFile(); // 現在の時間割が保存済みファイルと一致するか確認する

    // student Tab
    void setupStudentTab(); // student tabの初期表示と接続を行う
    void renderStudentList(); // 生徒一覧を再描画する
    void loadStudent(int index); // 生徒一覧の指定行を編集欄へ読み込む
    void renderStudentEntry(); // 選択中の生徒を編集欄へ再表示する
    void clearStudentEntry(); // 生徒編集欄を空に戻す
    void removeStudent(); // 選択中の生徒を削除する
    void saveStudent(); // 生徒編集欄の内容を追加または更新する
    void loadStudent(); // 生徒データファイルを読み込む
    bool saveStudentsToFile(const QVector<GradeStudents> &allStudents); // 生徒データをファイルへ保存する
    QStringList subjectNamesForStudent(
        const QString &grade,
        const QString &studentName) const; // 生徒に登録された教科名一覧を返す
    QStringList materialNamesForStudentSubject(
        const QString &grade,
        const QString &studentName,
        const QString &subjectName) const; // 生徒と教科に登録された教材名一覧を返す
    void copySelectedStudentScheduleToClipboard(); // 生徒一覧で選択中の生徒予定表をコピーする

    void updateSchoolComboBox(); // 学校名コンボボックスを最新の一覧で更新する
    void addSchoolList(); // 学校一覧へ学校名を追加する
    void deleteSchoolList(); // 学校一覧から学校名を削除する
    void saveSchoolList(); // 学校一覧をファイルへ保存する
    void loadSchoolList(); // 学校一覧をファイルから読み込む

    // teacher Tab
    void setupTeacherTab(); // teacher tabの初期表示と接続を行う
    void renderTeacherList(); // 講師一覧を再描画する
    void loadTeacher(int index); // 講師一覧の指定行を編集欄へ読み込む
    void renderTeacherEntry(); // 選択中の講師を編集欄へ再表示する
    void clearTeacherEntry(); // 講師編集欄を初期値へ戻す
    void removeTeacher(); // 選択中の講師を削除する
    void saveTeacher(); // 講師編集欄の内容を追加または更新する
    void loadTeacher(); // 講師データファイルを読み込む
    bool saveTeachersToFile(); // 講師データをファイルへ保存する
    void copySelectedTeacherScheduleToClipboard(); // 講師一覧で選択中の講師予定表をコピーする

    // export Tab
    void setupExportTab(); // export tabのボタンと出力処理を接続する
    void showSchedulePrintPreview(); // 時間割表の印刷プレビューを表示する
    void exportSchedulePdf(); // 時間割表をPDFファイルへ出力する
    void showTeacherDailyPrintPreview(); // 講師向け授業一覧の印刷プレビューを表示する
    void showSalaryStatementPrintPreview(); // 給与支払明細書の印刷プレビューを表示する
    void showGuidanceReportPrintPreview(); // 指導報告書の印刷プレビューを表示する
    void showSelectedCellGuidanceReportPrintPreview(); // 選択セルの内容で指導報告書を表示する
    QVector<TeacherDailyPayData> salaryDailyPayDefaults(
        const QString &teacherName,
        const QDate &month) const; // 給与明細用の日別支給初期値を作る
    bool editSalaryDailyPays(
        const QString &teacherName,
        const QDate &month,
        QVector<TeacherDailyPayData> *dailyPays); // 日別の業務給や交通費を編集する
    void copyStudentScheduleToClipboard(); // 生徒予定表を選択してクリップボードへコピーする
    void renderScheduleForPrint(QPrinter *printer); // 時間割表をプリンタへ描画する
    void renderTeacherDailyReportForPrint(
        QPrinter *printer,
        const QString &teacherName,
        const QDate &date); // 講師向け授業一覧をプリンタへ描画する
    void renderSalaryStatementForPrint(
        QPrinter *printer,
        const QString &teacherName,
        const QDate &month,
        const QVector<int> &deductions,
        const QVector<TeacherDailyPayData> &dailyPays); // 給与支払明細書をプリンタへ描画する
    void renderGuidanceReportFormatForPrint(
        QPrinter *printer,
        const QString &grade,
        const QString &studentName,
        const QString &subjectName,
        const QStringList &materialNames); // 指導報告書フォーマットをプリンタへ描画する
    bool findStudentData(
        const QString &grade,
        const QString &studentName,
        StudentData *student) const; // 学年と名前から生徒データを探す
    bool findTeacherData(
        const QString &teacherName,
        TeacherData *teacher) const; // 名前から講師データを探す
    QString studentNameWithHonorific(
        const QString &grade,
        const QString &studentName,
        bool insertSpace) const; // 設定に従って生徒名に敬称を付ける
    bool selectTeacherScheduleOptions(
        QString *teacherName,
        QDate *date,
        const QString &title,
        const QString &defaultTeacherName = QString()); // 講師向け出力の講師と日付を選択する
    QString teacherScheduleText(
        const QString &teacherName,
        const QDate &date) const; // 講師予定表のコピー用テキストを作る
    QVector<TeacherScheduleBlock> teacherScheduleBlocks(
        const QString &teacherName,
        const QDate &date,
        bool includeEmptySlots) const; // 講師向け授業一覧の時限ブロックを作る
    bool selectStudentSubject(
        QString *grade,
        QString *studentName,
        QString *subjectName,
        QStringList *materialNames,
        const QString &title,
        bool requireSubject,
        bool includeMaterial,
        bool allowBlankSelection = false); // 生徒と教科と教材を選択する
    bool findNextLessonForStudent(
        const LessonRecord &baseLesson,
        LessonRecord *nextLesson) const; // 指定授業の次にある同じ生徒の授業を探す
    QString studentScheduleText(
        const QString &grade,
        const QString &studentName,
        const QString &subjectName) const; // 生徒予定表のコピー用テキストを作る
    int totalScheduleTeacherColumns() const; // 全曜日の講師列数合計を返す
    QRectF schedulePrintContentRect(QPrinter *printer) const; // 時間割印刷の描画範囲を計算する
    qreal schedulePrintLineWidth(QPainter *painter, int width) const; // 印刷設定に合わせた線幅を計算する
    QColor schedulePrintColor(const QString &colorName) const; // 印刷用に補正した色を返す
    void drawSchedulePrintText(
        QPainter *painter,
        const QRectF &rect,
        const QString &text,
        int alignment,
        bool bold = false) const; // 時間割印刷用の文字を描画する
    void fillSchedulePrintCellBackground(
        QPainter *painter,
        const QRectF &rect,
        int tableRow,
        int dayIndex,
        int teacherIndex,
        int periodIndex,
        int studentIndex,
        const LessonData *lesson) const; // 時間割印刷セルの背景色を塗る
    void drawSchedulePrintLines(
        QPainter *painter,
        const QRectF &rect,
        bool drawRightSection,
        bool drawBottomSection) const; // 時間割印刷セルの罫線を描く
    void drawSchedulePrintHeader(
        QPainter *painter,
        const QRectF &area,
        qreal timeColumnWidth,
        qreal teacherColumnWidth,
        qreal dayHeaderHeight,
        qreal teacherHeaderHeight) const; // 時間割印刷の曜日と講師ヘッダーを描く
    void drawSchedulePrintBody(
        QPainter *painter,
        const QRectF &area,
        qreal timeColumnWidth,
        qreal teacherColumnWidth,
        qreal headerHeight,
        qreal studentRowHeight) const; // 時間割印刷の時限と授業セルを描く
};

#endif // MAINWINDOW_H
