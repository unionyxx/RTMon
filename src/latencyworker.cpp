#include "latencyworker.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

LatencyWorker::LatencyWorker(QObject *parent) 
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_isPaused(false),
      m_loopCounter(0),
      m_windowMaxLatencyUs(0.0),
      m_lastMajorFaults(0) 
{
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &LatencyWorker::sampleOnce);
}

void LatencyWorker::startProcessing() {
    if (m_timer->isActive()) return;

    m_isPaused = false;
    m_loopCounter = 0;
    m_windowMaxLatencyUs = 0.0;
    m_lastMajorFaults = 0;
    m_lastStatsTime = std::chrono::steady_clock::now();

    // Small initial delay to avoid immediate overflow
    m_nextWakeTime = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(1);
    m_timer->start(1);
}

void LatencyWorker::stopProcessing() {
    if (!m_timer->isActive()) return;

    m_timer->stop();
    emit finished();
}

void LatencyWorker::setPaused(bool paused) {
    m_isPaused = paused;
    if (m_isPaused) {
        m_windowMaxLatencyUs = 0.0;
    } else {
        m_nextWakeTime = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(1);
    }
}

void LatencyWorker::sampleOnce() {
    if (m_isPaused) return;

    auto now = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_nextWakeTime).count();
    
    double currentLatencyUs = std::max(0.0, diff / 1000.0);

    if (currentLatencyUs > m_windowMaxLatencyUs) {
        m_windowMaxLatencyUs = currentLatencyUs;
    }

    // Report stats every 500ms
    if (++m_loopCounter >= 500) {
        m_loopCounter = 0;

        QVector<InterruptInfo> topHard;
        QVector<InterruptInfo> topSoft;
        unsigned long long majFaultsPerSec = 0;

        updateSystemStats(topHard, topSoft, majFaultsPerSec);

        emit dataUpdated(m_windowMaxLatencyUs, topHard, topSoft, majFaultsPerSec);
        m_windowMaxLatencyUs = 0.0;
    }

    m_nextWakeTime += std::chrono::milliseconds(1);
    
    // Correct for scheduling drift
    if (now > m_nextWakeTime) {
        m_nextWakeTime = now + std::chrono::milliseconds(1);
    }
}

void LatencyWorker::updateSystemStats(QVector<InterruptInfo>& topHard, QVector<InterruptInfo>& topSoft, unsigned long long& majFaultsPerSec) {
    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastStatsTime).count() / 1000.0;
    if (elapsedSec <= 0.0) elapsedSec = 0.5;
    m_lastStatsTime = now;

    // Major page faults from vmstat
    unsigned long long currentMajFaults = 0;
    QFile vmstatFile("/proc/vmstat");
    if (vmstatFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&vmstatFile);
        QString line;
        while (in.readLineInto(&line)) {
            if (line.startsWith("pgmajfault")) {
                QStringList parts = line.split(QRegularExpression("\\s+"));
                if (parts.size() >= 2) currentMajFaults = parts[1].toULongLong();
                break;
            }
        }
        vmstatFile.close();
    }
    if (m_lastMajorFaults > 0 && currentMajFaults >= m_lastMajorFaults) {
        majFaultsPerSec = static_cast<unsigned long long>((currentMajFaults - m_lastMajorFaults) / elapsedSec);
    }
    m_lastMajorFaults = currentMajFaults;

    // Hard IRQs from interrupts
    QMap<QString, unsigned long long> currentHard;
    QFile irqFile("/proc/interrupts");
    if (irqFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&irqFile);
        QString line;
        in.readLineInto(&line); // Skip header

        while (in.readLineInto(&line)) {
            QStringList parts = line.trimmed().split(QRegularExpression("\\s+"));
            if (parts.size() < 3) continue;

            QString irqLabel = parts[0].remove(':');
            unsigned long long totalCount = 0;
            int lastCpuIdx = 1;
            bool ok = false;
            
            while (lastCpuIdx < parts.size()) {
                unsigned long long coreCount = parts[lastCpuIdx].toULongLong(&ok);
                if (!ok) break;
                totalCount += coreCount;
                lastCpuIdx++;
            }

            QString deviceName = "";
            if (lastCpuIdx < parts.size()) deviceName = parts.mid(lastCpuIdx).join(" ");
            if (deviceName.isEmpty() || deviceName.contains("ERR") || deviceName.contains("MIS")) {
                deviceName = "System (IRQ " + irqLabel + ")";
            }
            currentHard[deviceName] += totalCount;
        }
        irqFile.close();
    }

    for (auto it = currentHard.begin(); it != currentHard.end(); ++it) {
        unsigned long long last = m_lastHardCounts.value(it.key(), it.value());
        unsigned long long perSec = (it.value() >= last) ? static_cast<unsigned long long>((it.value() - last) / elapsedSec) : 0;
        if (perSec > 0) topHard.append({it.key(), perSec});
    }
    m_lastHardCounts = currentHard;
    std::sort(topHard.begin(), topHard.end(), [](const InterruptInfo& a, const InterruptInfo& b) { return a.countPerSec > b.countPerSec; });
    if (topHard.size() > 25) topHard.resize(25);

    // Soft IRQs from softirqs
    QMap<QString, unsigned long long> currentSoft;
    QFile softFile("/proc/softirqs");
    if (softFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&softFile);
        QString line;
        while (in.readLineInto(&line)) {
            QStringList parts = line.trimmed().split(QRegularExpression("\\s+"));
            if (parts.size() < 2 || !parts[0].contains(':')) continue;

            QString softName = parts[0].remove(':');
            unsigned long long totalCount = 0;
            for (int i = 1; i < parts.size(); ++i) {
                bool ok;
                unsigned long long c = parts[i].toULongLong(&ok);
                if (ok) totalCount += c;
            }
            currentSoft[softName] = totalCount;
        }
        softFile.close();
    }

    for (auto it = currentSoft.begin(); it != currentSoft.end(); ++it) {
        unsigned long long last = m_lastSoftCounts.value(it.key(), it.value());
        unsigned long long perSec = (it.value() >= last) ? static_cast<unsigned long long>((it.value() - last) / elapsedSec) : 0;
        if (perSec > 0) topSoft.append({it.key(), perSec});
    }
    m_lastSoftCounts = currentSoft;
    std::sort(topSoft.begin(), topSoft.end(), [](const InterruptInfo& a, const InterruptInfo& b) { return a.countPerSec > b.countPerSec; });
}
