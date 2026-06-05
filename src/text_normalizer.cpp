#include "text_normalizer.h"

#include <QRegularExpression>

namespace wensousou {
namespace {

const QString kHan = QStringLiteral("\\x{3400}-\\x{4DBF}\\x{4E00}-\\x{9FFF}"
                                    "\\x{F900}-\\x{FAFF}");

}  // namespace

QString normalizeExtractedText(QString text) {
  text = text.normalized(QString::NormalizationForm_KC);
  static const QRegularExpression betweenHan(
      QStringLiteral("([%1])\\s+(?=[%1])").arg(kHan));
  static const QRegularExpression beforePunctuation(
      QStringLiteral("([%1])\\s+(?=[，。！？；：、）】》])").arg(kHan));
  static const QRegularExpression afterOpeningPunctuation(
      QStringLiteral("([（【《])\\s+(?=[%1])").arg(kHan));
  text.replace(betweenHan, QStringLiteral("\\1"));
  text.replace(beforePunctuation, QStringLiteral("\\1"));
  text.replace(afterOpeningPunctuation, QStringLiteral("\\1"));
  return text;
}

QString normalizeSearchText(QString text) {
  return text.normalized(QString::NormalizationForm_KC).simplified();
}

}  // namespace wensousou
