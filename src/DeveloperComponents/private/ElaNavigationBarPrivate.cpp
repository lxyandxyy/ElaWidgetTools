#include "ElaNavigationBarPrivate.h"

#include <QLayout>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QTimer>

#include "DeveloperComponents/ElaCompactDelegate.h"
#include "DeveloperComponents/ElaCompactModel.h"
#include "DeveloperComponents/ElaFooterDelegate.h"
#include "DeveloperComponents/ElaFooterModel.h"
#include "DeveloperComponents/ElaNavigationBar.h"
#include "DeveloperComponents/ElaNavigationDelegate.h"
#include "DeveloperComponents/ElaNavigationModel.h"
#include "DeveloperComponents/ElaNavigationNode.h"
#include "DeveloperComponents/ElaNavigationSuggestBox.h"
#include "DeveloperComponents/ElaNavigationView.h"
#include "ElaApplication.h"
#include "ElaBreadcrumbBar.h"
#include "ElaListView.h"
#include "ElaMenu.h"
#include "ElaNavigationSuggestBoxPrivate.h"
#include "ElaScrollPage.h"
#include "ElaWindow.h"
#include "private/ElaScrollPagePrivate.h"
#include "private/ElaWindowPrivate.h"
ElaNavigationBarPrivate::ElaNavigationBarPrivate(QObject* parent)
    : QObject{parent}
{
}

ElaNavigationBarPrivate::~ElaNavigationBarPrivate()
{
}

QStringList ElaNavigationBarPrivate::getLastRoute()
{
    int routeQueueCount = _routeQueue.count();
    if (routeQueueCount > 0)
    {
        if (routeQueueCount == 1)
        {
            Q_EMIT routeBackButtonStateChanged(false);
        }
        QStringList nodeList = _routeQueue.last();
        _routeQueue.removeLast();
        return nodeList;
    }
    return QStringList();
}

void ElaNavigationBarPrivate::onThemeChanged(ElaApplicationType::ThemeMode themeMode)
{
    if (themeMode == ElaApplicationType::Light)
    {
        _windowLinearGradient->setColorAt(0, QColor(0xF3, 0xF2, 0xF9));
        _windowLinearGradient->setColorAt(1, QColor(0xF4, 0xF1, 0xF8));
    }
    else
    {
        _windowLinearGradient->setColorAt(0, QColor(0x1A, 0x1A, 0x1A));
        _windowLinearGradient->setColorAt(1, QColor(0x1A, 0x1A, 0x1A));
    }
}

void ElaNavigationBarPrivate::onRouteBackButtonClicked()
{
    Q_Q(ElaNavigationBar);
    QStringList lastNodeKeyList = getLastRoute();
    if (lastNodeKeyList.isEmpty())
    {
        return;
    }
    if (lastNodeKeyList.count() == 1)
    {
        q->navigation(lastNodeKeyList[0], false);
    }
    else
    {
        // 面包屑
        q->navigation(lastNodeKeyList[0], false);
        QString stateCheckSumKey = lastNodeKeyList[1];
        ElaWindow* window = dynamic_cast<ElaWindow*>(q->parent());
        QStackedWidget* centerStackedWidget = window->d_ptr->_centerStackedWidget;
        if (centerStackedWidget->currentWidget()->property("ElaBaseClassName").toString() == "ElaScrollPage")
        {
            ElaScrollPage* scrollPage = dynamic_cast<ElaScrollPage*>(centerStackedWidget->currentWidget());
            if (scrollPage)
            {
                if (stateCheckSumKey == "Navigation")
                {
                    scrollPage->navigation(scrollPage->d_ptr->_centralWidgetMap.value(lastNodeKeyList[2]), false);
                }
                else if (stateCheckSumKey == "BreadcrumbClicked")
                {
                    int widgetIndex = scrollPage->d_ptr->_centralWidgetMap.value(lastNodeKeyList.last());
                    scrollPage->d_ptr->_switchCentralStackIndex(widgetIndex, scrollPage->d_ptr->_navigationTargetIndex);
                    scrollPage->d_ptr->_navigationTargetIndex = widgetIndex;
                    scrollPage->d_ptr->_breadcrumbBar->setBreadcrumbList(lastNodeKeyList.mid(2));
                }
            }
        }
    }
}

