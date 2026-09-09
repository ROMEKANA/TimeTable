#include "mainwindow.h"
#include "settingsdialog.h"
#include "ui_mainwindow.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>

// マニュアルの読み込みと画面操作を接続する
void MainWindow::setupManualTab()
{
    ui->manualSplitter->setChildrenCollapsible(false);
    ui->manualSplitter->setStretchFactor(0, 0);
    ui->manualSplitter->setStretchFactor(1, 1);
    manualStyleSettings = loadMasterJson();
    connect(ui->manualTopicTree, &QTreeWidget::currentItemChanged, this, [this]() { showManualPage(); });
    connect(ui->manualSearchEdit, &QLineEdit::textChanged, this, [this]() { filterManualPages(); });
    connect(ui->manualReloadButton, &QPushButton::clicked, this, &MainWindow::reloadManual);
    connect(ui->manualStyleButton, &QPushButton::clicked, this, [this]() { showSettingsDialog(SettingsDialog::ManualTab); });
    connect(ui->manualOpenFolderButton, &QPushButton::clicked, this, [this]()
    {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(manualFilePath()).absolutePath())))
            QMessageBox::warning(this, "フォルダーを開けません", manualFilePath());
    });
    connect(ui->manualBrowser, &QTextBrowser::anchorClicked, this, [this](const QUrl &url)
    {
        if (url.scheme() != "timetable") return;
        const QString value = url.path().mid(1);
        if (url.host() == "topic")
        {
            for (int i = 0; i < manualPages.size(); ++i)
            {
                if (manualPages[i].id.isEmpty() || manualPages[i].id != value) continue;
                ui->manualSearchEdit->clear();
                for (int c = 0; c < ui->manualTopicTree->topLevelItemCount(); ++c)
                {
                    QTreeWidgetItem *category = ui->manualTopicTree->topLevelItem(c);
                    for (int t = 0; t < category->childCount(); ++t)
                    {
                        QTreeWidgetItem *item = category->child(t);
                        if (item->data(0, Qt::UserRole).toInt() != i) continue;
                        ui->manualTopicTree->setCurrentItem(item);
                        ui->manualTopicTree->scrollToItem(item);
                        return;
                    }
                }
            }
            QMessageBox::information(this, "ページが見つかりません", "リンク先のページIDを確認してください：" + value);
            return;
        }
        bool ok = false;
        const int index = value.toInt(&ok);
        if (!ok) return;
        if (url.host() == "tab" && index >= 0 && index < ui->mainTabWidget->count())
            ui->mainTabWidget->setCurrentIndex(index);
        else if (url.host() == "settings" && index >= 0 && index <= SettingsDialog::ManualTab)
            showSettingsDialog(index);
    });
    reloadManual();
}

// 編集済みのMarkdownを読み込み、成功時に目次を入れ替える
void MainWindow::reloadManual()
{
    QString error;
    QVector<ManualPage> pages;
    if (!loadManualPages(&pages, &error))
    {
        ui->manualFileLabel->setText(error + "\n" + QDir::toNativeSeparators(manualFilePath()) +
                                   (manualPages.isEmpty() ? "" : "\n直前に読み込めた内容を表示しています。"));
        return;
    }
    QString previousId;
    QString previousTitle;
    QString previousCategory;
    const QTreeWidgetItem *current = ui->manualTopicTree->currentItem();
    if (current && current->parent())
    {
        const int index = current->data(0, Qt::UserRole).toInt();
        if (index >= 0 && index < manualPages.size())
        {
            previousId = manualPages[index].id;
            previousTitle = manualPages[index].title;
            previousCategory = manualPages[index].category;
        }
    }
    manualPages = std::move(pages);
    manualRenderedPage = -1;
    {
        const QSignalBlocker blocker(ui->manualTopicTree);
        ui->manualTopicTree->clear();
        QTreeWidgetItem *category = nullptr;
        QTreeWidgetItem *selected = nullptr;
        for (int i = 0; i < manualPages.size(); ++i)
        {
            const ManualPage &page = manualPages[i];
            if (!category || category->text(0) != page.category)
            {
                category = new QTreeWidgetItem(ui->manualTopicTree, {page.category});
                category->setFlags(category->flags() & ~Qt::ItemIsSelectable);
                category->setExpanded(true);
            }
            auto *item = new QTreeWidgetItem(category, {page.title});
            item->setData(0, Qt::UserRole, i);
            if ((!previousId.isEmpty() && page.id == previousId) ||
                (previousId.isEmpty() && page.title == previousTitle && page.category == previousCategory))
                selected = item;
        }
        if (selected) ui->manualTopicTree->setCurrentItem(selected);
    }
    ui->manualFileLabel->setText("編集できるファイル：" + QDir::toNativeSeparators(manualFilePath()));
    filterManualPages();
}

