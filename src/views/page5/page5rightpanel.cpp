#include "page5rightpanel.h"
#include "page5style.h"
#include "page5viewmodel.h"
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

Page5RightPanel::Page5RightPanel(Page5ViewModel* viewModel, QWidget* parent)
    : SidePanel(Qt::RightEdge, 260, parent), m_viewModel(viewModel)
{
    buildPanel();
}

void Page5RightPanel::buildPanel()
{
    auto* title = new QLabel(tr("Report"), this);
    title->setStyleSheet(Page5Style::PANEL_TITLE);
    contentLayout()->addWidget(title);

    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(10, 12, 10, 12);
    layout->addWidget(buildFileForm());
    contentLayout()->addWidget(body);
    contentLayout()->addStretch();
}

QWidget* Page5RightPanel::buildFileForm()
{
    auto* form = new QFrame(this);
    form->setObjectName("reportFileForm");
    form->setStyleSheet(Page5Style::REPORT_FORM);
    form->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(form);
    layout->setContentsMargins(12, 14, 12, 14);
    layout->setSpacing(8);

    auto* nameLabel = new QLabel(tr("&File name"), form);
    nameLabel->setProperty("fieldLabel", true);
    auto* nameEdit = new QLineEdit(form);
    nameEdit->setObjectName("reportNameEdit");
    nameEdit->setAccessibleName(tr("Report file name"));
    nameEdit->setPlaceholderText(tr("e.g. TestResults"));
    nameEdit->setClearButtonEnabled(true);
    nameEdit->setMinimumWidth(0);
    nameEdit->setMinimumHeight(34);
    nameLabel->setBuddy(nameEdit);
    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);

    layout->addSpacing(10);

    auto* folderLabel = new QLabel(tr("Save &folder"), form);
    folderLabel->setProperty("fieldLabel", true);
    auto* pathEdit = new QLineEdit(form);
    pathEdit->setObjectName("reportDirectoryEdit");
    pathEdit->setAccessibleName(tr("Report save folder"));
    pathEdit->setPlaceholderText(tr("Paste a folder path or browse"));
    pathEdit->setClearButtonEnabled(true);
    pathEdit->setMinimumWidth(0);
    pathEdit->setMinimumHeight(34);
    folderLabel->setBuddy(pathEdit);
    layout->addWidget(folderLabel);
    layout->addWidget(pathEdit);

    auto* browse = new QPushButton(tr("&Browse folder..."), form);
    browse->setObjectName("reportBrowseButton");
    browse->setMinimumHeight(34);
    browse->setCursor(Qt::PointingHandCursor);
    layout->addWidget(browse);

    auto syncFile = [this, nameEdit, pathEdit] {
        const QSignalBlocker nameBlock(nameEdit), pathBlock(pathEdit);
        nameEdit->setText(m_viewModel->reportName());
        pathEdit->setText(m_viewModel->reportDirectory());
        pathEdit->setCursorPosition(0);
        nameEdit->setToolTip(nameEdit->text());
        pathEdit->setToolTip(pathEdit->text());
    };
    syncFile();
    auto updateFile = [this, nameEdit, pathEdit] {
        m_viewModel->setReportFile(nameEdit->text(), pathEdit->text());
        nameEdit->setToolTip(nameEdit->text());
        pathEdit->setToolTip(pathEdit->text());
    };
    connect(nameEdit, &QLineEdit::textChanged, this, updateFile);
    connect(pathEdit, &QLineEdit::textChanged, this, updateFile);
    connect(m_viewModel, &Page5ViewModel::reportFileChanged, form, syncFile);
    connect(m_viewModel, &Page5ViewModel::runningChanged, form,
            [form](bool running) { form->setEnabled(!running); });
    form->setEnabled(!m_viewModel->isRunning());

    connect(browse, &QPushButton::clicked, this, [this, pathEdit] {
        const QString start = pathEdit->text().isEmpty() ? QDir::homePath() : pathEdit->text();
        const QString directory = QFileDialog::getExistingDirectory(
            this, tr("Select report folder"), start,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!directory.isEmpty()) {
            pathEdit->setText(directory);
            pathEdit->setCursorPosition(0);
        }
    });

    QWidget::setTabOrder(nameEdit, pathEdit);
    QWidget::setTabOrder(pathEdit, browse);
    return form;
}