void ElaNavigationBarPrivate::onElaRouteEvent(QMap<QString, QVariant> data)
{
    QStringList pageKeyList = data.value("ElaPageKey").toStringList();
    if (pageKeyList.isEmpty())
    {
        return;
    }
    if (_routeQueue.isEmpty())
    {
        Q_EMIT routeBackButtonStateChanged(true);
    }
    else
    {
        if (_routeQueue.count() >= 25)
        {
            _routeQueue.dequeue();
        }
    }
    _routeQueue.enqueue(pageKeyList);
}

void ElaNavigationBarPrivate::onTreeViewClicked(const QModelIndex& index, bool isLogRoute)
{
    if (index.isValid())
    {
        ElaNavigationNode* node = static_cast<ElaNavigationNode*>(index.internalPointer());
        if (!node)
        {
            return;
        }
        if (node->getIsExpanderNode())
        {
            if (node->getIsHasChild())
            {
                QMap<QString, QVariant> data;
                if (_navigationView->isExpanded(index))
                {
                    // 收起
                    data.insert("Collapse", QVariant::fromValue(node));
                    node->setIsExpanded(false);
                    _navigationDelegate->navigationNodeStateChange(data);
                    _navigationView->navigationNodeStateChange(data);
                    _navigationView->collapse(index);
                }
                else
                {
                    // 展开
                    data.insert("Expand", QVariant::fromValue(node));
                    node->setIsExpanded(true);
                    _navigationDelegate->navigationNodeStateChange(data);
                    _navigationView->navigationNodeStateChange(data);
                    _navigationView->expand(index);
                }
            }
        }
        else
        {
            if (node->getKeyPoints())
            {
                node->setKeyPoints(0);
                _navigationView->update();
            }
            ElaNavigationNode* selectedNode = _navigationModel->getSelectedNode();
            if (selectedNode != node)
            {
                // 记录跳转
                if (isLogRoute)
                {
                    QMap<QString, QVariant> postData = QMap<QString, QVariant>();
                    QStringList pageKeyList;
                    if (selectedNode)
                    {
                        pageKeyList.append(selectedNode->getNodeKey());
                    }
                    else
                    {
                        if (_footerModel->getSelectedNode())
                        {
                            pageKeyList.append(_footerModel->getSelectedNode()->getNodeKey());
                        }
                    }
                    postData.insert("ElaPageKey", pageKeyList);
                    onElaRouteEvent(postData);
                }
                _switchMainStackIndex(node->getNodeKey());
                QMap<QString, QVariant> compactPostData = QMap<QString, QVariant>();
                compactPostData.insert("SelectMarkChanged", true);
                if (_footerModel->getSelectedNode())
                {
                    _footerView->clearSelection();
                    QMap<QString, QVariant> footerPostData = QMap<QString, QVariant>();
                    footerPostData.insert("SelectMarkChanged", true);
                    footerPostData.insert("LastSelectedNode", QVariant::fromValue(_footerModel->getSelectedNode()));
                    footerPostData.insert("SelectedNode", QVariant::fromValue(nullptr));
                    compactPostData.insert("LastSelectedNode", QVariant::fromValue(_footerModel->getSelectedNode()));
                    _footerModel->setSelectedNode(nullptr);
                    _footerDelegate->navigationNodeStateChange(footerPostData);
                }
                QMap<QString, QVariant> postData = QMap<QString, QVariant>();
                postData.insert("SelectMarkChanged", true);
                if (_navigationModel->getSelectedNode())
                {
                    compactPostData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedNode()->getOriginalNode()));
                }
                if (_navigationModel->getSelectedExpandedNode())
                {
                    postData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedExpandedNode()));
                }
                else
                {
                    postData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedNode()));
                }
                postData.insert("SelectedNode", QVariant::fromValue(node));
                ElaNavigationNode* originalNode = node->getOriginalNode();
                compactPostData.insert("SelectedNode", QVariant::fromValue(originalNode));
                //避免同一起源节点下的无效Mark动画
                if (originalNode != _compactModel->getSelectedNode())
                {
                    _compactModel->setSelectedNode(originalNode);
                    _compactDelegate->navigationNodeStateChange(compactPostData);
                }
                _navigationModel->setSelectedNode(node);
                _navigationDelegate->navigationNodeStateChange(postData);
                if (!node->getIsVisible())
                {
                    _expandSelectedNodeParent();
                }
            }
        }
        _resetNodeSelected();
    }
}

