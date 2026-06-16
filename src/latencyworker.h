#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>
#include <QTimer>
#include <chrono>

struct InterruptInfo {
    QString name;
    unsigned long long countPerSec;
};

class LatencyWorker : public QObject {
    Q_OBJECT

public:
    explicit LatencyWorker(QObject *parent = nullptr);
    ~LatencyWorker() override = default;

public slots:
    void startProcessing();
    void stopProcessing();
    void setPaused(bool paused);

signals:
    void dataUpdated(double windowMaxLatencyUs, 
                     const QVector<InterruptInfo>& topHardInterrupts, 
                     const QVector<InterruptInfo>& topSoftInterrupts, 
                     unsigned long long majorPageFaultsPerSec);
    void finished();

private slots:
    void sampleOnce();

private:
    QTimer *m_timer;
    bool m_isPaused;
    int m_loopCounter;
    double m_windowMaxLatencyUs;

    std::chrono::high_resolution_clock::time_point m_nextWakeTime;
    std::chrono::steady_clock::time_point m_lastStatsTime;

    QMap<QString, unsigned long long> m_lastHardCounts;
    QMap<QString, unsigned long long> m_lastSoftCounts;
    unsigned long long m_lastMajorFaults;

    void updateSystemStats(QVector<InterruptInfo>& topHard, QVector<InterruptInfo>& topSoft, unsigned long long& majFaultsPerSec);
};