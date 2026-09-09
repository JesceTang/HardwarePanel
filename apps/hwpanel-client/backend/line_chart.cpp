#include "apps/hwpanel-client/backend/line_chart.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>

namespace hwpanel::client {

LineChart::LineChart(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(QQuickItem::ItemHasContents, true);
}

LineChart::~LineChart() = default;

void LineChart::setValues(const QVariantList& values) {
  if (values_ == values) {
    return;
  }
  values_ = values;
  emit valuesChanged();
  update();
}

void LineChart::setLineColor(const QColor& color) {
  if (line_color_ == color) {
    return;
  }
  line_color_ = color;
  emit lineColorChanged();
  update();
}

void LineChart::setMinValue(double value) {
  if (qFuzzyCompare(min_value_, value)) {
    return;
  }
  min_value_ = value;
  emit rangeChanged();
  update();
}

void LineChart::setMaxValue(double value) {
  if (qFuzzyCompare(max_value_, value)) {
    return;
  }
  max_value_ = value;
  emit rangeChanged();
  update();
}

void LineChart::setAutoRange(bool enabled) {
  if (auto_range_ == enabled) {
    return;
  }
  auto_range_ = enabled;
  emit rangeChanged();
  update();
}

QSGNode* LineChart::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) {
  const int count = values_.size();
  const qreal w = width();
  const qreal h = height();

  auto* node = static_cast<QSGGeometryNode*>(old_node);
  if (node == nullptr) {
    node = new QSGGeometryNode;
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
    // RHI 仅支持 lineWidth==1（>1 会打印 "Line widths other than 1 are not
    // supported by the graphics API" 且仍按 1 渲染），故显式设 1 保持确定且无告警。
    geometry->setLineWidth(1.0f);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);

    auto* material = new QSGFlatColorMaterial;
    material->setColor(line_color_);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
  } else {
    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    if (material->color() != line_color_) {
      material->setColor(line_color_);
      node->markDirty(QSGNode::DirtyMaterial);
    }
  }

  if (count < 2 || w <= 0.0 || h <= 0.0) {
    node->geometry()->allocate(0);
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
  }

  double lo = min_value_;
  double hi = max_value_;
  if (auto_range_) {
    lo = values_.first().toDouble();
    hi = lo;
    for (const auto& v : values_) {
      const double d = v.toDouble();
      lo = std::min(lo, d);
      hi = std::max(hi, d);
    }
    if (qFuzzyCompare(lo, hi)) {
      lo -= 1.0;
      hi += 1.0;
    }
  }
  if (!(hi > lo)) {
    hi = lo + 1.0;
  }

  auto* geometry = node->geometry();
  geometry->allocate(count);
  auto* vertices = geometry->vertexDataAsPoint2D();
  const float step_x = static_cast<float>(w) / static_cast<float>(count - 1);
  for (int i = 0; i < count; ++i) {
    const double d = values_.at(i).toDouble();
    const float t = static_cast<float>((d - lo) / (hi - lo));
    vertices[i].set(step_x * static_cast<float>(i),
                    static_cast<float>(h) * (1.0f - std::clamp(t, 0.0f, 1.0f)));
  }
  node->markDirty(QSGNode::DirtyGeometry);
  return node;
}

}  // namespace hwpanel::client
