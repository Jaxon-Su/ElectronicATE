#include "genericoscwritedialog.h"
#include "oscshareddata.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QCheckBox>
#include <QComboBox>

using namespace OscSharedData;

GenericOscWriteDialog::GenericOscWriteDialog(Oscilloscope*      scope,
                                             const QString&     configuredModel,
                                             const QVariantMap& initCfg,
                                             int                seqNo,
                                             const QString&     extName,
                                             QWidget*           parent)
    : OscWriteDialogBase(scope, configuredModel, initCfg, seqNo, extName, parent)
{
    buildFrame({480, 320}, true);
}

QWidget* GenericOscWriteDialog::buildContentWidget()
{
    auto* card = new QFrame;
    card->setObjectName("card");
    auto* vlay = new QVBoxLayout(card);
    vlay->setContentsMargins(12, 8, 12, 8);

    auto* infoLbl = new QLabel(
        QString("⚠  Model \"%1\" has no dedicated UI.\n"
                "Using generic oscilloscope settings.").arg(m_model));
    infoLbl->setStyleSheet("font-size: 12px; color: #c07000; padding: 6px;");
    infoLbl->setWordWrap(true);
    vlay->addWidget(infoLbl);

    auto* grp  = new QGroupBox("Basic Settings");
    auto* wrap = new QVBoxLayout(grp);
    wrap->setContentsMargins(8, 2, 8, 4);
    wrap->setSpacing(2);

    m_basicIgnore = new QCheckBox("Ignore");
    m_basicIgnore->setChecked(m_cfg.value("ignore_basic", false).toBool());
    m_basicIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
    wrap->addWidget(m_basicIgnore, 0, Qt::AlignRight);

    auto* basicContent = new QWidget;
    auto* form         = new QFormLayout(basicContent);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);

    m_timescale = makeCombo(TIMESCALES, m_cfg.value("timescale", "200ms").toString());
    m_acqMode   = makeCombo(ACQ_MODES,  m_cfg.value("acq_mode",  "PEAKdetect").toString());
    form->addRow("Timescale:", m_timescale);
    form->addRow("Acq Mode:", m_acqMode);

    wrap->addWidget(basicContent);
    basicContent->setEnabled(!m_basicIgnore->isChecked());
    connect(m_basicIgnore, &QCheckBox::toggled, basicContent,
            [basicContent](bool ig){ basicContent->setEnabled(!ig); });

    vlay->addWidget(grp);

    // ── Trigger ─────────────────────────────────────
    auto* trigGrp  = new QGroupBox("Trigger (Edge)");
    auto* trigWrap = new QVBoxLayout(trigGrp);
    trigWrap->setContentsMargins(8, 2, 8, 4);
    trigWrap->setSpacing(2);

    m_trigIgnore = new QCheckBox("Ignore");
    m_trigIgnore->setChecked(m_cfg.value("ignore_trigger", false).toBool());
    m_trigIgnore->setStyleSheet("font-size:10px; color:#aa6600;");
    trigWrap->addWidget(m_trigIgnore, 0, Qt::AlignRight);

    auto* trigContent = new QWidget;
    auto* trigForm    = new QFormLayout(trigContent);
    trigForm->setLabelAlignment(Qt::AlignRight);
    trigForm->setHorizontalSpacing(12);
    trigForm->setVerticalSpacing(6);

    m_trigSource = makeCombo(TRIG_SOURCES_DPO,
                             m_cfg.value("trig_source", "CH1").toString());
    m_trigEdge   = makeCombo(TRIG_EDGES,
                             m_cfg.value("trig_edge",   "Rising").toString());
    trigForm->addRow("Source:", m_trigSource);
    trigForm->addRow("Edge:",   m_trigEdge);

    trigWrap->addWidget(trigContent);
    trigContent->setEnabled(!m_trigIgnore->isChecked());
    connect(m_trigIgnore, &QCheckBox::toggled, trigContent,
            [trigContent](bool ig){ trigContent->setEnabled(!ig); });

    vlay->addWidget(trigGrp);
    vlay->addStretch();
    return card;
}

void GenericOscWriteDialog::saveConfig()
{
    if (!m_timescale) return;
    m_cfg["ignore_basic"]   = m_basicIgnore->isChecked();
    m_cfg["ignore_trigger"] = m_trigIgnore->isChecked();
    m_cfg["timescale"]      = m_timescale->currentText();
    m_cfg["acq_mode"]       = m_acqMode->currentText();
    m_cfg["trig_source"]    = m_trigSource->currentText();
    m_cfg["trig_edge"]      = m_trigEdge->currentText();
}
