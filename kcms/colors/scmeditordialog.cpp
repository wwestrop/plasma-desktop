/* ColorEdit widget for KDE Display color scheme setup module
 * Copyright (C) 2016 Olivier Churlaud <olivier@churlaud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "scmeditordialog.h"
#include "scmeditoroptions.h"
#include "scmeditorcolors.h"
#include "scmeditoreffects.h"
#include "colorscm.h"

#include <QDebug>
#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QUrl>

#include <KConfigGroup>
#include <KColorScheme>
#include <KMessageBox>

#include <KNS3/UploadDialog>

SchemeEditorDialog::SchemeEditorDialog(KSharedConfigPtr config, KColorCm *parent)
    : QDialog( parent )
    , m_disableUpdates(false)
    , m_unsavedChanges(false)
    , m_kcm(parent)
{
    m_config = config;
    init();
}

SchemeEditorDialog::SchemeEditorDialog(const QString &path, KColorCm *parent)
    : QDialog( parent )
    , m_filePath(path)
    , m_disableUpdates(false)
    , m_unsavedChanges(false)
    , m_kcm(parent)
{
    m_config = KSharedConfig::openConfig(path);

    m_schemeName = KConfigGroup(m_config, "General").readEntry("Name");
    this->setWindowTitle(m_schemeName);
    init();
}

void SchemeEditorDialog::init()
{
    setupUi(this);

    //we are in standalone appication mode? don't show the apply button
    if (!m_kcm) {
        buttonBox->button(QDialogButtonBox::Apply)->setVisible(false);
    }

    schemeKnsUploadButton->setIcon( QIcon::fromTheme(QStringLiteral("get-hot-new-stuff")) );

    m_optionTab = new SchemeEditorOptions(m_config);
    m_colorTab = new SchemeEditorColors(m_config);
    m_disabledTab = new SchemeEditorEffects(m_config, QPalette::Disabled);
    m_inactiveTab = new SchemeEditorEffects(m_config, QPalette::Inactive);
    tabWidget->addTab(m_optionTab, i18n("Options"));
    tabWidget->addTab(m_colorTab, i18n("Colors"));
    tabWidget->addTab(m_disabledTab, i18n("Disabled"));
    tabWidget->setCurrentWidget(m_colorTab);

    connect(m_optionTab, &SchemeEditorOptions::changed, this, &SchemeEditorDialog::updateTabs);
    connect(m_colorTab, &SchemeEditorColors::changed, this, &SchemeEditorDialog::updateTabs);
    connect(m_disabledTab, &SchemeEditorEffects::changed, this, &SchemeEditorDialog::updateTabs);
    connect(m_inactiveTab, &SchemeEditorEffects::changed, this, &SchemeEditorDialog::updateTabs);

    buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
    updateTabs();
}

void SchemeEditorDialog::on_schemeKnsUploadButton_clicked()
{
    if (m_unsavedChanges)
    {
        KMessageBox::ButtonCode reallyUpload = KMessageBox::questionYesNo(
            this, i18n("This colour scheme was not saved. Continue?"),
            i18n("Do you really want to upload?"));
        if (reallyUpload == KMessageBox::No) {
            return;
        }
    }

    // upload
    KNS3::UploadDialog dialog(QStringLiteral("colorschemes.knsrc"), this);
    dialog.setUploadFile(QUrl::fromLocalFile(m_config->name()) );
    dialog.exec();
}

void SchemeEditorDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    if (buttonBox->standardButton(button) == QDialogButtonBox::Reset)
    {
        m_config->markAsClean();
        m_config->reparseConfiguration();
        updateTabs();
        setUnsavedChanges(false);
    }
    else if (buttonBox->standardButton(button) == QDialogButtonBox::Save)
    {
        saveScheme();
    }
    else if (buttonBox->standardButton(button) == QDialogButtonBox::Apply)
    {
        applyScheme();
        emit applied();
    }
    else if (buttonBox->standardButton(button) == QDialogButtonBox::Close)
    {
        if (m_unsavedChanges) {
            KMessageBox::ButtonCode ans = KMessageBox::questionYesNo(
                this, i18n("You have unsaved changes. Do you really want to quit?"),
                i18n("Unsaved changes"));
            if (ans == KMessageBox::No)
            {
                return;
            }
        }
        m_config->markAsClean();
        m_config->reparseConfiguration();
        this->accept();
    }
}

void SchemeEditorDialog::applyScheme()
{
    if (!m_kcm) {
        return;
    }

    m_kcm->updateConfig(m_config);
    m_kcm->save();
}

void SchemeEditorDialog::saveScheme()
{
    // prompt for the name to save as
    bool ok;
    QString schemeName = KConfigGroup(m_config, "General").readEntry("Name");
    QString name = QInputDialog::getText(this, i18n("Save Color Scheme"),
        i18n("&Enter a name for the color scheme:"), QLineEdit::Normal, m_schemeName, &ok);
    if (!ok) {
        return;
    }

    QString filename = name;
    filename.remove(QLatin1Char('\'')); // So Foo's does not become FooS
    QRegExp fixer(QStringLiteral("[\\W,.-]+(.?)"));
    int offset;
    while ((offset = fixer.indexIn(filename)) >= 0)
        filename.replace(offset, fixer.matchedLength(), fixer.cap(1).toUpper());
    filename.replace(0, 1, filename.at(0).toUpper());

    // check if that name is already in the list
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
        QStringLiteral("color-schemes/") + filename + QStringLiteral(".colors"));

    QFile file(path);
    const int permissions = file.permissions();
    const bool canWrite = (permissions & QFile::WriteUser);
    // or if we can overwrite it if it exists
    if (path.isEmpty() || !file.exists() || canWrite)
    {
        if(canWrite){
            int ret = KMessageBox::questionYesNo(this,
                i18n("A color scheme with that name already exists.\nDo you want to overwrite it?"),
                i18n("Save Color Scheme"),
                KStandardGuiItem::overwrite(),
                KStandardGuiItem::cancel());

            //on don't overwrite, call again the function
            if(ret == KMessageBox::No){
                this->saveScheme();
                return;
            }
        }

        // go ahead and save it
        QString newpath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                + "/color-schemes/";
        QDir dir;
        dir.mkpath(newpath);
        newpath += filename + ".colors";

        KConfig *config = m_config->copyTo(newpath);
        m_config->markAsClean();
        m_config->reparseConfiguration();
        KConfigGroup group(config, "General");
        group.writeEntry("Name", name);

        // sync it and delete pointer
        config->sync();
        delete config;
        // reopen and update window
        m_config = KSharedConfig::openConfig(newpath);
        m_schemeName = name;
        setWindowTitle(name);

        setUnsavedChanges(false);
    }
    else if (!canWrite && file.exists())
    {
        KMessageBox::error(this, i18n("You do not have permission to overwrite that scheme"), i18n("Error"));
    }
}

void SchemeEditorDialog::updateTabs(bool madeByUser)
{
    if (madeByUser)
    {
        setUnsavedChanges(true);
    }
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    bool showInactiveTab = group.readEntry("Enable", QVariant(true)).toBool();

    const int idx = tabWidget->indexOf(m_inactiveTab);

    if (showInactiveTab && idx == -1) {
        tabWidget->addTab(m_inactiveTab, i18n("Inactive"));
    } else if (!showInactiveTab && idx > -1) {
        tabWidget->removeTab(idx);
    }

    m_optionTab->updateValues();
    m_colorTab->updateValues();
    m_inactiveTab->updateValues();
    m_disabledTab->updateValues();
}

void SchemeEditorDialog::setUnsavedChanges(bool changes)
{
    m_unsavedChanges = changes;
    if (changes)
    {
        buttonBox->button(QDialogButtonBox::Save)->setEnabled(true);
        buttonBox->button(QDialogButtonBox::Reset)->setEnabled(true);
    }
    else
    {
        buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
        buttonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
    }
}
