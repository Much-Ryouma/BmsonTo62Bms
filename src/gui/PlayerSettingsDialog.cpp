#include "PlayerSettingsDialog.hpp"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

#include "QtText.hpp"

namespace b2b::gui {

namespace {

QWidget* withBrowseButton(QLineEdit* edit, const std::function<void()>& browse, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);
    auto* button = new QPushButton(QStringLiteral("参照…"), row);
    QObject::connect(button, &QPushButton::clicked, row, browse);
    layout->addWidget(button);
    return row;
}

}  // namespace

PlayerSettingsDialog::PlayerSettingsDialog(const app::PlayerSettings& settings, QWidget* parent)
    : QDialog(parent),
      executable_(new QLineEdit(toQString(settings.executable), this)),
      commandLine_(new QLineEdit(toQString(settings.commandTemplate), this)),
      workingDirectory_(new QLineEdit(toQString(settings.workingDirectory), this)) {
    setWindowTitle(QStringLiteral("外部プレイヤーの設定"));
    executable_->setObjectName(QStringLiteral("playerExecutable"));
    commandLine_->setObjectName(QStringLiteral("playerCommandLine"));
    workingDirectory_->setObjectName(QStringLiteral("playerWorkingDirectory"));
    workingDirectory_->setPlaceholderText(QStringLiteral("空欄なら実行ファイルのフォルダ"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("実行ファイル"), withBrowseButton(executable_, [this] {
                     const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("外部プレイヤーの実行ファイル"),
                                                                       executable_->text());
                     if (!file.isEmpty()) {
                         executable_->setText(QDir::toNativeSeparators(file));
                     }
                 }, this));
    form->addRow(QStringLiteral("コマンドライン"), commandLine_);
    form->addRow(QStringLiteral("作業フォルダ"), withBrowseButton(workingDirectory_, [this] {
                     const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("作業フォルダ"),
                                                                              workingDirectory_->text());
                     if (!folder.isEmpty()) {
                         workingDirectory_->setText(QDir::toNativeSeparators(folder));
                     }
                 }, this));

    auto* help = new QLabel(QStringLiteral(
        "コマンドラインの {exe} は実行ファイル、{file} は書き出した BMS のパスに置き換わります。\n"
        "既定は \"{exe}\" \"{file}\" です。オートプレイなどのオプションはプレイヤーの説明を見て足してください。\n"
        "beatoraja などは起動したフォルダから設定を読むため、作業フォルダは空欄（実行ファイルのフォルダ）をおすすめします。"),
        this);
    help->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(help);
    layout->addWidget(buttons);
    resize(640, sizeHint().height());
}

app::PlayerSettings PlayerSettingsDialog::settings() const {
    app::PlayerSettings settings;
    settings.executable = toUtf8(executable_->text().trimmed());
    settings.commandTemplate = toUtf8(commandLine_->text().trimmed());
    settings.workingDirectory = toUtf8(workingDirectory_->text().trimmed());
    return settings;
}

}  // namespace b2b::gui
