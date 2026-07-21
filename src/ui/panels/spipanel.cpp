#include "src/ui/panels/spipanel.h"
#include "src/ui/panels/byteparsing.h"
#include <QVBoxLayout>
#include <QLabel>

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