// メモリ上の本文を検索して目次を絞り込む
void MainWindow::filterManualPages()
{
    const QStringList terms = ui->manualSearchEdit->text().simplified().toCaseFolded().split(' ', Qt::SkipEmptyParts);
    QTreeWidgetItem *first = nullptr;
    int count = 0;
    for (int c = 0; c < ui->manualTopicTree->topLevelItemCount(); ++c)
    {
        QTreeWidgetItem *category = ui->manualTopicTree->topLevelItem(c);
        bool categoryVisible = false;
        for (int t = 0; t < category->childCount(); ++t)
        {
            QTreeWidgetItem *item = category->child(t);
            const int index = item->data(0, Qt::UserRole).toInt();
            bool matches = true;
            for (const QString &term : terms)
            {
                if (!manualPages[index].searchText.contains(term)) { matches = false; break; }
            }
            item->setHidden(!matches);
            if (!matches) continue;
            ++count;
            categoryVisible = true;
            if (!first) first = item;
        }
        category->setHidden(!categoryVisible);
        category->setExpanded(true);
    }
    ui->manualResultLabel->setText(terms.isEmpty() ? QString("全%1件").arg(count) : QString("%1件").arg(count));
    if (!first)
    {
        ui->manualStackedWidget->setCurrentWidget(ui->manualNoResultsPage);
        return;
    }
    QTreeWidgetItem *current = ui->manualTopicTree->currentItem();
    if (!current || !current->parent() || current->isHidden())
    {
        const QSignalBlocker blocker(ui->manualTopicTree);
        ui->manualTopicTree->setCurrentItem(first);
    }
    showManualPage();
}

// 選択されたページだけを描画する
void MainWindow::showManualPage()
{
    const QTreeWidgetItem *item = ui->manualTopicTree->currentItem();
    if (!item || !item->parent() || item->isHidden()) return;
    const int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= manualPages.size()) return;
    if (manualRenderedPage == index)
    {
        ui->manualStackedWidget->setCurrentWidget(ui->manualBrowser);
        return;
    }
    QColor background(manualStyleSettings.value("manualBackgroundColor").toString("#ffffff"));
    if (!background.isValid()) background = QColor("#ffffff");
    QPalette palette = ui->manualBrowser->palette();
    palette.setColor(QPalette::Base, background);
    ui->manualBrowser->setPalette(palette);
    renderManualPage(ui->manualBrowser->document(), manualPages[index], manualStyleSettings);
    manualRenderedPage = index;
    ui->manualBrowser->verticalScrollBar()->setValue(0);
    ui->manualStackedWidget->setCurrentWidget(ui->manualBrowser);
}

// 保存済み設定を表示中のマニュアルへ反映する
void MainWindow::applyManualStyle()
{
    manualRenderedPage = -1;
    manualStyleSettings = loadMasterJson();
    if (ui->manualStackedWidget->currentWidget() == ui->manualBrowser) showManualPage();
}
