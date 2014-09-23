/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "helpwidget.h"

#include "helpconstants.h"
#include "helpplugin.h"
#include "helpviewer.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>
#include <coreplugin/findplaceholder.h>
#include <texteditor/texteditorconstants.h>
#include <utils/qtcassert.h>
#include <utils/styledbar.h>

#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

static QToolButton *toolButton(QAction *action, Core::Command *cmd = 0)
{
    QToolButton *button = new QToolButton;
    button->setDefaultAction(action);
    button->setPopupMode(QToolButton::DelayedPopup);
    if (cmd) {
        action->setToolTip(cmd->stringWithAppendedShortcut(action->text()));
        QObject::connect(cmd, &Core::Command::keySequenceChanged, action, [cmd, action]() {
            action->setToolTip(cmd->stringWithAppendedShortcut(action->text()));
        });
    }
    return button;
}

namespace Help {
namespace Internal {

HelpWidget::HelpWidget(const Core::Context &context, WidgetStyle style, QWidget *parent) :
    QWidget(parent),
    m_scaleUp(0),
    m_scaleDown(0),
    m_resetScale(0),
    m_style(style)
{
    Utils::StyledBar *toolBar = new Utils::StyledBar();
    QHBoxLayout *layout = new QHBoxLayout(toolBar);
    layout->setSpacing(0);
    layout->setMargin(0);
    Core::Command *cmd;

    m_switchToHelp = new QAction(tr("Go to Help Mode"), toolBar);
    cmd = Core::ActionManager::registerAction(m_switchToHelp, Constants::CONTEXT_HELP, context);
    connect(m_switchToHelp, SIGNAL(triggered()), this, SLOT(helpModeButtonClicked()));
    layout->addWidget(toolButton(m_switchToHelp, cmd));

    m_backAction = new QAction(QIcon(QLatin1String(":/help/images/previous.png")),
        tr("Back"), toolBar);
    m_backMenu = new QMenu(toolBar);
    connect(m_backMenu, SIGNAL(aboutToShow()), this, SLOT(updateBackMenu()));
    m_backAction->setMenu(m_backMenu);
    cmd = Core::ActionManager::registerAction(m_backAction, Constants::HELP_PREVIOUS, context);
    cmd->setDefaultKeySequence(QKeySequence::Back);
    layout->addWidget(toolButton(m_backAction, cmd));

    m_forwardAction = new QAction(QIcon(QLatin1String(":/help/images/next.png")),
        tr("Forward"), toolBar);
    m_forwardMenu = new QMenu(toolBar);
    connect(m_forwardMenu, SIGNAL(aboutToShow()), this, SLOT(updateForwardMenu()));
    m_forwardAction->setMenu(m_forwardMenu);
    cmd = Core::ActionManager::registerAction(m_forwardAction, Constants::HELP_NEXT, context);
    cmd->setDefaultKeySequence(QKeySequence::Forward);
    layout->addWidget(toolButton(m_forwardAction, cmd));

    layout->addStretch();

    m_viewer = HelpPlugin::createHelpViewer(qreal(0.0));

    QVBoxLayout *vLayout = new QVBoxLayout(this);
    vLayout->setMargin(0);
    vLayout->setSpacing(0);
    vLayout->addWidget(toolBar);
    vLayout->addWidget(m_viewer);
    Core::FindToolBarPlaceHolder *fth = new Core::FindToolBarPlaceHolder(this);
    vLayout->addWidget(fth);

    setFocusProxy(m_viewer);

    m_context = new Core::IContext(this);
    m_context->setContext(context);
    m_context->setWidget(m_viewer);
    Core::ICore::addContextObject(m_context);

    m_backAction->setEnabled(m_viewer->isBackwardAvailable());
    connect(m_backAction, SIGNAL(triggered()), m_viewer, SLOT(backward()));
    connect(m_viewer, SIGNAL(backwardAvailable(bool)), m_backAction,
        SLOT(setEnabled(bool)));

    m_forwardAction->setEnabled(m_viewer->isForwardAvailable());
    connect(m_forwardAction, SIGNAL(triggered()), m_viewer, SLOT(forward()));
    connect(m_viewer, SIGNAL(forwardAvailable(bool)), m_forwardAction,
        SLOT(setEnabled(bool)));

    m_copy = new QAction(this);
    Core::ActionManager::registerAction(m_copy, Core::Constants::COPY, context);
    connect(m_copy, SIGNAL(triggered()), m_viewer, SLOT(copy()));

    Core::ActionContainer *advancedMenu = Core::ActionManager::actionContainer(Core::Constants::M_EDIT_ADVANCED);
    QTC_CHECK(advancedMenu);
    if (advancedMenu) {
        // reuse TextEditor constants to avoid a second pair of menu actions
        m_scaleUp = new QAction(tr("Increase Font Size"), this);
        cmd = Core::ActionManager::registerAction(m_scaleUp, TextEditor::Constants::INCREASE_FONT_SIZE,
                                                  context);
        connect(m_scaleUp, SIGNAL(triggered()), m_viewer, SLOT(scaleUp()));
        advancedMenu->addAction(cmd, Core::Constants::G_EDIT_FONT);

        m_scaleDown = new QAction(tr("Decrease Font Size"), this);
        cmd = Core::ActionManager::registerAction(m_scaleDown, TextEditor::Constants::DECREASE_FONT_SIZE,
                                                  context);
        connect(m_scaleDown, SIGNAL(triggered()), m_viewer, SLOT(scaleDown()));
        advancedMenu->addAction(cmd, Core::Constants::G_EDIT_FONT);

        m_resetScale = new QAction(tr("Reset Font Size"), this);
        cmd = Core::ActionManager::registerAction(m_resetScale, TextEditor::Constants::RESET_FONT_SIZE,
                                                  context);
        connect(m_resetScale, SIGNAL(triggered()), m_viewer, SLOT(resetScale()));
        advancedMenu->addAction(cmd, Core::Constants::G_EDIT_FONT);
    }

    if (style == SideBarWidget) {
        QAction *close = new QAction(QIcon(QLatin1String(Core::Constants::ICON_BUTTON_CLOSE)),
            QString(), toolBar);
        connect(close, SIGNAL(triggered()), this, SIGNAL(closeButtonClicked()));
        layout->addWidget(toolButton(close));
        m_viewer->setOpenInNewPageActionVisible(false);
    } else if (style == ExternalWindow) {
        static int windowId = 0;
        Core::ICore::registerWindow(this,
                                    Core::Context(Core::Id("Help.Window.").withSuffix(++windowId)));
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_QuitOnClose, false); // don't prevent Qt Creator from closing
        connect(m_viewer, SIGNAL(titleChanged()), this, SLOT(updateWindowTitle()));
        updateWindowTitle();
        m_viewer->setOpenInNewPageActionVisible(false);
    }
}

HelpWidget::~HelpWidget()
{
    Core::ICore::removeContextObject(m_context);
    Core::ActionManager::unregisterAction(m_copy, Core::Constants::COPY);
    Core::ActionManager::unregisterAction(m_switchToHelp, Constants::CONTEXT_HELP);
    Core::ActionManager::unregisterAction(m_forwardAction, Constants::HELP_NEXT);
    Core::ActionManager::unregisterAction(m_backAction, Constants::HELP_PREVIOUS);
    if (m_scaleUp)
        Core::ActionManager::unregisterAction(m_scaleUp, TextEditor::Constants::INCREASE_FONT_SIZE);
    if (m_scaleDown)
        Core::ActionManager::unregisterAction(m_scaleDown, TextEditor::Constants::DECREASE_FONT_SIZE);
    if (m_resetScale)
        Core::ActionManager::unregisterAction(m_resetScale, TextEditor::Constants::RESET_FONT_SIZE);
}

HelpViewer *HelpWidget::currentViewer() const
{
    return m_viewer;
}

void HelpWidget::closeEvent(QCloseEvent *)
{
    emit aboutToClose();
}

void HelpWidget::updateBackMenu()
{
    m_backMenu->clear();
    m_viewer->addBackHistoryItems(m_backMenu);
}

void HelpWidget::updateForwardMenu()
{
    m_forwardMenu->clear();
    m_viewer->addForwardHistoryItems(m_forwardMenu);
}

void HelpWidget::updateWindowTitle()
{
    const QString pageTitle = m_viewer->title();
    if (pageTitle.isEmpty())
        setWindowTitle(tr("Help"));
    else
        setWindowTitle(tr("Help - %1").arg(pageTitle));
}

void HelpWidget::helpModeButtonClicked()
{
    emit openHelpMode(m_viewer->source());
    if (m_style == ExternalWindow)
        close();
}

} // Internal
} // Help
