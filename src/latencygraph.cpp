#include "latencygraph.h"
#include <QPainter>
#include <QPainterPath>

LatencyGraph::LatencyGraph(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(200);
}

void LatencyGraph::updateData(const QVector<double>& history, double maxLatencyUs, double currentLatencyUs) {
    m_history = history;
    m_maxLatencyUs = maxLatencyUs;
    m_currentLatencyUs = currentLatencyUs;
    update();
}

void LatencyGraph::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPalette pal = palette();
    QColor borderColor = pal.color(QPalette::WindowText);
    borderColor.setAlpha(40);
    
    const int topPadding = 20;
    const int bottomPadding = 10;
    const int leftPadding = 5;
    const int rightPadding = 5;
    QRect drawingRect = rect().adjusted(leftPadding, topPadding, -rightPadding, -bottomPadding);

    painter.setPen(QPen(borderColor, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1)); 

    if (m_history.isEmpty()) return;

    // Reference mid-line
    int midY = drawingRect.center().y();
    painter.setPen(QPen(borderColor, 1, Qt::DashLine));
    painter.drawLine(rect().left(), midY, rect().right(), midY);

    const int maxHistorySize = 300; 
    double stepX = static_cast<double>(drawingRect.width()) / maxHistorySize;
    
    // Scaling with 15% headroom
    double yScaleLimit = qMax(200.0, m_maxLatencyUs * 1.15);

    QPainterPath path;
    for (int i = 0; i < m_history.size(); ++i) {
        double x = drawingRect.left() + i * stepX;
        double y = drawingRect.bottom() - (m_history[i] / yScaleLimit) * drawingRect.height();
        
        if (y < drawingRect.top()) y = drawingRect.top();

        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }

    // Color feedback based on latency levels
    QColor lineColor = QColor(85, 255, 85); // Green
    if (m_currentLatencyUs > 2000) lineColor = QColor(255, 85, 85); // Red
    else if (m_currentLatencyUs > 1000) lineColor = QColor(255, 170, 0); // Orange

    painter.setPen(QPen(lineColor, 2));
    painter.drawPath(path);

    // Marker for the latest sample
    if (!m_history.isEmpty()) {
        double lastX = drawingRect.left() + (m_history.size() - 1) * stepX;
        double lastY = drawingRect.bottom() - (m_history.last() / yScaleLimit) * drawingRect.height();
        if (lastY < drawingRect.top()) lastY = drawingRect.top();

        painter.setBrush(lineColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(lastX, lastY), 3, 3);
    }
}
