#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <QVector>
#include <QThread>
#include <QPointer>
#include "latencyworker.h"
#include "latencygraph.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void handleData(double windowMaxLatencyUs, 
                    const QVector<InterruptInfo>& topHard, 
                    const QVector<InterruptInfo>& topSoft, 
                    unsigned long long majorPageFaultsPerSec);
    void toggleMonitoring();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void saveLog();

private:
    QThread *m_workerThread;
    QPointer<LatencyWorker> m_worker;
    LatencyGraph *m_graph;
    
    QLabel *m_currentLabel;
    QLabel *m_maxLabel;
    QLabel *m_statusLabel;
    QLabel *m_statusHintLabel;
    QLabel *m_faultsLabel;
    QTextEdit *m_adviceBox;
    
    QPushButton *m_controlButton;
    QPushButton *m_saveButton;
    
    QTableWidget *m_hardTable;
    QTableWidget *m_softTable;
    
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;

    double m_maxLatencyUs = 0.0;
    double m_lastLatencyUs = 0.0;
    bool m_isUiPaused;
    
    QVector<double> m_history;
    const int m_maxHistorySize = 300;

    void updateTableData(QTableWidget *table, const QVector<InterruptInfo>& data);
};