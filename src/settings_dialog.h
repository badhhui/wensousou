#pragma once

#include <QDialog>
#include <QList>

class QSpinBox;
class QCheckBox;

namespace wensousou {

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

 private slots:
  void save();

 private:
  QSpinBox* maxFileMb_ = nullptr;
  QSpinBox* maxCharactersWan_ = nullptr;
  QSpinBox* timeoutSeconds_ = nullptr;
  QCheckBox* startupUpdate_ = nullptr;
  QList<QCheckBox*> extensionChecks_;
};

}  // namespace wensousou
