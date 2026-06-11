#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "services/collectorservice.h"
#include "services/controllerservice.h"
#include "services/exportservice.h"
#include "services/modbuslabservice.h"
#include "transport/virtualbus.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QSerialPort>
#include <QSpinBox>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QEvent;
class QMouseEvent;

namespace Ui {

struct ChartSample {
    double voltage = 0.0;
    double current = 0.0;
    double temperature = 0.0;
    double battery = 0.0;
    double voltageLimit = 1.0;
    double currentLimit = 1.0;
    double temperatureLimit = 1.0;
    double batteryLimit = 1.0;
};

struct SessionRecord {
    QString startedAt;
    QString cardId;
    int startPower = 0;
    int endPower = 0;
    double fee = 0.0;
    QString result;
};

enum class LinkMode {
    Virtual,
    SerialController,
    SerialCollector
};

class ChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void setSamples(const QVector<ChartSample> &samples);
    void setOptions(bool showVoltage,
                    bool showCurrent,
                    bool showTemperature,
                    bool showBattery,
                    bool normalized,
                    bool showThresholds);
    void setDarkMode(bool dark);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF plotArea() const;
    QString tooltipTextAt(const QPoint &pos) const;

    QVector<ChartSample> m_samples;
    bool m_showVoltage = true;
    bool m_showCurrent = true;
    bool m_showTemperature = true;
    bool m_showBattery = true;
    bool m_normalized = true;
    bool m_showThresholds = true;
    bool m_darkMode = false;
};

class DashboardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    void setSnapshot(const Services::ControllerSnapshot &snapshot);
    void setSessions(const QVector<SessionRecord> &sessions);
    void setDarkMode(bool dark);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Services::ControllerSnapshot m_snapshot;
    QVector<SessionRecord> m_sessions;
    bool m_darkMode = false;
};

class FrameVisualWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FrameVisualWidget(QWidget *parent = nullptr);
    void setFrame(const Transport::BusLogEntry &entry);
    void setDarkMode(bool dark);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Transport::BusLogEntry m_entry;
    bool m_hasFrame = false;
    bool m_darkMode = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
#if defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private slots:
    void onStart();
    void onStop();
    void onWriteCard();
    void onPoll();
    void onApplySettings();
    void applyPresetSlow();
    void applyPresetFast();
    void applyPresetSafe();
    void onAlarmChanged();
    void refreshUi();
    void updateLog();
    void explainLastFrame();
    void openProtocolLab();
    void clearChart();
    void updateChartOptions();
    void exportLog();
    void exportHistory();
    void exportBill();
    void configureLink();
    void showSessionRecords();
    void showVisualDashboard();
    void toggleTheme();
    void handleCollectorSerialReadyRead();

private:
    QWidget *buildControllerPanel();
    QWidget *buildCollectorPanel();
    QWidget *buildMonitorPanel();
    QWidget *buildMonitorSidePanel();
    QWidget *buildActionBar();
    QWidget *buildWindowTitleBar();
    QLabel *valueLabel(const QString &title, QGridLayout *layout, int row, int col);
    QPushButton *commandButton(const QString &text, const QString &role = QString());
    void applyTheme();
    void toggleMaximizedState();
    void refreshWindowButtons();
    void setStatus(const QString &text);
    void updateActionHints(const Services::ControllerSnapshot &snapshot);
    QString diagnosisText(const Services::ControllerSnapshot &snapshot) const;
    bool validateSettings(QString *message = nullptr) const;
    void rememberSessionStart(const Services::ControllerSnapshot &snapshot);
    void finishSession(const QString &result);
    void switchToVirtualLink(const QString &message = QString());
    QByteArray exchangeSerialFrame(const QByteArray &payload);
    Services::ControllerSnapshot currentSnapshotForVisuals() const;
    void appendHistory(const Services::ControllerSnapshot &snapshot);
    ChartSample normalizeSnapshot(const Services::ControllerSnapshot &snapshot) const;
    void resetChartForParameterChange(const QString &message);
    void installHelpText(QWidget *widget, const QString &text);
    void showHelpText(QWidget *widget, const QString &text);

    bool m_darkMode = false;
    Services::CollectorService m_collector;
    Transport::VirtualBus m_bus;
    Services::ControllerService m_controller;
    Services::ExportService m_exportService;
    Services::ModbusLabService m_labService;
    QTimer m_timer;
    QSerialPort m_serial;
    LinkMode m_linkMode = LinkMode::Virtual;
    QByteArray m_serialRxBuffer;
    QVector<SessionRecord> m_sessionRecords;
    SessionRecord m_activeSession;
    bool m_hasActiveSession = false;
    QWidget *m_windowTitleBar = nullptr;
    QWidget *m_windowControlBox = nullptr;
    QPushButton *m_maximizeButton = nullptr;
    bool m_draggingWindow = false;
    QPoint m_dragStartGlobal;
    QPoint m_dragStartFrame;

    QCheckBox *m_plugCheck = nullptr;
    QLineEdit *m_cardEdit = nullptr;
    QCheckBox *m_autoPollCheck = nullptr;
    QPushButton *m_cardButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_pollButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_linkModeLabel = nullptr;
    QLabel *m_voltageLabel = nullptr;
    QLabel *m_currentLabel = nullptr;
    QLabel *m_temperatureLabel = nullptr;
    QLabel *m_batteryLabel = nullptr;
    QLabel *m_alarmLabel = nullptr;
    QLabel *m_chargeModeLabel = nullptr;
    QLabel *m_marginLabel = nullptr;
    QLabel *m_runtimeLabel = nullptr;
    QLabel *m_energyUsedLabel = nullptr;
    QLabel *m_energyLabel = nullptr;
    QLabel *m_etaLabel = nullptr;
    QLabel *m_riskLabel = nullptr;
    QProgressBar *m_progress = nullptr;

    QLabel *m_collectorStateLabel = nullptr;
    QLabel *m_collectorAlarmLabel = nullptr;
    QSpinBox *m_voltageLimitSpin = nullptr;
    QSpinBox *m_currentLimitSpin = nullptr;
    QSpinBox *m_temperatureLimitSpin = nullptr;
    QSpinBox *m_batteryLimitSpin = nullptr;
    QSpinBox *m_batteryPowerSpin = nullptr;
    QCheckBox *m_overVoltageCheck = nullptr;
    QCheckBox *m_overCurrentCheck = nullptr;
    QCheckBox *m_overTemperatureCheck = nullptr;

    QPlainTextEdit *m_logEdit = nullptr;
    QCheckBox *m_showVoltageCheck = nullptr;
    QCheckBox *m_showCurrentCheck = nullptr;
    QCheckBox *m_showTemperatureCheck = nullptr;
    QCheckBox *m_showBatteryCheck = nullptr;
    QCheckBox *m_showThresholdsCheck = nullptr;
    QCheckBox *m_pauseChartCheck = nullptr;
    QComboBox *m_chartModeCombo = nullptr;
    QLabel *m_txCountLabel = nullptr;
    QLabel *m_rxCountLabel = nullptr;
    QLabel *m_crcOkLabel = nullptr;
    QLabel *m_lastFrameLabel = nullptr;
    ChartWidget *m_chart = nullptr;
    QPushButton *m_themeButton = nullptr;
    DashboardWidget *m_dashboardWidget = nullptr;
    QVector<ChartSample> m_chartSamples;
    QVector<Services::ControllerSnapshot> m_snapshots;
};

}

#endif
