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
    label->setProperty("role", "muted-label");
    layout->addWidget(label);

    bytesEdit_ = new QLineEdit(this);
    bytesEdit_->setPlaceholderText("e.g. 0x00, 0x12, 0x34");
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
