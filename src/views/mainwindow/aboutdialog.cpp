#include "aboutdialog.h"
#include "applicationinfo.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("關於 ElectronicATE"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(16);

    auto *heading = new QHBoxLayout;
    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon(":/images/oscilloscope.png").pixmap(56, 56));
    heading->addWidget(icon);
    auto *title = new QLabel(QString::fromUtf8(ApplicationInfo::Name), this);
    auto titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    heading->addWidget(title, 1);
    layout->addLayout(heading);

    auto *details = new QFormLayout;
    details->addRow(tr("版本："), new QLabel("v" + QString::fromUtf8(ApplicationInfo::Version), this));
    details->addRow(tr("作者："), new QLabel(QString::fromUtf8(ApplicationInfo::Author), this));
    layout->addLayout(details);

    const QString url = QString::fromUtf8(ApplicationInfo::GitHubUrl);
    auto *github = new QLabel(QString("<a href=\"%1\">%1</a>").arg(url.toHtmlEscaped()), this);
    github->setObjectName("aboutGitHubLink");
    github->setTextInteractionFlags(Qt::TextBrowserInteraction);
    github->setOpenExternalLinks(true);
    github->setFocusPolicy(Qt::StrongFocus);
    github->setAccessibleName(tr("ElectronicATE GitHub 專案"));
    layout->addWidget(github);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("關閉"));
    auto *aboutQt = buttons->addButton(tr("關於 Qt"), QDialogButtonBox::HelpRole);
    connect(aboutQt, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
