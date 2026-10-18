#include "waveformocrpage.h"
#include "../../viewmodels/waveformocr/waveformocrviewmodel.h"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

WaveformOcrPage::WaveformOcrPage(WaveformOcrViewModel *viewModel, QWidget *parent) : QWidget(parent), m_vm(viewModel)
{
    setObjectName("waveformOcr");
    auto *layout = new QVBoxLayout(this);
    auto *settings = new QGridLayout;
    m_folder = new QLineEdit(this);
    m_folder->setObjectName("ocrFolder");
    m_folder->setPlaceholderText("圖片資料夾（包含子資料夾）");
    m_browse = new QPushButton("選擇資料夾…", this);
    settings->addWidget(new QLabel("圖片來源", this), 0, 0);
    settings->addWidget(m_folder, 0, 1);
    settings->addWidget(m_browse, 0, 2);
    m_models = new QComboBox(this);
    m_models->setObjectName("ocrModel");
    m_models->setEditable(true);
    m_models->addItem(m_vm->defaultModelName());
    m_models->setMinimumWidth(100);
    m_models->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_connect = new QPushButton("連線／更新模型", this);
    m_connect->setObjectName("ocrConnect");
    settings->addWidget(new QLabel("視覺模型", this), 1, 0);
    settings->addWidget(m_models, 1, 1);
    settings->addWidget(m_connect, 1, 2);
    settings->setColumnStretch(1, 1);
    layout->addLayout(settings);
    auto *actions = new QHBoxLayout;
    m_start = new QPushButton("開始批次辨識", this);
    m_start->setObjectName("ocrStart");
    m_stop = new QPushButton("停止", this);
    m_stop->setObjectName("ocrStop");
    m_export = new QPushButton("匯出 Excel…", this);
    m_export->setObjectName("ocrExport");
    m_edit = new QPushButton("修改數值", this);
    m_edit->setObjectName("ocrEdit");
    auto *metadata = new QCheckBox("顯示通道／統計範圍", this);
    metadata->setObjectName("ocrMetadata");
    m_progress = new QProgressBar(this);
    m_progress->setMinimumWidth(80);
    m_progress->setFormat("%v / %m");
    actions->addWidget(m_start);
    actions->addWidget(m_stop);
    actions->addWidget(m_edit);
    actions->addWidget(m_export);
    actions->addWidget(metadata);
    actions->addWidget(m_progress, 1);
    layout->addLayout(actions);
    m_status = new QLabel(this);
    m_status->setObjectName("ocrStatus");
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);

    auto *split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    auto *upper = new QSplitter(Qt::Horizontal, split);
    upper->setChildrenCollapsible(false);
    auto *imagePanel = new QWidget(upper);
    auto *imageLayout = new QVBoxLayout(imagePanel);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    auto *imageTools = new QHBoxLayout;
    m_imagePath = new QLabel("選取結果列以預覽原圖", imagePanel);
    m_imagePath->setTextFormat(Qt::PlainText);
    m_imagePath->setMinimumWidth(0);
    auto *fit = new QPushButton("適合視窗", imagePanel), *zoomIn = new QPushButton("＋", imagePanel),
         *zoomOut = new QPushButton("－", imagePanel);
    zoomIn->setFixedWidth(34);
    zoomOut->setFixedWidth(34);
    imageTools->addWidget(m_imagePath, 1);
    imageTools->addWidget(zoomOut);
    imageTools->addWidget(zoomIn);
    imageTools->addWidget(fit);
    imageLayout->addLayout(imageTools);
    m_scene = new QGraphicsScene(this);
    m_preview = new QGraphicsView(m_scene, imagePanel);
    m_preview->setObjectName("ocrPreview");
    m_preview->setBackgroundBrush(QColor("#20252c"));
    m_preview->setDragMode(QGraphicsView::ScrollHandDrag);
    m_preview->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_preview->setMinimumSize(200, 120);
    imageLayout->addWidget(m_preview);
    auto *detailsTabs = new QTabWidget(upper);
    detailsTabs->setMinimumWidth(160);
    m_details = new QPlainTextEdit(detailsTabs);
    m_details->setReadOnly(true);
    m_details->setObjectName("ocrDetails");
    m_raw = new QPlainTextEdit(detailsTabs);
    m_raw->setReadOnly(true);
    m_raw->setObjectName("ocrRawResponse");
    detailsTabs->addTab(m_details, "圖片資訊");
    detailsTabs->addTab(m_raw, "原始辨識文字");
    upper->setSizes({850, 320});
    m_table = new QTableView(split);
    m_table->setObjectName("ocrResults");
    m_table->setModel(m_vm->results());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->setAlternatingRowColors(true);
    m_table->setMinimumHeight(120);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    const int widths[] = {230, 100, 145, 120, 120, 120, 120, 65, 175, 250};
    for (int i = 0; i < OcrResultsModel::Count; ++i)
        m_table->setColumnWidth(i, widths[i]);
    m_table->setColumnHidden(OcrResultsModel::Channel, true);
    m_table->setColumnHidden(OcrResultsModel::Scope, true);
    split->setSizes({360, 230});
    layout->addWidget(split, 1);
    auto *hint = new QLabel(
        "雙擊數值或按 F2 修改（例如 5.1 V、-185.3 mA）；修改後按 Enter 儲存。"
        "—／黃色底色表示缺值或格式錯誤。「已核對」及通道資訊為選填，不影響匯出。", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(metadata, &QCheckBox::toggled, this, [this](bool visible) {
        m_table->setColumnHidden(OcrResultsModel::Channel, !visible);
        m_table->setColumnHidden(OcrResultsModel::Scope, !visible);
    });
    connect(m_edit, &QPushButton::clicked, this, [this] {
        const auto current = m_table->currentIndex();
        if (!current.isValid())
            return;
        const int column = current.column() >= OcrResultsModel::Max && current.column() <= OcrResultsModel::Mean
                               ? current.column() : OcrResultsModel::Max;
        const auto index = m_vm->results()->index(current.row(), column);
        m_table->setCurrentIndex(index);
        m_table->edit(index);
    });

    connect(m_browse, &QPushButton::clicked, this,
            [this]
            {
                const auto path = QFileDialog::getExistingDirectory(this, "選擇圖片來源", m_folder->text());
                if (!path.isEmpty())
                    m_folder->setText(QDir::toNativeSeparators(path));
            });
    connect(m_connect, &QPushButton::clicked, this,
            [this] { m_vm->refreshModels(); });
    connect(m_start, &QPushButton::clicked, this,
            [this]
            {
                if (m_vm->hasUnexportedResults() &&
                    QMessageBox::question(
                        this, "開始新批次", "目前結果尚未匯出。開始新批次會清除這些結果，是否繼續？",
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                    return;
                m_vm->start(m_folder->text().trimmed(), m_models->currentText());
            });
    connect(m_stop, &QPushButton::clicked, m_vm, &WaveformOcrViewModel::stop);
    connect(m_export, &QPushButton::clicked, this,
            [this]
            {
                const auto path = QFileDialog::getSaveFileName(this, "匯出 Excel（同名自動加流水號）",
                                                               m_vm->suggestedExportPath(), "Excel (*.xlsx)", nullptr,
                                                               QFileDialog::DontConfirmOverwrite);
                if (!path.isEmpty())
                    m_vm->exportResults(path);
            });
    connect(m_vm, &WaveformOcrViewModel::changed, this, &WaveformOcrPage::updateState);
    connect(m_vm, &WaveformOcrViewModel::modelNamesChanged, this,
            [this](const QStringList &names)
            {
                const auto current = m_models->currentText();
                m_models->clear();
                m_models->addItems(names);
                m_models->setCurrentText(current);
            });
    connect(m_vm, &WaveformOcrViewModel::imageStarted, this,
            [this](const QString &path)
            {
                // Keep the image paired with the selected result during a running batch.
                if (!m_table->currentIndex().isValid())
                    preview(path);
            });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &WaveformOcrPage::showSelection);
    connect(m_vm->results(), &QAbstractItemModel::dataChanged, this, &WaveformOcrPage::showSelection);
    connect(m_vm->results(), &QAbstractItemModel::modelReset, this,
            [this]
            {
                m_scene->clear();
                m_currentPath.clear();
                m_details->clear();
                m_raw->clear();
            });
    connect(fit, &QPushButton::clicked, this, &WaveformOcrPage::fitImage);
    connect(zoomIn, &QPushButton::clicked, this,
            [this]
            {
                if (m_preview->transform().m11() < 16)
                    m_preview->scale(1.25, 1.25);
            });
    connect(zoomOut, &QPushButton::clicked, this,
            [this]
            {
                if (m_preview->transform().m11() > 0.02)
                    m_preview->scale(0.8, 0.8);
            });
    updateState();
}
void WaveformOcrPage::finishEditing()
{
    // A window-close event may arrive before an active cell editor loses focus.
    // Let the normal delegate commit it before checking dirty/exporting a snapshot.
    if (auto *editor = QApplication::focusWidget(); editor && m_table->isAncestorOf(editor))
        m_table->setFocus(Qt::OtherFocusReason);
}

void WaveformOcrPage::updateState()
{
    const bool idle = !m_vm->busy();
    for (auto *widget : QList<QWidget *>{m_folder, m_models, m_browse, m_connect, m_start})
        widget->setEnabled(idle);
    m_stop->setEnabled(!idle);
    m_export->setEnabled(idle && m_vm->results()->rowCount() > 0);
    m_edit->setEnabled(idle && m_vm->results()->rowCount() > 0);
    m_status->setText(m_vm->message());
    m_progress->setRange(0, qMax(1, m_vm->total()));
    m_progress->setValue(m_vm->completed());
    if (idle && m_vm->results()->rowCount() > 0 && !m_table->currentIndex().isValid())
        m_table->selectRow(0);
}
void WaveformOcrPage::showSelection()
{
    const auto selection = m_vm->selection(m_table->currentIndex().row());
    if (selection.imagePath.isEmpty())
        return;
    preview(selection.imagePath);
    m_details->setPlainText(selection.details);
    m_raw->setPlainText(selection.rawResponse);
}
void WaveformOcrPage::preview(const QString &path)
{
    if (m_currentPath == path)
        return;
    m_currentPath = path;
    m_scene->clear();
    m_preview->resetTransform();
    m_imagePath->setText(QFileInfo(path).fileName());
    m_imagePath->setToolTip(QDir::toNativeSeparators(path));
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 40000000)
    {
        m_scene->addText("圖片格式無效或尺寸過大")->setDefaultTextColor(Qt::white);
        return;
    }
    if (size.width() > 4096 || size.height() > 4096)
        reader.setScaledSize(size.scaled(4096, 4096, Qt::KeepAspectRatio));
    const auto image = reader.read();
    if (image.isNull())
    {
        m_scene->addText("無法預覽圖片: " + reader.errorString())->setDefaultTextColor(Qt::white);
        return;
    }
    m_scene->addPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(QRectF(QPointF(0, 0), image.size()));
    QTimer::singleShot(0, this, &WaveformOcrPage::fitImage);
}
void WaveformOcrPage::fitImage()
{
    if (!m_scene->items().isEmpty())
        m_preview->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}