void ElaNavigationBarPrivate::onFooterViewClicked(const QModelIndex& index, bool isLogRoute)
{
    ElaNavigationNode* node = index.data(Qt::UserRole).value<ElaNavigationNode*>();
    if (node->getKeyPoints())
    {
        node->setKeyPoints(0);
        _navigationView->update();
    }
    ElaNavigationNode* selectedNode = _footerModel->getSelectedNode();

    if (selectedNode != node)
    {
        // 记录跳转
        if (isLogRoute && node->getIsHasFooterPage())
        {
            QMap<QString, QVariant> postData = QMap<QString, QVariant>();
            QStringList pageKeyList;
            if (selectedNode)
            {
                pageKeyList.append(selectedNode->getNodeKey());
            }
            else
            {
                if (_navigationModel->getSelectedNode())
                {
                    pageKeyList.append(_navigationModel->getSelectedNode()->getNodeKey());
                }
            }
            postData.insert("ElaPageKey", pageKeyList);
            onElaRouteEvent(postData);
        }
        _switchMainStackIndex(node->getNodeKey());

        if (node->getIsHasFooterPage())
        {
            QMap<QString, QVariant> compactPostData = QMap<QString, QVariant>();
            compactPostData.insert("SelectMarkChanged", true);
            if (_navigationModel->getSelectedNode() || _navigationModel->getSelectedExpandedNode())
            {
                QMap<QString, QVariant> mainPostData = QMap<QString, QVariant>();
                mainPostData.insert("SelectMarkChanged", true);
                mainPostData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedExpandedNode() ? _navigationModel->getSelectedExpandedNode() : _navigationModel->getSelectedNode()));
                mainPostData.insert("SelectedNode", QVariant::fromValue(nullptr));
                compactPostData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedNode()->getOriginalNode()));
                _navigationView->clearSelection();
                _navigationDelegate->navigationNodeStateChange(mainPostData);
                _navigationModel->setSelectedExpandedNode(nullptr);
                _navigationModel->setSelectedNode(nullptr);
            }
            _footerView->clearSelection();
            _footerView->selectionModel()->select(index, QItemSelectionModel::Select);
            QMap<QString, QVariant> postData = QMap<QString, QVariant>();
            postData.insert("SelectMarkChanged", true);
            postData.insert("LastSelectedNode", QVariant::fromValue(_footerModel->getSelectedNode()));
            postData.insert("SelectedNode", QVariant::fromValue(node));
            compactPostData.insert("SelectedNode", QVariant::fromValue(node));
            _compactModel->setSelectedNode(node);
            _compactDelegate->navigationNodeStateChange(compactPostData);
            _footerDelegate->navigationNodeStateChange(postData);
            _footerModel->setSelectedNode(node);
        }
    }
}

