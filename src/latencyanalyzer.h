#pragma once

#include <QString>
#include <QVector>
#include "latencyworker.h"

struct DiagnosticReport {
    QString statusLabel;
    QString statusHint;
    QString colorStyle;
    
    QString verdict;
    QString source;
    QString action;
    
    // UI fallbacks
    QString statusText; 
    QString adviceText;
};

class LatencyAnalyzer {
public:
    static DiagnosticReport analyze(double windowMaxLatencyUs, 
                                    const QVector<InterruptInfo>& hardIrqs,
                                    const QVector<InterruptInfo>& softIrqs,
                                    unsigned long long majorFaultsPerSec);
};
