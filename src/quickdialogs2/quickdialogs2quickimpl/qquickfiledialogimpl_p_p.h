/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKFILEDIALOG_P_P_H
#define QQUICKFILEDIALOG_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQuickTemplates2/private/qquickcombobox_p.h>
#include <QtQuickTemplates2/private/qquickdialog_p_p.h>
#include <QtQuickTemplates2/private/qquickdialogbuttonbox_p.h>

#include "qquickfiledialogimpl_p.h"

QT_BEGIN_NAMESPACE

class QQuickFileNameFilter;

class QQuickFileDialogImplPrivate : public QQuickDialogPrivate
{
public:
    Q_DECLARE_PUBLIC(QQuickFileDialogImpl)

    QQuickFileDialogImplPrivate();

    static QQuickFileDialogImplPrivate *get(QQuickFileDialogImpl *dialog)
    {
        return dialog->d_func();
    }

    QQuickFileDialogImplAttached *attachedOrWarn();

    void setNameFilters(const QStringList &filters);

    void updateEnabled();
    void updateCurrentFile(const QString &oldFolderPath);

    void handleAccept() override;
    void handleClick(QQuickAbstractButton *button) override;

    QSharedPointer<QFileDialogOptions> options;
    QUrl currentFolder;
    QUrl selectedFile;
    QUrl currentFile;
    QStringList nameFilters;
    mutable QQuickFileNameFilter *selectedNameFilter = nullptr;
    QString acceptLabel;
    QString rejectLabel;
};

class QQuickFileDialogImplAttachedPrivate : public QObjectPrivate
{
    void nameFiltersComboBoxItemActivated(int index);
    void fileDialogListViewCurrentIndexChanged();

public:
    Q_DECLARE_PUBLIC(QQuickFileDialogImplAttached)

    QPointer<QQuickDialogButtonBox> buttonBox;
    QPointer<QQuickComboBox> nameFiltersComboBox;
    QPointer<QQuickListView> fileDialogListView;
    QPointer<QQuickFolderBreadcrumbBar> breadcrumbBar;
};

QT_END_NAMESPACE

#endif // QQUICKFILEDIALOG_P_P_H
