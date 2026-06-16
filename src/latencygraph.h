#pragma once

#include <QWidget>
#include <QVector>
#include <QPainter>
#include <QPainterPath>

class LatencyGraph : public QWidget {
    Q_OBJECT
public:
    explicit LatencyGraph(QWidget *parent = nullptr);
    void updateData(const QVector<double>& history, double maxLatencyUs, double currentLatencyUs);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_history;
    double m_maxLatencyUs = 0.0;
    double m_currentLatencyUs = 0.0;
};