void ElaNavigationBarPrivate::onCompactViewClicked(const QModelIndex& index)
{
    ElaNavigationNode* node = index.data(Qt::UserRole).value<ElaNavigationNode*>();
    if (node->getIsFooterNode())
    {
        this->onFooterViewClicked(node->getModelIndex());
    }
    else
    {
        if (node->getIsExpanderNode())
        {
            if (node->getIsHasPageChild())
            {
                //展开菜单
                ElaMenu* menu = _compactMenuMap.value(node);
                if (menu)
                {
                    menu->popup(QCursor::pos());
                }
            }
        }
        else
        {
            this->onTreeViewClicked(node->getModelIndex());
        }
    }
}

void ElaNavigationBarPrivate::_resetNodeSelected()
{
    _navigationView->clearSelection();
    ElaNavigationNode* selectedNode = _navigationModel->getSelectedNode();
    if (!selectedNode || !selectedNode->getModelIndex().isValid())
    {
        return;
    }
    if (selectedNode->getParentNode()->getIsRootNode() || selectedNode->getIsVisible())
    {
        _navigationView->selectionModel()->select(selectedNode->getModelIndex(), QItemSelectionModel::Select);
        if (_navigationModel->getSelectedExpandedNode())
        {
            QMap<QString, QVariant> postData = QMap<QString, QVariant>();
            postData.insert("SelectMarkChanged", true);
            postData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedExpandedNode()));
            postData.insert("SelectedNode", QVariant::fromValue(selectedNode));
            _navigationDelegate->navigationNodeStateChange(postData);
        }
        _navigationModel->setSelectedExpandedNode(nullptr);
    }
    else
    {
        ElaNavigationNode* parentNode = selectedNode->getParentNode();
        while (parentNode && !parentNode->getParentNode()->getIsRootNode() && !parentNode->getIsVisible())
        {
            parentNode = parentNode->getParentNode();
        }
        if (!parentNode)
        {
            return;
        }
        // 单级节点展开/收起时Mark变化
        if (!_navigationModel->getSelectedExpandedNode())
        {
            QMap<QString, QVariant> postData = QMap<QString, QVariant>();
            postData.insert("SelectMarkChanged", true);
            postData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedNode()));
            postData.insert("SelectedNode", QVariant::fromValue(parentNode));
            _navigationDelegate->navigationNodeStateChange(postData);
        }
        else
        {
            // 多级节点
            if (_navigationModel->getSelectedExpandedNode() != parentNode)
            {
                // 同一起源节点展开/收起时的Mark变化
                if (_navigationModel->getSelectedExpandedNode()->getOriginalNode() == parentNode->getOriginalNode())
                {
                    QMap<QString, QVariant> postData = QMap<QString, QVariant>();
                    postData.insert("SelectMarkChanged", true);
                    postData.insert("LastSelectedNode", QVariant::fromValue(_navigationModel->getSelectedExpandedNode()));
                    postData.insert("SelectedNode", QVariant::fromValue(parentNode));
                    _navigationDelegate->navigationNodeStateChange(postData);
                }
            }
        }
        _navigationModel->setSelectedExpandedNode(parentNode);
        _navigationView->selectionModel()->select(parentNode->getModelIndex(), QItemSelectionModel::Select);
    }
}

void ElaNavigationBarPrivate::_expandSelectedNodeParent()
{
    ElaNavigationNode* parentNode = _navigationModel->getSelectedNode()->getParentNode();
    while (parentNode && !parentNode->getIsRootNode())
    {
        QMap<QString, QVariant> data;
        data.insert("Expand", QVariant::fromValue(parentNode));
        _navigationDelegate->navigationNodeStateChange(data);
        _navigationView->navigationNodeStateChange(data);
        parentNode->setIsExpanded(true);
        _navigationView->expand(parentNode->getModelIndex());
        parentNode = parentNode->getParentNode();
    }
}

void ElaNavigationBarPrivate::_initNodeModelIndex(const QModelIndex& parentIndex)
{
    int rowCount = _navigationModel->rowCount(parentIndex);
    for (int row = 0; row < rowCount; ++row)
    {
        QModelIndex index = _navigationModel->index(row, 0, parentIndex);
        ElaNavigationNode* childNode = static_cast<ElaNavigationNode*>(index.internalPointer());
        childNode->setModelIndex(index);
        if (_navigationModel->hasChildren(index))
        {
            _initNodeModelIndex(index);
        }
    }
}

