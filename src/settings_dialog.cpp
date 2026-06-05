#include "settings_dialog.h"

#include "index_worker.h"
#include "root_policy.h"

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace wensousou {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("索引设置"));
  setMinimumWidth(420);
  const IndexSettings settings = IndexSettings::load();

  maxFileMb_ = new QSpinBox(this);
  maxFileMb_->setRange(1, 500);
  maxFileMb_->setValue(static_cast<int>(settings.maxFileBytes / 1024 / 1024));
  maxCharactersWan_ = new QSpinBox(this);
  maxCharactersWan_->setRange(1, 200);
  maxCharactersWan_->setValue(settings.maxCharacters / 10000);
  timeoutSeconds_ = new QSpinBox(this);
  timeoutSeconds_->setRange(5, 120);
  timeoutSeconds_->setValue(settings.timeoutSeconds);
  startupUpdate_ = new QCheckBox(QStringLiteral("启动文搜搜后自动检查并增量更新索引"), this);
  startupUpdate_->setChecked(
      QSettings().value(QStringLiteral("index/updateOnStartup"), false).toBool());

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral("单文件大小上限（MB）"), maxFileMb_);
  form->addRow(QStringLiteral("正文字符上限（万字）"), maxCharactersWan_);
  form->addRow(QStringLiteral("单文件解析超时（秒）"), timeoutSeconds_);
  form->addRow(QStringLiteral("启动检查更新"), startupUpdate_);

  auto* typesLabel = new QLabel(QStringLiteral("建立索引的文件类型"), this);
  typesLabel->setObjectName(QStringLiteral("sectionLabel"));
  auto* typesGrid = new QGridLayout;
  typesGrid->setHorizontalSpacing(14);
  typesGrid->setVerticalSpacing(8);
  const QStringList enabled = RootPolicy::enabledExtensions();
  const QStringList available = RootPolicy::availableExtensions();
  for (int index = 0; index < available.size(); ++index) {
    const QString extension = available.at(index);
    auto* check = new QCheckBox(extension.toUpper(), this);
    check->setProperty("extension", extension);
    check->setChecked(enabled.contains(extension));
    extensionChecks_.append(check);
    typesGrid->addWidget(check, index / 5, index % 5);
  }

  auto* note = new QLabel(
      QStringLiteral("新设置将在下一次更新索引时生效。超过限制的文件会记录为失败，"
                     "不会阻塞其他文件。TXT 默认不建立索引，如需要可在这里开启。"
                     "至少需要保留一种文件类型。"),
      this);
  note->setWordWrap(true);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                        this);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(typesLabel);
  layout->addLayout(typesGrid);
  layout->addWidget(note);
  layout->addWidget(buttons);
}

void SettingsDialog::save() {
  QStringList enabledExtensions;
  for (QCheckBox* check : extensionChecks_) {
    if (check->isChecked()) {
      enabledExtensions.append(check->property("extension").toString());
    }
  }
  if (enabledExtensions.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("无法保存"),
                         QStringLiteral("至少需要选择一种建立索引的文件类型。"));
    return;
  }
  QSettings settings;
  settings.setValue(QStringLiteral("index/maxFileBytes"),
                    maxFileMb_->value() * 1024LL * 1024LL);
  settings.setValue(QStringLiteral("index/maxCharacters"),
                    maxCharactersWan_->value() * 10000);
  settings.setValue(QStringLiteral("index/timeoutSeconds"),
                    timeoutSeconds_->value());
  settings.setValue(QStringLiteral("index/updateOnStartup"), startupUpdate_->isChecked());
  settings.setValue(QStringLiteral("index/enabledExtensions"), enabledExtensions);
  accept();
}

}  // namespace wensousou
