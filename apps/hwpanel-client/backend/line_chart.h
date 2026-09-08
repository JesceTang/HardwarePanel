#pragma once

#include <QColor>
#include <QQuickItem>
#include <QVariantList>

namespace hwpanel::client {

// Self-drawn realtime curve rendered directly in the scene graph via a
// QSGGeometryNode line strip (no Qt Charts dependency). Values are pushed
// from QML bindings on MetricSeries::values.
class LineChart : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(QVariantList values READ values WRITE setValues NOTIFY valuesChanged)
  Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged)
  Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY rangeChanged)
  Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY rangeChanged)
  Q_PROPERTY(bool autoRange READ autoRange WRITE setAutoRange NOTIFY rangeChanged)
 public:
  explicit LineChart(QQuickItem* parent = nullptr);
  ~LineChart() override;

  QVariantList values() const { return values_; }
  void setValues(const QVariantList& values);

  QColor lineColor() const { return line_color_; }
  void setLineColor(const QColor& color);

  double minValue() const { return min_value_; }
  void setMinValue(double value);
  double maxValue() const { return max_value_; }
  void setMaxValue(double value);

  bool autoRange() const { return auto_range_; }
  void setAutoRange(bool enabled);

 signals:
  void valuesChanged();
  void lineColorChanged();
  void rangeChanged();

 protected:
  QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) override;

 private:
  QVariantList values_;
  QColor line_color_ = QColor("#4da3ff");
  double min_value_ = 0.0;
  double max_value_ = 100.0;
  bool auto_range_ = false;
};

}  // namespace hwpanel::client
