#include "vogleditor_qsettingsdialog.h"
#include "ui_vogleditor_qsettingsdialog.h"

#include "vogleditor_qsettings.h"

vogleditor_QSettingsDialog::vogleditor_QSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::vogleditor_QSettingsDialog)
{
    ui->setupUi(this);

    // tab parent
    ui->tabWidget->setCurrentIndex(g_settings.tab_page());

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(tabCB(int)));

    connect(ui->buttonBox, SIGNAL(accepted()), SLOT(acceptCB()));
    connect(ui->buttonBox, SIGNAL(rejected()), SLOT(cancelCB()));

    connect(this, &vogleditor_QSettingsDialog::settingsChanged, &g_settings, &vogleditor_qsettings::update_group_active_lists);

    // Startup settings tab
    QString strSettings = g_settings.to_string();

    ui->textEdit->setText(strSettings);

    // Group settings tab

    // State Render
    ui->checkboxStateRender->setText(g_settings.group_state_render_name());
    ui->checkboxStateRender->setChecked(g_settings.group_state_render_stat());
    ui->checkboxStateRender->setEnabled(g_settings.group_state_render_used());

    connect(ui->checkboxStateRender, SIGNAL(stateChanged(int)),
            SLOT(checkboxStateRenderCB(int)));

    // Debug markers
    QVector<QCheckBox *> checkboxList;

    QStringList group_debug_marker_names = g_settings.group_debug_marker_name_list();
    QVector<bool> debug_marker_stat = g_settings.group_debug_marker_stat_list();
    QVector<bool> debug_marker_used = g_settings.group_debug_marker_used_list();
    int debug_marker_cnt = group_debug_marker_names.size();
    for (int i = 0; i < debug_marker_cnt; i++)
    {
        checkboxList << new QCheckBox(group_debug_marker_names[i], ui->groupboxDebugMarker);
        checkboxList[i]->setChecked(debug_marker_stat[i]);
        checkboxList[i]->setEnabled(debug_marker_used[i]);
        ui->vLayout_groupboxDebugMarker->insertWidget(i, checkboxList[i]);
    }

    for (int i = 0; i < debug_marker_cnt; i++)
    {
        connect(checkboxList[i], SIGNAL(stateChanged(int)),
                SLOT(checkboxCB(int)));
    }

    // Debug marker options
    ui->radiobuttonDebugMarkerNameOption->setText(g_settings.group_debug_marker_option_name_label());
    ui->radiobuttonDebugMarkerNameOption->setChecked(g_settings.group_debug_marker_option_name_stat());

    ui->radiobuttonDebugMarkerOmitOption->setText(g_settings.group_debug_marker_option_omit_label());
    ui->radiobuttonDebugMarkerOmitOption->setChecked(g_settings.group_debug_marker_option_omit_stat());

    setEnableDebugMarkerOptions();

    connect(ui->radiobuttonDebugMarkerNameOption, SIGNAL(toggled(bool)),
            SLOT(radiobuttonNameCB(bool)));
    connect(ui->radiobuttonDebugMarkerOmitOption, SIGNAL(toggled(bool)),
            SLOT(radiobuttonOmitCB(bool)));

    // Nest options
    checkboxList.clear();

    // Groupbox
    ui->groupboxNestOptions->setTitle(g_settings.groupbox_nest_options_name());
    ui->groupboxNestOptions->setChecked(g_settings.groupbox_nest_options_stat());
    ui->groupboxNestOptions->setEnabled(g_settings.groupbox_nest_options_used());
    if (g_settings.groupbox_nest_options_stat())
    {
        // For now, toggle State/Render groups and Nest options
        checkboxStateRenderCB(false);
    }

    // Checkboxes
    QStringList group_nest_options_names = g_settings.group_nest_options_name_list();
    QVector<bool> nest_options_stat = g_settings.group_nest_options_stat_list();
    QVector<bool> nest_options_used = g_settings.group_nest_options_used_list();
    int nest_options_cnt = group_nest_options_names.size();
    for (int i = 0; i < nest_options_cnt; i++)
    {
        checkboxList << new QCheckBox(group_nest_options_names[i], ui->groupboxNestOptions);
        checkboxList[i]->setChecked(nest_options_stat[i]);
        checkboxList[i]->setEnabled(nest_options_used[i]);
        ui->vLayout_groupboxNestOptionsScrollarea->addWidget(checkboxList[i]);
    }

    connect(ui->groupboxNestOptions, SIGNAL(toggled(bool)), // groupbox
            SLOT(groupboxNestOptionsCB(bool)));

    for (int i = 0; i < nest_options_cnt; i++) // checkboxes in groupbox
    {
        connect(checkboxList[i], SIGNAL(stateChanged(int)),
                SLOT(checkboxCB(int)));
    }

    // Save initial state
    m_bGroupInitialState = groupState();

} // constructor

vogleditor_QSettingsDialog::~vogleditor_QSettingsDialog()
{
    clearLayout(ui->verticalLayout_tabGroups);
    delete ui;
} // destructor

void vogleditor_QSettingsDialog::clearLayout(QLayout *layout)
{
    // taken from
    // http://stackoverflow.com/questions/4272196/qt-remove-all-widgets-from-layout
    // ... didn't seem to make any difference using valgrind Memcheck...

    while (QLayoutItem *item = layout->takeAt(0))
    {
        delete item->widget();
        if (QLayout *childLayout = item->layout())
            clearLayout(childLayout);
        delete item;
    }
} // clearLayout

