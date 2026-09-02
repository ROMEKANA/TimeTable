#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>

#include <algorithm>

namespace
{
    constexpr int TopicIdRole = Qt::UserRole;
    constexpr int TopicPageRole = Qt::UserRole + 1;
    constexpr int TopicSearchRole = Qt::UserRole + 2;

    // UI上の目次順と各ページを対応付けるID一覧を返す
    QStringList manualTopicIds()
    {
        return {
            "teacher-flow",
            "teacher-memo",
            "teacher-output",
            "admin-start",
            "admin-master",
            "admin-schedule",
            "admin-output",
            "admin-guidance-pdf",
            "admin-data",
            "qa-edit-save",
            "qa-output",
            "qa-data",
            "quick-reference"
        };
    }

    // UIに配置した目次から選択可能な項目を表示順に取得する
    QVector<QTreeWidgetItem *> manualTopicItems(QTreeWidget *tree)
    {
        QVector<QTreeWidgetItem *> items;

        for (int categoryIndex = 0;
             categoryIndex < tree->topLevelItemCount();
             ++categoryIndex)
        {
            QTreeWidgetItem *categoryItem = tree->topLevelItem(categoryIndex);
            categoryItem->setFlags(
                categoryItem->flags() & ~Qt::ItemIsSelectable);
            categoryItem->setExpanded(true);

            for (int topicIndex = 0;
                 topicIndex < categoryItem->childCount();
                 ++topicIndex)
            {
                items.append(categoryItem->child(topicIndex));
            }
        }

        return items;
    }

    // UIに配置した各ページから本文表示欄を取得する
    QVector<QTextBrowser *> manualBrowsers(QStackedWidget *stackedWidget)
    {
        QVector<QTextBrowser *> browsers;

        for (int pageIndex = 0;
             pageIndex < stackedWidget->count();
             ++pageIndex)
        {
            QTextBrowser *browser =
                stackedWidget->widget(pageIndex)->findChild<QTextBrowser *>();

            if (browser != nullptr)
            {
                browsers.append(browser);
            }
        }

        return browsers;
    }

    // 指定IDに対応する目次項目を探す
    QTreeWidgetItem *findTopicItem(
        const QVector<QTreeWidgetItem *> &items,
        const QString &topicId)
    {
        for (QTreeWidgetItem *item : items)
        {
            if (item->data(0, TopicIdRole).toString() == topicId)
            {
                return item;
            }
        }

        return nullptr;
    }

    // 検索結果内で最初に表示されている項目を返す
    QTreeWidgetItem *firstVisibleTopicItem(
        const QVector<QTreeWidgetItem *> &items)
    {
        for (QTreeWidgetItem *item : items)
        {
            if (!item->isHidden())
            {
                return item;
            }
        }

        return nullptr;
    }
}

// UIに配置したマニュアルへ検索・ページ移動・画面移動の動作を設定する
void MainWindow::setupManualTab()
{
    const QVector<QTreeWidgetItem *> topicItems =
        manualTopicItems(ui->manualTopicTree);
    const QVector<QTextBrowser *> browsers =
        manualBrowsers(ui->manualStackedWidget);
    const QStringList topicIds = manualTopicIds();
    const int topicCount = std::min(
        topicItems.size(),
        std::min(browsers.size(), topicIds.size()));

    for (int topicIndex = 0; topicIndex < topicCount; ++topicIndex)
    {
        QTreeWidgetItem *topicItem = topicItems[topicIndex];
        QTextBrowser *browser = browsers[topicIndex];
        topicItem->setData(0, TopicIdRole, topicIds[topicIndex]);
        topicItem->setData(0, TopicPageRole, topicIndex);
        topicItem->setData(
            0,
            TopicSearchRole,
            QString("%1 %2 %3")
                .arg(
                    topicItem->parent()->text(0),
                    topicItem->text(0),
                    browser->toPlainText())
                .toCaseFolded());
        browser->setOpenExternalLinks(false);
        browser->setOpenLinks(false);
    }

    ui->manualSplitter->setChildrenCollapsible(false);
    ui->manualSplitter->setStretchFactor(0, 0);
    ui->manualSplitter->setStretchFactor(1, 1);
    ui->manualResultLabel->setText(QString("全%1件").arg(topicCount));

    connect(
        ui->manualTopicTree,
        &QTreeWidget::currentItemChanged,
        this,
        [this](QTreeWidgetItem *current, QTreeWidgetItem *)
        {
            if (current == nullptr)
            {
                return;
            }

            const int pageIndex = current->data(0, TopicPageRole).toInt();

            if (pageIndex >= 0 &&
                pageIndex < ui->manualStackedWidget->count() - 1)
            {
                ui->manualStackedWidget->setCurrentIndex(pageIndex);
            }
        });

    connect(
        ui->manualSearchEdit,
        &QLineEdit::textChanged,
        this,
        [this, topicItems](const QString &query)
        {
            const QStringList terms = query.simplified().toCaseFolded().split(
                ' ',
                Qt::SkipEmptyParts);
            int visibleTopicCount = 0;

            for (QTreeWidgetItem *topicItem : topicItems)
            {
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
                    ++visibleTopicCount;
                }
            }

            for (int categoryIndex = 0;
                 categoryIndex < ui->manualTopicTree->topLevelItemCount();
                 ++categoryIndex)
            {
                QTreeWidgetItem *categoryItem =
                    ui->manualTopicTree->topLevelItem(categoryIndex);
                bool hasVisibleTopic = false;

                for (int topicIndex = 0;
                     topicIndex < categoryItem->childCount();
                     ++topicIndex)
                {
                    if (!categoryItem->child(topicIndex)->isHidden())
                    {
                        hasVisibleTopic = true;
                        break;
                    }
                }

                categoryItem->setHidden(!hasVisibleTopic);
                categoryItem->setExpanded(true);
            }

            ui->manualResultLabel->setText(
                terms.isEmpty()
                    ? QString("全%1件").arg(visibleTopicCount)
                    : QString("%1件").arg(visibleTopicCount));

            if (visibleTopicCount == 0)
            {
                ui->manualStackedWidget->setCurrentWidget(
                    ui->manualNoResultsPage);
                return;
            }

            QTreeWidgetItem *currentItem =
                ui->manualTopicTree->currentItem();

            if (currentItem == nullptr || currentItem->isHidden())
            {
                currentItem = firstVisibleTopicItem(topicItems);

                if (currentItem != nullptr)
                {
                    ui->manualTopicTree->setCurrentItem(currentItem);
                }
            }

            if (currentItem != nullptr)
            {
                const int pageIndex =
                    currentItem->data(0, TopicPageRole).toInt();
                ui->manualStackedWidget->setCurrentIndex(pageIndex);
            }
        });

    const auto openManualLink =
        [this, topicItems](const QUrl &url)
        {
            if (url.scheme() != "timetable")
            {
                return;
            }

            const QString value = url.path().mid(1);

            if (url.host() == "topic")
            {
                QTreeWidgetItem *topicItem =
                    findTopicItem(topicItems, value);

                if (topicItem != nullptr)
                {
                    ui->manualSearchEdit->clear();
                    ui->manualTopicTree->setCurrentItem(topicItem);
                    ui->manualTopicTree->scrollToItem(topicItem);
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
        };

    for (QTextBrowser *browser : browsers)
    {
        connect(
            browser,
            &QTextBrowser::anchorClicked,
            this,
            openManualLink);
    }

    if (!topicItems.isEmpty())
    {
        ui->manualTopicTree->setCurrentItem(topicItems.first());
    }
}
