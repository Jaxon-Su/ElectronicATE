#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

// Draft controls only; validation and all protocol work belong below the View.
class ModbusRequestEditor : public QWidget {
  public:
    explicit ModbusRequestEditor(QWidget *parent = nullptr) : QWidget(parent) {
        auto *layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_slave = new QSpinBox(this);
        m_slave->setObjectName("modbusSlave"); m_slave->setRange(1, 247);
        m_slave->setMinimumWidth(100);
        m_slave->setToolTip(tr("連線時帶入 Address 的 SLAVE；每次請求以此欄位為準"));
        m_function = new QComboBox(this);
        m_function->setObjectName("modbusFunction");
        for (const auto &entry : {qMakePair(1, "01 Read Coils"), qMakePair(2, "02 Read Discrete Inputs"),
                                 qMakePair(3, "03 Read Holding Registers"), qMakePair(4, "04 Read Input Registers"),
                                 qMakePair(5, "05 Write Single Coil"), qMakePair(6, "06 Write Single Register"),
                                 qMakePair(15, "0F Write Multiple Coils"), qMakePair(16, "10 Write Multiple Registers")})
            m_function->addItem(QString::fromLatin1(entry.second), entry.first);
        m_address = new QSpinBox(this);
        m_address->setObjectName("modbusAddress"); m_address->setRange(0, 65535);
        m_address->setDisplayIntegerBase(16); m_address->setPrefix("0x");
        m_address->setMinimumWidth(100);
        m_count = new QSpinBox(this);
        m_count->setObjectName("modbusQuantity"); m_count->setRange(1, 2000);
        m_values = new QLineEdit(this);
        m_values->setObjectName("modbusValues"); m_values->setMaxLength(14000);
        m_send = new QPushButton(tr("Send\n(Ctrl+Enter)"), this);
        m_send->setObjectName("modbusSend");
        m_send->setMinimumWidth(80);
        m_hint = new QLabel(this); m_hint->setWordWrap(true);
        layout->addWidget(new QLabel(tr("Slave ID:"), this), 0, 0);
        layout->addWidget(m_slave, 0, 1);
        layout->addWidget(new QLabel(tr("Function:"), this), 0, 2);
        layout->addWidget(m_function, 0, 3, 1, 2);
        layout->addWidget(new QLabel(tr("Start address (hex):"), this), 1, 0);
        layout->addWidget(m_address, 1, 1);
        layout->addWidget(new QLabel(tr("Quantity:"), this), 1, 2);
        layout->addWidget(m_count, 1, 3);
        layout->addWidget(new QLabel(tr("Write values:"), this), 2, 0);
        layout->addWidget(m_values, 2, 1, 1, 3);
        layout->addWidget(m_send, 1, 4, 2, 1);
        layout->addWidget(m_hint, 3, 0, 1, 5);
        layout->setColumnStretch(3, 1);
        connect(m_function, &QComboBox::currentIndexChanged, this, [this] { updateFields(); });
        updateFields();
    }
    int slave() const { return m_slave->value(); }
    int function() const { return m_function->currentData().toInt(); }
    int address() const { return m_address->value(); }
    int count() const { return m_count->value(); }
    QString values() const { return function() <= 4 ? QString{} : m_values->text(); }
    void setSlave(int slave) { m_slave->setValue(slave); }
    QPushButton *sendButton() const { return m_send; }
  private:
    void updateFields() {
        const int code = function();
        const bool read = code <= 4, coil = code == 5 || code == 15;
        const int limit = code <= 2 ? 2000 : code <= 4 ? 125 : code <= 6 ? 1 : code == 15 ? 1968 : 123;
        m_count->setRange(1, limit); m_count->setEnabled(limit != 1);
        m_values->setEnabled(!read);
        m_values->setPlaceholderText(read ? tr("讀取功能不需填寫") : coil ? tr("0 = OFF、1 = ON；多筆以空白或逗號分隔")
                                        : tr("十進位或 0x 十六進位，例如 100, 0x1234"));
        m_hint->setText(tr("位址從 0 起算，請填協定位址（非 4xxxx 編號）；CRC 自動計算。")
                       + (read ? tr("讀取結果會列出位址與數值。") : tr("寫入值的筆數須等於 Quantity，並等待設備確認回應。")));
    }
    QSpinBox *m_slave, *m_address, *m_count;
    QComboBox *m_function;
    QLineEdit *m_values;
    QPushButton *m_send;
    QLabel *m_hint;
};
