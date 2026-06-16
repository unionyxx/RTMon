#include "mainwindow.h"
#include "latencyanalyzer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QGroupBox>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QFontDatabase>
#include <QStyle>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent),
      m_workerThread(new QThread(this)),
      m_worker(new LatencyWorker()),
      m_isUiPaused(false) 
{
    resize(760, 650);
    setWindowTitle("RTMon");

    // Setup toolbar controls
    QToolBar *toolBar = addToolBar("Execution Controls");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);

    m_controlButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaPause), "Pause", this);
    m_controlButton->setCheckable(true);
    m_saveButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton), "Save Log", this);

    toolBar->addWidget(m_controlButton);
    toolBar->addSeparator();
    toolBar->addWidget(m_saveButton);

    connect(m_controlButton, &QPushButton::clicked, this, &MainWindow::toggleMonitoring);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveLog);

    // Main layout initialization
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    QHBoxLayout *metricLayout = new QHBoxLayout();
    metricLayout->setSpacing(10);

    // Use a fixed font so numbers don't jump around when updating
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixedFont.setPointSize(14);
    fixedFont.setBold(true);

    // Helper lambda to clean up metric card creation
    auto createMetricContainer = [this, &fixedFont](const QString &groupTitle, QLabel* &valueLabelTarget) {
        QGroupBox *groupBox = new QGroupBox(groupTitle, this);
        QVBoxLayout *boxLayout = new QVBoxLayout(groupBox);
        boxLayout->setContentsMargins(12, 8, 12, 8);
        
        valueLabelTarget = new QLabel("0.0 µs", groupBox);
        valueLabelTarget->setFont(fixedFont);
        
        boxLayout->addWidget(valueLabelTarget);
        return groupBox;
    };

    metricLayout->addWidget(createMetricContainer("Current", m_currentLabel));
    metricLayout->addWidget(createMetricContainer("Peak", m_maxLabel));
    metricLayout->addWidget(createMetricContainer("Major Faults", m_faultsLabel));
    mainLayout->addLayout(metricLayout);

    // Health status row
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setContentsMargins(5, 0, 5, 0);
    
    m_statusLabel = new QLabel("Stable", this);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(12);
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);

    m_statusHintLabel = new QLabel("— Good for gaming & real-time audio", this);
    m_statusHintLabel->setStyleSheet("color: gray;");
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_statusHintLabel);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // Splitter for graph and IRQ tables
    QSplitter *workspaceSplitter = new QSplitter(Qt::Vertical, this);
    workspaceSplitter->setChildrenCollapsible(false);
    
    m_graph = new LatencyGraph(this);
    workspaceSplitter->addWidget(m_graph);

    QTabWidget *tabs = new QTabWidget(this);
    
    m_adviceBox = new QTextEdit(this);
    m_adviceBox->setReadOnly(true);
    m_adviceBox->setFrameShape(QFrame::NoFrame); // Remove frame so it blends into the tab panel
    tabs->addTab(m_adviceBox, "Report");

    // Column rules for the IRQ data tables
    auto configureGridArchitecture = [](QTableWidget *table) {
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels(QStringList() << "Source" << "IRQ/s");
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setHighlightSections(false);
        table->verticalHeader()->setVisible(false); 
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSortingEnabled(true);
        table->setShowGrid(true);
        table->setAlternatingRowColors(true);
    };

    m_hardTable = new QTableWidget(this);
    configureGridArchitecture(m_hardTable);
    tabs->addTab(m_hardTable, "Hard IRQ");

    m_softTable = new QTableWidget(this);
    configureGridArchitecture(m_softTable);
    tabs->addTab(m_softTable, "Soft IRQ");

    workspaceSplitter->addWidget(tabs);
    
    // Give the bottom tables slightly more space than the graph by default
    workspaceSplitter->setStretchFactor(0, 9);
    workspaceSplitter->setStretchFactor(1, 11);
    
    mainLayout->addWidget(workspaceSplitter, 1);
    setCentralWidget(centralWidget);

    QStatusBar *statusBarWidget = new QStatusBar(this);
    setStatusBar(statusBarWidget);
    statusBarWidget->showMessage("Sampling: 1 ms | Engine: Active");

    // System tray setup
    m_trayIcon = new QSystemTrayIcon(style()->standardIcon(QStyle::SP_ComputerIcon), this);
    m_trayMenu = new QMenu(this);
    QAction *restoreAction = m_trayMenu->addAction("Restore Window");
    QAction *quitAction = m_trayMenu->addAction("Exit");
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated);
    
    connect(restoreAction, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
    });
    
    // Use qApp quit to bypass our closeEvent window hiding logic
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    // Spin up the worker thread for background data gathering
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker.data(), &LatencyWorker::startProcessing);
    connect(m_worker.data(), &LatencyWorker::finished, m_workerThread, &QThread::quit);
    connect(m_worker.data(), &LatencyWorker::finished, m_worker.data(), &QObject::deleteLater);
    connect(m_worker.data(), &LatencyWorker::dataUpdated, this, &MainWindow::handleData);

    m_workerThread->start();
}