void ElaNavigationBarPrivate::_switchMainStackIndex(QString nodeKey)
{
    Q_Q(ElaNavigationBar);
    ElaWindow* window = dynamic_cast<ElaWindow*>(q->parent());
    QStackedWidget* centerStackedWidget = window->d_ptr->_centerStackedWidget;
    int nodeIndex = _routeMap.value(nodeKey);
    if (nodeIndex == -1)
    {
        // 页脚没有绑定页面
        Q_EMIT q->footerNodeClicked(nodeKey);
        return;
    }
    if (_navigationTargetIndex == nodeIndex || centerStackedWidget->count() <= nodeIndex)
    {
        return;
    }
    _navigationTargetIndex = nodeIndex;
    QTimer::singleShot(180, this, [=]() {
        QWidget* currentWidget = centerStackedWidget->widget(nodeIndex);
        QPropertyAnimation* currentWidgetAnimation = new QPropertyAnimation(currentWidget, "pos");
        currentWidgetAnimation->setEasingCurve(QEasingCurve::InOutCubic);
        currentWidgetAnimation->setDuration(280);
        QPoint currentWidgetPos = currentWidget->pos();
        currentWidgetPos.setY(10);
        currentWidgetAnimation->setEndValue(currentWidgetPos);
        currentWidgetPos.setY(currentWidgetPos.y() + 60);
        currentWidgetAnimation->setStartValue(currentWidgetPos);
        centerStackedWidget->setCurrentIndex(nodeIndex);
        currentWidgetAnimation->start(QAbstractAnimation::DeleteWhenStopped); });
}

void ElaNavigationBarPrivate::_raiseNavigationBar()
{
    Q_Q(ElaNavigationBar);
    q->raise();
    _navigationSuggestBox->d_ptr->_raiseSearchView();
}

void ElaNavigationBarPrivate::_addStackedPage(QWidget* page, QString pageKey)
{
    Q_Q(ElaNavigationBar);
    page->setProperty("ElaPageKey", pageKey);
    ElaWindow* window = dynamic_cast<ElaWindow*>(q->parent());
    QStackedWidget* centerStackedWidget = window->d_ptr->_centerStackedWidget;
    _routeMap.insert(pageKey, centerStackedWidget->count());
    centerStackedWidget->addWidget(page);
    _initNodeModelIndex(QModelIndex());
    _navigationSuggestBox->d_ptr->_appendPageNode(_navigationModel->getNavigationNode(pageKey));
    if (centerStackedWidget->count() == 1)
    {
        _resetNodeSelected();
    }
}

void ElaNavigationBarPrivate::_addFooterPage(QWidget* page, QString footKey)
{
    Q_Q(ElaNavigationBar);
    if (page)
    {
        page->setProperty("ElaPageKey", footKey);
        ElaWindow* window = dynamic_cast<ElaWindow*>(q->parent());
        QStackedWidget* centerStackedWidget = window->d_ptr->_centerStackedWidget;
        _routeMap.insert(footKey, centerStackedWidget->count());
        centerStackedWidget->addWidget(page);
    }
    else
    {
        _routeMap.insert(footKey, -1);
    }
    _footerView->setFixedHeight(40 * _footerModel->getFooterNodeCount());
    _navigationSuggestBox->d_ptr->_appendPageNode(_footerModel->getNavigationNode(footKey));
}

void ElaNavigationBarPrivate::_setLayoutWidgetsVisible(QLayout* layout, bool isVisible)
{
    if (!layout)
    {
        return;
    }
    for (int i = 0; i < layout->count(); i++)
    {
        QWidget* widget = layout->itemAt(i)->widget();
        if (widget)
        {
            widget->setVisible(isVisible);
        }
    }
}
