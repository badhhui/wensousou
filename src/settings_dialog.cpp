#include "settings_dialog.h"

#include "app_paths.h"
#include "index_worker.h"
#include "root_policy.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace wensousou {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("索引设置"));
  setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
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
  fontScale_ = new QComboBox(this);
  const QList<QPair<QString, double>> fontScales = {
      {QStringLiteral("很小（85%）"), 0.85}, {QStringLiteral("小（90%）"), 0.90},
      {QStringLiteral("偏小（95%）"), 0.95}, {QStringLiteral("标准（100%）"), 1.00},
      {QStringLiteral("偏大（105%）"), 1.05}, {QStringLiteral("大（110%）"), 1.10},
      {QStringLiteral("较大（115%）"), 1.15}, {QStringLiteral("很大（120%）"), 1.20},
      {QStringLiteral("超大（130%）"), 1.30}, {QStringLiteral("特大（140%）"), 1.40},
  };
  const double currentFontScale =
      QSettings().value(QStringLiteral("ui/fontScale"), 1.0).toDouble();
  int selectedFontIndex = 0;
  double bestDistance = 10.0;
  for (int index = 0; index < fontScales.size(); ++index) {
    fontScale_->addItem(fontScales.at(index).first, fontScales.at(index).second);
    const double distance = qAbs(fontScales.at(index).second - currentFontScale);
    if (distance < bestDistance) {
      bestDistance = distance;
      selectedFontIndex = index;
    }
  }
  fontScale_->setCurrentIndex(selectedFontIndex);
  resultLimit_ = new QComboBox(this);
  const QList<QPair<QString, int>> resultLimits = {
      {QStringLiteral("不限制（默认）"), 0},
      {QStringLiteral("50 条"), 50},
      {QStringLiteral("100 条"), 100},
      {QStringLiteral("200 条"), 200},
      {QStringLiteral("500 条"), 500},
      {QStringLiteral("1000 条"), 1000},
      {QStringLiteral("2000 条"), 2000},
      {QStringLiteral("5000 条"), 5000},
  };
  const int currentResultLimit =
      QSettings().value(QStringLiteral("search/resultLimit"), 0).toInt();
  int selectedResultLimit = 0;
  for (int index = 0; index < resultLimits.size(); ++index) {
    resultLimit_->addItem(resultLimits.at(index).first, resultLimits.at(index).second);
    if (resultLimits.at(index).second == currentResultLimit) {
      selectedResultLimit = index;
    }
  }
  resultLimit_->setCurrentIndex(selectedResultLimit);
  startupUpdate_ = new QCheckBox(QStringLiteral("启动文搜搜后自动检查并增量更新索引"), this);
  startupUpdate_->setChecked(
      QSettings().value(QStringLiteral("index/updateOnStartup"), false).toBool());

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral("单文件大小上限（MB）"), maxFileMb_);
  form->addRow(QStringLiteral("正文字符上限（万字）"), maxCharactersWan_);
  form->addRow(QStringLiteral("单文件解析超时（秒）"), timeoutSeconds_);
  form->addRow(QStringLiteral("界面字体大小"), fontScale_);
  form->addRow(QStringLiteral("搜索结果保留条数"), resultLimit_);
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

  auto* resetButton = new QPushButton(QStringLiteral("重置程序数据"), this);
  resetButton->setObjectName(QStringLiteral("dangerButton"));
  auto* aboutButton = new QPushButton(QStringLiteral("关于文搜搜"), this);
  aboutButton->setObjectName(QStringLiteral("quietButton"));
  auto* cancelButton = new QPushButton(QStringLiteral("取消"), this);
  cancelButton->setObjectName(QStringLiteral("quietButton"));
  auto* saveButton = new QPushButton(QStringLiteral("保存"), this);
  saveButton->setObjectName(QStringLiteral("primaryButton"));
  saveButton->setDefault(true);
  auto* buttons = new QHBoxLayout;
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(10);
  buttons->addWidget(resetButton);
  buttons->addWidget(aboutButton);
  buttons->addStretch();
  buttons->addWidget(cancelButton);
  buttons->addWidget(saveButton);
  connect(aboutButton, &QPushButton::clicked, this, [this]() {
    QMessageBox about(this);
    about.setWindowTitle(QStringLiteral("关于文搜搜"));
    about.setTextFormat(Qt::RichText);
    about.setTextInteractionFlags(Qt::TextBrowserInteraction);
    about.setText(
        QStringLiteral("<h3>文搜搜</h3>"
                       "<p>作者：hhui<br>"
                       "邮箱反馈：<a href=\"mailto:badhhui@163.com\">"
                       "badhhui@163.com</a><br>"
                       "当前版本号：%1<br>"
                       "仓库地址：<a href=\"https://github.com/badhhui/wensousou\">"
                       "badhhui/wensousou</a></p>")
            .arg(QCoreApplication::applicationVersion()));
    about.addButton(QStringLiteral("关闭"), QMessageBox::AcceptRole);
    about.exec();
  });
  connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetApplication);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::save);

  auto* resetHint = new QLabel(
      QStringLiteral("程序运行异常时可点击“重置程序数据”。重置会清除索引数据库和本机配置，"
                     "不会删除你的原始文档。"),
      this);
  resetHint->setObjectName(QStringLiteral("muted"));
  resetHint->setWordWrap(true);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(typesLabel);
  layout->addLayout(typesGrid);
  layout->addWidget(note);
  layout->addWidget(resetHint);
  layout->addLayout(buttons);
}

