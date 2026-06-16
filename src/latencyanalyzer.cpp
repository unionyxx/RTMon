#include "latencyanalyzer.h"

DiagnosticReport LatencyAnalyzer::analyze(double windowMaxLatencyUs, 
                                          const QVector<InterruptInfo>& hardIrqs,
                                          const QVector<InterruptInfo>& softIrqs,
                                          unsigned long long majorFaultsPerSec) 
{
    DiagnosticReport report;
    
    const unsigned long long HARD_IRQ_THRESHOLD_SEC = 2000;
    const unsigned long long SOFT_IRQ_THRESHOLD_SEC = 3000;

    if (windowMaxLatencyUs <= 500.0) {
        report.statusLabel = "Stable";
        report.statusHint = "Good for gaming & real-time audio";
        report.colorStyle = "color: #55ff55; font-weight: bold;";
        report.verdict = "System is rock solid with negligible jitter.";
    } 
    else if (windowMaxLatencyUs <= 1000.0) {
        report.statusLabel = "Moderate";
        report.statusHint = "Minor jitter detected";
        report.colorStyle = "color: #aaff55; font-weight: bold;";
        report.verdict = "System is performing well for general tasks.";
    }
    else if (windowMaxLatencyUs <= 2000.0) {
        report.statusLabel = "Elevated";
        report.statusHint = "May affect low-latency tasks";
        report.colorStyle = "color: #ffaa00; font-weight: bold;";
        report.verdict = "Latency is within normal limits for a standard kernel.";
    } else {
        report.statusLabel = "Critical";
        report.statusHint = "Unsuitable for real-time use";
        report.colorStyle = "color: #ff5555; font-weight: bold;";
        report.verdict = "Significant scheduling delays detected (>2ms).";
    }

    // Support for legacy UI fields
    report.statusText = QString("Status: %1 (%2)").arg(report.statusLabel).arg(report.statusHint);

    bool issuesFound = false;

    if (majorFaultsPerSec > 1) {
        report.source = QString("Memory thrashing (%1 Major Faults/s)").arg(majorFaultsPerSec);
        report.action = "Increase RAM or lower vm.swappiness to 10.";
        issuesFound = true;
    }

    if (!issuesFound) {
        QString hardCulprit = "";
        unsigned long long maxHardCount = 0;

        for (const auto& irq : hardIrqs) {
            QString name = irq.name.toLower();
            // Filter out internal/system interrupts that are usually noise
            if (name.contains("timer") || name.contains("rescheduling") || 
                name.contains("call") || name.contains("work") || name.contains("tlb")) {
                continue;
            }
            if (irq.countPerSec > maxHardCount) {
                maxHardCount = irq.countPerSec;
                hardCulprit = irq.name;
            }
        }

        if (!hardCulprit.isEmpty() && maxHardCount > HARD_IRQ_THRESHOLD_SEC) {
            if (hardCulprit.contains("amdgpu") || hardCulprit.contains("nvidia") || hardCulprit.contains("i915")) {
                report.source = QString("GPU Driver load (%1: %2 IRQ/s)").arg(hardCulprit).arg(maxHardCount);
                report.action = "Disable GPU frequency monitoring or check power profiles.";
            } 
            else if (hardCulprit.contains("xhci_hcd") || hardCulprit.contains("ehci")) {
                report.source = QString("USB Controller load (%1: %2 IRQ/s)").arg(hardCulprit).arg(maxHardCount);
                report.action = "Move audio peripherals to native CPU ports (avoid hubs).";
            } else {
                report.source = QString("Device [%1] high interrupt rate (%2 IRQ/s)").arg(hardCulprit).arg(maxHardCount);
                report.action = "Check for driver updates or hardware conflicts.";
            }
            issuesFound = true;
        }
    }

    if (!issuesFound) {
        QString softCulprit = "";
        unsigned long long maxSoftCount = 0;

        for (const auto& sirq : softIrqs) {
            if (sirq.name == "TIMER" || sirq.name == "SCHED" || sirq.name == "RCU") {
                if (sirq.countPerSec < 8000) continue; 
            }
            if (sirq.name == "HI" || sirq.name == "TASKLET") continue;

            if (sirq.countPerSec > maxSoftCount) {
                maxSoftCount = sirq.countPerSec;
                softCulprit = sirq.name;
            }
        }

        if (!softCulprit.isEmpty() && maxSoftCount > SOFT_IRQ_THRESHOLD_SEC) {
            if (softCulprit == "NET_RX" || softCulprit == "NET_TX") {
                report.source = QString("Network traffic load (%1: %2/s)").arg(softCulprit).arg(maxSoftCount);
                report.action = "Limit background downloads or optimize network scheduler.";
            }
            else if (softCulprit == "BLOCK") {
                report.source = QString("I/O Subsystem load (BLOCK: %1/s)").arg(maxSoftCount);
                report.action = "Change I/O scheduler to 'none' or 'mq-deadline'.";
            } else {
                report.source = QString("SoftIRQ load (%1: %2/s)").arg(softCulprit).arg(maxSoftCount);
                report.action = "Reduce background service activity.";
            }
            issuesFound = true;
        }
    }

    if (!issuesFound) {
        report.source = "Standard kernel scheduling jitter.";
        report.action = "For better results, use a real-time kernel (linux-rt or zen).";
    }

    report.adviceText = QString("Verdict: %1\nSource: %2\nAction: %3")
                        .arg(report.verdict)
                        .arg(report.source)
                        .arg(report.action);

    return report;
}
