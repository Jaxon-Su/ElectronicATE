#pragma once
#include <QMessageBox>
#include <QPushButton>

inline bool confirmActiveOutputExit(QWidget* parent, bool unknown)
{
    QMessageBox dialog(QMessageBox::Warning,
                       unknown ? QObject::tr("Output state unknown") : QObject::tr("Instrument outputs active"),
                       QObject::tr("Instrument outputs are still ON or have not been confirmed OFF.\n"
                                   "Closing this application does not turn off instrument outputs.\n"
                                   "Check the instruments directly before leaving them unattended."),
                       QMessageBox::NoButton, parent);
    dialog.setObjectName("activeOutputExitDialog");
    auto* cancel = dialog.addButton(QObject::tr("Return to controls"), QMessageBox::RejectRole);
    auto* exit = dialog.addButton(QObject::tr("Close application"), QMessageBox::AcceptRole);
    dialog.setDefaultButton(cancel);
    dialog.setEscapeButton(cancel);
    dialog.exec();
    return dialog.clickedButton() == exit;
}