MainWindow::~MainWindow() {
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_worker) {
            QMetaObject::invokeMethod(m_worker.data(), "stopProcessing", Qt::QueuedConnection);
        }
        
        // Try to let the thread exit cleanly, force quit if it deadlocks
        if (!m_workerThread->wait(1500)) {
            qWarning() << "Worker thread termination timeout reached. Forcing event loop closure.";
            m_workerThread->quit();
            
            if (!m_workerThread->wait(500)) {
                qCritical() << "FATAL: Subsystem loop deadlocked in kernel layer. Forcing immediate fail-fast exit.";
                std::terminate(); 
            }
        }
    }
}

void MainWindow::toggleMonitoring() {
    m_isUiPaused = !m_isUiPaused;

    if (m_worker) {
        QMetaObject::invokeMethod(m_worker.data(), "setPaused", Qt::QueuedConnection, Q_ARG(bool, m_isUiPaused));
    }

    m_controlButton->setText(m_isUiPaused ? "Resume" : "Pause");
    m_controlButton->setIcon(style()->standardIcon(m_isUiPaused ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
    
    if (statusBar()) {
        statusBar()->showMessage(m_isUiPaused ? "Sampling: Suspended | Engine: Halted" : "Sampling: 1 ms | Engine: Active");
    }

    m_statusLabel->setText(m_isUiPaused ? "Paused" : "Active");
    m_statusLabel->setStyleSheet(m_isUiPaused ? "color: gray;" : "color: palette(text);");
    m_statusHintLabel->setText(m_isUiPaused ? "— Monitoring suspended" : "— Polling system state");

    if (m_isUiPaused) {
        m_adviceBox->setText("Data polling suspended. System state tracking workflows are paused.");
    }
}

void MainWindow::updateTableData(QTableWidget *table, const QVector<InterruptInfo>& data) {
    if (data.isEmpty()) return;
    
    // Disable sorting while adding items to prevent UI lag
    table->setSortingEnabled(false);
    table->setRowCount(data.size());

    QFont gridNumFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    for (int i = 0; i < data.size(); ++i) {
        QTableWidgetItem *nameItem = new QTableWidgetItem(data[i].name);
        QTableWidgetItem *countItem = new QTableWidgetItem();
        
        countItem->setData(Qt::DisplayRole, static_cast<qlonglong>(data[i].countPerSec));
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        countItem->setFont(gridNumFont); 

        // Highlight heavy interrupt rows
        if (data[i].countPerSec > 10000) {
            QColor warnColor = palette().color(QPalette::Highlight);
            warnColor.setAlpha(35);
            nameItem->setBackground(warnColor);
            countItem->setBackground(warnColor);
        }

        table->setItem(i, 0, nameItem);
        table->setItem(i, 1, countItem);
    }
    table->setSortingEnabled(true);
}

void MainWindow::handleData(double windowMaxLatencyUs, 
                             const QVector<InterruptInfo>& topHard, 
                             const QVector<InterruptInfo>& topSoft, 
                             unsigned long long majorPageFaultsPerSec) 
{
    m_lastLatencyUs = windowMaxLatencyUs;
    if (windowMaxLatencyUs > m_maxLatencyUs) {
        m_maxLatencyUs = windowMaxLatencyUs;
    }

    m_history.append(windowMaxLatencyUs);
    if (m_history.size() > m_maxHistorySize) {
        m_history.removeFirst();
    }

    // Refresh UI text indicators
    m_currentLabel->setText(QString("%1 µs").arg(windowMaxLatencyUs, 0, 'f', 1));
    m_maxLabel->setText(QString("%1 µs").arg(m_maxLatencyUs, 0, 'f', 1));
    m_faultsLabel->setText(QString("%1 /s").arg(majorPageFaultsPerSec));

    // Evaluate stats against rulesets to build advice text
    DiagnosticReport report = LatencyAnalyzer::analyze(windowMaxLatencyUs, topHard, topSoft, majorPageFaultsPerSec);
    
    m_statusLabel->setText(report.statusLabel);
    m_statusLabel->setStyleSheet(report.colorStyle);
    m_statusHintLabel->setText("— " + report.statusHint);
    
    m_adviceBox->setText(report.adviceText);

    m_graph->updateData(m_history, m_maxLatencyUs, windowMaxLatencyUs);
    updateTableData(m_hardTable, topHard);
    updateTableData(m_softTable, topSoft);

    m_trayIcon->setToolTip(QString("Current: %1 µs\nPeak: %2 µs")
                           .arg(windowMaxLatencyUs, 0, 'f', 1)
                           .arg(m_maxLatencyUs, 0, 'f', 1));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Intercept closing so the app minimizes to tray instead of quitting
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
        }
    }
}

void MainWindow::saveLog() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save Metric File Log", QDir::homePath() + "/latency_report.txt", "Data Document Target Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "========================================\n";
        out << "       SCHEDULING DIAGNOSTIC LOG        \n";
        out << "========================================\n";
        out << "Timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        out << "Maximum Window Core Latency: " << m_maxLatencyUs << " us\n";
        out << "Analysis Report Summary Context:\n" << m_adviceBox->toPlainText() << "\n";
        
        file.close();
        QMessageBox::information(this, "Export Log", "Performance metric log data saved successfully.");
    }
}