void vogleditor_QSettingsDialog::tabCB(int page)
{
    g_settings.set_tab_page(page);
}

void vogleditor_QSettingsDialog::checkboxStateRenderCB(int val)
{
    bool bVal = bool(val);
    if (bVal)
    {
        ui->groupboxNestOptions->setChecked(!bVal);
        g_settings.set_groupbox_nest_options_stat(!bVal);
    }
    // Update
    g_settings.set_group_state_render_stat(bVal);
    updateTextTab();

} // checkboxStateRenderCB

void vogleditor_QSettingsDialog::groupboxNestOptionsCB(bool bVal)
{
    if (bVal)
    {
        ui->checkboxStateRender->setChecked(!bVal);
        g_settings.set_group_state_render_stat(!bVal);
    }
    // Update
    g_settings.set_groupbox_nest_options_stat(bVal);
    updateTextTab();

} // groupboxNestOptionsCB

void vogleditor_QSettingsDialog::checkboxCB(int state)
{
    g_settings.set_group_state_render_stat(ui->checkboxStateRender->isChecked());
    g_settings.set_group_debug_marker_stat_list(checkboxValues(ui->groupboxDebugMarker));
    g_settings.set_group_nest_options_stat_list(checkboxValues(ui->groupboxNestOptions));

    setEnableDebugMarkerOptions();

    updateTextTab();

} // checkboxCB

void vogleditor_QSettingsDialog::setEnableDebugMarkerOptions()
{
    //  Disable options if no Debug marker groups are checked
    QVector<bool> bCurrentState = checkboxValues(ui->groupboxDebugMarker);

    bool bVal;
    foreach(bVal, bCurrentState)
    {
        if (bVal)
            break;
    }
    enableDebugMarkerOptions(bVal);
}
void vogleditor_QSettingsDialog::enableDebugMarkerOptions(bool bVal)
{
    ui->radiobuttonDebugMarkerNameOption->setEnabled(bVal);
    ui->radiobuttonDebugMarkerOmitOption->setEnabled(bVal);

    // Notify g_settings
    g_settings.set_group_debug_marker_option_name_used(bVal);
    g_settings.set_group_debug_marker_option_omit_used(bVal);
}

void vogleditor_QSettingsDialog::radiobuttonNameCB(bool state)
{
    g_settings.set_group_debug_marker_option_name_stat(state);
    updateTextTab();
}

void vogleditor_QSettingsDialog::radiobuttonOmitCB(bool state)
{
    g_settings.set_group_debug_marker_option_omit_stat(state);
    updateTextTab();
}

void vogleditor_QSettingsDialog::updateTextTab()
{
    //update json tab settings page
    QString strSettings = g_settings.to_string();
    ui->textEdit->setText(strSettings);
}

QVector<bool> vogleditor_QSettingsDialog::checkboxValues(QObject *widget)
{
    // Note: This function is intended to only return checkbox values of
    //       API call names, so for the Debug marker API call list parent
    //       radiobuttons were used for the options.
    QList<QCheckBox *> groupCheckBoxes = widget->findChildren<QCheckBox *>();

    QVector<bool> bQVector;
    QList<QCheckBox *>::const_iterator iter = groupCheckBoxes.begin();

    while (iter != groupCheckBoxes.end())
    {
        bQVector << (*iter)->isChecked();
        iter++;
    }
    return bQVector;

} // checkboxValues()

QVector<bool> vogleditor_QSettingsDialog::groupState()
{
    QVector<bool> bCurrentState;

    bCurrentState << ui->checkboxStateRender->isChecked();
    bCurrentState << ui->groupboxNestOptions->isChecked();

    bCurrentState << checkboxValues(ui->groupboxDebugMarker);
    bCurrentState << checkboxValues(ui->groupboxNestOptions);

    bCurrentState << ui->radiobuttonDebugMarkerNameOption->isChecked();
    bCurrentState << ui->radiobuttonDebugMarkerOmitOption->isChecked();

    return bCurrentState;

} // groupState

void vogleditor_QSettingsDialog::reset()
{
    if (groupOptionsChanged())
    {
        // Set in same order as saved (from ::groupState())
        //
        // TODO: QMap these, widget key to value, in constructor
        //       so order won't matter
        int i = 0;
        ui->checkboxStateRender->setChecked(m_bGroupInitialState[i++]);
        ui->groupboxNestOptions->setChecked(m_bGroupInitialState[i++]);

        QList<QCheckBox *> checkboxes = ui->groupboxDebugMarker->findChildren<QCheckBox *>();
        for (int indx = 0; indx < checkboxes.count(); indx++)
        {
            checkboxes[indx]->setChecked(m_bGroupInitialState[i++]);
        }
        checkboxes = ui->groupboxNestOptions->findChildren<QCheckBox *>();
        for (int indx = 0; indx < checkboxes.count(); indx++)
        {
            checkboxes[indx]->setChecked(m_bGroupInitialState[i++]);
        }

        ui->radiobuttonDebugMarkerNameOption->setChecked(m_bGroupInitialState[i++]);
        ui->radiobuttonDebugMarkerOmitOption->setChecked(m_bGroupInitialState[i++]);
    }
} // reset

bool vogleditor_QSettingsDialog::groupOptionsChanged()
{
    return m_bGroupInitialState != groupState();
}

void vogleditor_QSettingsDialog::acceptCB()
{
    save();

    if (groupOptionsChanged())
    {
        emit settingsChanged();
    }
}

void vogleditor_QSettingsDialog::cancelCB()
{
    reset();
}

void vogleditor_QSettingsDialog::save()
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save();
}
