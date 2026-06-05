#include "action_icons.h"

#include <QImage>
#include <QPixmap>
#include <QtTest>

namespace wensousou {
namespace {

bool hasVisiblePixels(const QIcon& icon) {
  const QImage image = icon.pixmap(26, 26).toImage().convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(image.pixel(x, y)) > 0) return true;
    }
  }
  return false;
}

}  // namespace

class ActionIconsTest : public QObject {
  Q_OBJECT

 private slots:
  void rendersVisiblePixels();
};

void ActionIconsTest::rendersVisiblePixels() {
  QVERIFY(hasVisiblePixels(previewActionIcon()));
  QVERIFY(hasVisiblePixels(openActionIcon()));
  QVERIFY(hasVisiblePixels(folderActionIcon()));
}

}  // namespace wensousou

QTEST_MAIN(wensousou::ActionIconsTest)
#include "action_icons_test.moc"
