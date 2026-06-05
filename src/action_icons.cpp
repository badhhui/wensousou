#include "action_icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace wensousou {
namespace {

constexpr qreal kIconScale = 1.3;
constexpr int kIconSize = 26;

void setupPainter(QPixmap* pixmap, QPainter* painter) {
  pixmap->fill(Qt::transparent);
  painter->begin(pixmap);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->scale(kIconScale, kIconScale);
  painter->setPen(QPen(QColor(QStringLiteral("#475569")), 1.6, Qt::SolidLine,
                       Qt::RoundCap, Qt::RoundJoin));
}

QIcon previewIcon() {
  QPixmap pixmap(kIconSize, kIconSize);
  QPainter painter;
  setupPainter(&pixmap, &painter);
  QPainterPath eye;
  eye.moveTo(2, 10);
  eye.cubicTo(4.4, 6.7, 7, 5, 10, 5);
  eye.cubicTo(13, 5, 15.6, 6.7, 18, 10);
  eye.cubicTo(15.6, 13.3, 13, 15, 10, 15);
  eye.cubicTo(7, 15, 4.4, 13.3, 2, 10);
  painter.drawPath(eye);
  painter.drawEllipse(QPointF(10, 10), 2.3, 2.3);
  painter.end();
  return QIcon(pixmap);
}

QIcon openIcon() {
  QPixmap pixmap(kIconSize, kIconSize);
  QPainter painter;
  setupPainter(&pixmap, &painter);
  painter.drawLine(QPointF(5, 3.5), QPointF(11, 3.5));
  painter.drawLine(QPointF(11, 3.5), QPointF(14, 6.5));
  painter.drawLine(QPointF(14, 6.5), QPointF(14, 9));
  painter.drawLine(QPointF(11, 3.5), QPointF(11, 6.5));
  painter.drawLine(QPointF(11, 6.5), QPointF(14, 6.5));
  painter.drawLine(QPointF(5, 3.5), QPointF(3.5, 5));
  painter.drawLine(QPointF(3.5, 5), QPointF(3.5, 15));
  painter.drawLine(QPointF(3.5, 15), QPointF(5, 16.5));
  painter.drawLine(QPointF(5, 16.5), QPointF(11, 16.5));
  painter.drawLine(QPointF(9, 11), QPointF(16, 11));
  painter.drawLine(QPointF(16, 11), QPointF(13.5, 8.5));
  painter.drawLine(QPointF(16, 11), QPointF(13.5, 13.5));
  painter.end();
  return QIcon(pixmap);
}

QIcon folderIcon() {
  QPixmap pixmap(kIconSize, kIconSize);
  QPainter painter;
  setupPainter(&pixmap, &painter);
  QPainterPath folder;
  folder.moveTo(2.5, 6);
  folder.cubicTo(2.5, 5.2, 3.2, 4.5, 4, 4.5);
  folder.lineTo(8, 4.5);
  folder.lineTo(9.5, 6.5);
  folder.lineTo(16, 6.5);
  folder.cubicTo(16.8, 6.5, 17.5, 7.2, 17.5, 8);
  folder.lineTo(17.5, 14);
  folder.cubicTo(17.5, 14.8, 16.8, 15.5, 16, 15.5);
  folder.lineTo(4, 15.5);
  folder.cubicTo(3.2, 15.5, 2.5, 14.8, 2.5, 14);
  folder.closeSubpath();
  painter.fillPath(folder, QColor(QStringLiteral("#e0f2fe")));
  painter.drawPath(folder);
  painter.end();
  return QIcon(pixmap);
}

}  // namespace

QIcon previewActionIcon() { return previewIcon(); }
QIcon openActionIcon() { return openIcon(); }
QIcon folderActionIcon() { return folderIcon(); }

}  // namespace wensousou