void SettingsDialog::save() {
  QStringList enabledExtensions;
  for (QCheckBox* check : extensionChecks_) {
    if (check->isChecked()) {
      enabledExtensions.append(check->property("extension").toString());
    }
  }
  if (enabledExtensions.isEmpty()) {
    QMessageBox message(this);
    message.setIcon(QMessageBox::Warning);
    message.setWindowTitle(QStringLiteral("无法保存"));
    message.setText(QStringLiteral("至少需要选择一种建立索引的文件类型。"));
    message.addButton(QStringLiteral("确定"), QMessageBox::AcceptRole);
    message.exec();
    return;
  }
  QSettings settings;
  settings.setValue(QStringLiteral("index/maxFileBytes"),
                    maxFileMb_->value() * 1024LL * 1024LL);
  settings.setValue(QStringLiteral("index/maxCharacters"),
                    maxCharactersWan_->value() * 10000);
  settings.setValue(QStringLiteral("index/timeoutSeconds"),
                    timeoutSeconds_->value());
  settings.setValue(QStringLiteral("ui/fontScale"), fontScale_->currentData().toDouble());
  settings.setValue(QStringLiteral("search/resultLimit"), resultLimit_->currentData().toInt());
  settings.setValue(QStringLiteral("index/updateOnStartup"), startupUpdate_->isChecked());
  settings.setValue(QStringLiteral("index/enabledExtensions"), enabledExtensions);
  accept();
}

void SettingsDialog::resetApplication() {
  QSettings settings;
  settings.sync();
  const QString databasePath = AppPaths::databasePath();
  const QString settingsPath = settings.fileName();
  QStringList paths = {
      databasePath,
      databasePath + QStringLiteral("-wal"),
      databasePath + QStringLiteral("-shm"),
      databasePath + QStringLiteral("-journal"),
  };
  if (!settingsPath.isEmpty()) paths.append(settingsPath);

  QDialog warning(this);
  warning.setWindowTitle(QStringLiteral("重置程序数据"));
  warning.setWindowFlags(warning.windowFlags() & ~Qt::WindowMaximizeButtonHint);
  auto* warningLayout = new QVBoxLayout(&warning);
  warningLayout->setSpacing(12);

  auto* content = new QHBoxLayout;
  content->setSpacing(12);
  auto* icon = new QLabel(&warning);
  icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(38, 38));
  icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  content->addWidget(icon);

  auto* copy = new QVBoxLayout;
  auto* title = new QLabel(QStringLiteral("程序运行异常时可点击重置程序数据。"), &warning);
  title->setObjectName(QStringLiteral("resultTitle"));
  title->setWordWrap(true);
  auto* message = new QLabel(
      QStringLiteral("此操作将删除索引数据库、索引目录配置、搜索历史和所有本机设置，"
                     "然后自动重启文搜搜。\n\n原始文档不会被删除。是否继续？"),
      &warning);
  message->setWordWrap(true);
  copy->addWidget(title);
  copy->addWidget(message);
  content->addLayout(copy, 1);
  warningLayout->addLayout(content);

  auto* details = new QPlainTextEdit(&warning);
  details->setPlainText(
      QStringLiteral("将删除以下文件：\n%1").arg(paths.join(QStringLiteral("\n"))));
  details->setReadOnly(true);
  details->setVisible(false);
  details->setMinimumHeight(130);
  warningLayout->addWidget(details);

  auto* buttonRow = new QHBoxLayout;
  buttonRow->setContentsMargins(0, 0, 0, 0);
  auto* detailsButton = new QPushButton(QStringLiteral("显示详情"), &warning);
  detailsButton->setObjectName(QStringLiteral("quietButton"));
  auto* cancel = new QPushButton(QStringLiteral("取消"), &warning);
  cancel->setObjectName(QStringLiteral("quietButton"));
  auto* confirm = new QPushButton(QStringLiteral("确认重置并重启"), &warning);
  confirm->setObjectName(QStringLiteral("dangerButton"));
  buttonRow->addWidget(detailsButton);
  buttonRow->addStretch();
  buttonRow->addWidget(cancel);
  buttonRow->addWidget(confirm);
  warningLayout->addLayout(buttonRow);

  connect(detailsButton, &QPushButton::clicked, &warning, [details, detailsButton, &warning]() {
    const bool show = !details->isVisible();
    details->setVisible(show);
    detailsButton->setText(show ? QStringLiteral("隐藏详情") : QStringLiteral("显示详情"));
    warning.adjustSize();
  });
  connect(cancel, &QPushButton::clicked, &warning, &QDialog::reject);
  connect(confirm, &QPushButton::clicked, &warning, &QDialog::accept);
  if (warning.exec() != QDialog::Accepted) return;

  QStringList arguments = {
      QStringLiteral("--reset-user-data-helper"),
      QString::number(QCoreApplication::applicationPid()),
      QStringLiteral("--restart"),
      QCoreApplication::applicationFilePath(),
  };
  for (const QString& path : paths) {
    if (path.isEmpty()) continue;
    arguments << QStringLiteral("--path") << path;
  }

  if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments)) {
    QMessageBox message(this);
    message.setIcon(QMessageBox::Warning);
    message.setWindowTitle(QStringLiteral("重置失败"));
    message.setText(QStringLiteral("无法启动重置助手。"));
    message.setInformativeText(
        QStringLiteral("请关闭文搜搜后手动删除：\n%1")
            .arg(paths.join(QStringLiteral("\n"))));
    message.addButton(QStringLiteral("确定"), QMessageBox::AcceptRole);
    message.exec();
    return;
  }

  accept();
  QCoreApplication::quit();
}

}  // namespace wensousou
