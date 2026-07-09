#include "src/ui/spipanel.h"
#include <QVBoxLayout>
#include <QLabel>

static std::vector<uint8_t> parseByteList(const QString& text) {
    std::vector<uint8_t> bytes;
    for (const QString& tok : text.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        int v = tok.trimmed().toInt(&ok, 0); // base 0 auto-detects "0x.." / decimal
        if (ok)
            bytes.push_back((uint8_t)(v & 0xFF));
    }
    return bytes;
}

SpiPanel::SpiPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel* label = new QLabel(
        "Response bytes (comma-separated; SPI.transfer() cycles through these)", this);
    label->setStyleSheet("color: #888;");
    layout->addWidget(label);

    bytesEdit_ = new QLineEdit(this);
    bytesEdit_->setPlaceholderText("e.g. 0x00, 0x12, 0x34");
    bytesEdit_->setStyleSheet(
        "QLineEdit { background: #1e1e1e; color: #d4d4d4;"
        "border: 1px solid #3c3c3c; font-family: 'Courier New'; font-size: 12px;"
        "padding: 4px; }"
    );
    connect(bytesEdit_, &QLineEdit::textChanged, this, &SpiPanel::onTextChanged);
    layout->addWidget(bytesEdit_);
    layout->addStretch();
}

void SpiPanel::onTextChanged(const QString& text) {
    emit bytesChanged(parseByteList(text));
}

void SpiPanel::pushAll() {
    emit bytesChanged(parseByteList(bytesEdit_->text()));
}
