#include "mainwindow.h"

#include "core/crc16.h"
#include "core/modbusframe.h"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QTableWidget>
#include <QTextCursor>
#include <QToolTip>
#include <QtMath>
#include <QVBoxLayout>

namespace Ui {

namespace {

constexpr int MaxHistorySamples = 180;

QColor themedColor(bool dark, const char *light, const char *darkColor)
{
    return QColor(dark ? darkColor : light);
}

QScrollArea *wrapSidePanel(QWidget *content, int minWidth, int maxWidth)
{
    QScrollArea *area = new QScrollArea();
    area->setWidget(content);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setMinimumWidth(minWidth);
    area->setMaximumWidth(maxWidth);
    return area;
}

void drawLegendItem(QPainter &painter, int x, int y, const QColor &color, const QString &text, const QColor &textColor)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRect(x, y + 5, 18, 4), 2, 2);
    painter.setPen(textColor);
    painter.drawText(x + 26, y + 12, text);
}

double safeLimit(double value)
{
    return qMax(1.0, value);
}

double plottedValue(const ChartSample &sample, double ChartSample::*valueMember, double ChartSample::*limitMember, bool normalized)
{
    if (!normalized) {
        return sample.*valueMember;
    }
    return (sample.*valueMember) * 100.0 / safeLimit(sample.*limitMember);
}

double maxRawValue(const QVector<ChartSample> &samples)
{
    double maxValue = 1.0;
    for (const ChartSample &sample : samples) {
        maxValue = qMax(maxValue, sample.voltage);
        maxValue = qMax(maxValue, sample.current);
        maxValue = qMax(maxValue, sample.temperature);
        maxValue = qMax(maxValue, sample.battery);
        maxValue = qMax(maxValue, sample.voltageLimit);
        maxValue = qMax(maxValue, sample.currentLimit);
        maxValue = qMax(maxValue, sample.temperatureLimit);
        maxValue = qMax(maxValue, sample.batteryLimit);
    }
    return qMax(20.0, qCeil(maxValue / 20.0) * 20.0);
}

void drawSeries(QPainter &painter,
                const QRectF &area,
                const QVector<ChartSample> &samples,
                double ChartSample::*valueMember,
                double ChartSample::*limitMember,
                const QColor &color,
                bool normalized,
                double maxValue)
{
    if (samples.size() < 2) {
        return;
    }
    QPainterPath path;
    for (int i = 0; i < samples.size(); ++i) {
        const double x = area.left() + area.width() * i / (samples.size() - 1);
        const double value = qBound(0.0, plottedValue(samples[i], valueMember, limitMember, normalized), maxValue);
        const double y = area.bottom() - area.height() * value / safeLimit(maxValue);
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
}

} // namespace

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void ChartWidget::setSamples(const QVector<ChartSample> &samples)
{
    m_samples = samples;
    update();
}

void ChartWidget::setOptions(bool showVoltage,
                             bool showCurrent,
                             bool showTemperature,
                             bool showBattery,
                             bool normalized,
                             bool showThresholds)
{
    m_showVoltage = showVoltage;
    m_showCurrent = showCurrent;
    m_showTemperature = showTemperature;
    m_showBattery = showBattery;
    m_normalized = normalized;
    m_showThresholds = showThresholds;
    update();
}

void ChartWidget::setDarkMode(bool dark)
{
    m_darkMode = dark;
    update();
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor page = themedColor(m_darkMode, "#f8fafc", "#101827");
    const QColor cardBg = themedColor(m_darkMode, "#ffffff", "#182235");
    const QColor border = themedColor(m_darkMode, "#d8dee8", "#2b3b52");
    const QColor text = themedColor(m_darkMode, "#0f172a", "#e5edf7");
    const QColor muted = themedColor(m_darkMode, "#64748b", "#9aa8ba");
    const QColor grid = themedColor(m_darkMode, "#e2e8f0", "#29384d");
    const QColor axis = themedColor(m_darkMode, "#94a3b8", "#718096");
    painter.fillRect(rect(), page);

    const QRect card = rect().adjusted(0, 0, -1, -1);
    painter.setPen(border);
    painter.setBrush(cardBg);
    painter.drawRoundedRect(card, 8, 8);

    painter.setPen(text);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(9);
    painter.setFont(titleFont);
    painter.drawText(18, 26, QStringLiteral("归一化实时曲线"));

    QFont normalFont = painter.font();
    normalFont.setBold(false);
    normalFont.setPointSize(8);
    painter.setFont(normalFont);
    painter.setPen(muted);
    painter.drawText(18, 46, QStringLiteral("电压 / 电流 / 温度 / 电量均按各自上限归一到 0-100%"));

    drawLegendItem(painter, 18, 58, QColor("#2563eb"), QStringLiteral("电压"), text);
    drawLegendItem(painter, 92, 58, QColor("#16a34a"), QStringLiteral("电流"), text);
    drawLegendItem(painter, 166, 58, QColor("#dc2626"), QStringLiteral("温度"), text);
    drawLegendItem(painter, 240, 58, QColor("#7c3aed"), QStringLiteral("电量"), text);

    const double maxValue = m_normalized ? 100.0 : maxRawValue(m_samples);
    const QRectF area = plotArea();
    painter.setPen(grid);
    for (int i = 0; i <= 4; ++i) {
        const double y = area.top() + area.height() * i / 4.0;
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
    for (int i = 0; i <= 4; ++i) {
        const double x = area.left() + area.width() * i / 4.0;
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }

    painter.setPen(axis);
    painter.drawText(8, static_cast<int>(area.top() + 4), m_normalized ? "100%" : QString::number(maxValue, 'f', 0));
    painter.drawText(14, static_cast<int>(area.bottom()), "0%");
    painter.drawLine(area.bottomLeft(), area.bottomRight());
    painter.drawLine(area.bottomLeft(), area.topLeft());

    if (m_samples.size() < 2) {
        painter.setPen(muted);
        painter.drawText(area, Qt::AlignCenter, QStringLiteral("等待查询数据"));
        return;
    }

    if (m_showThresholds && m_normalized) {
        painter.setPen(QPen(axis, 1, Qt::DashLine));
        painter.drawLine(QPointF(area.left(), area.top()), QPointF(area.right(), area.top()));
    }
    if (m_showVoltage) {
        drawSeries(painter, area, m_samples, &ChartSample::voltage, &ChartSample::voltageLimit, QColor("#2563eb"), m_normalized, maxValue);
    }
    if (m_showCurrent) {
        drawSeries(painter, area, m_samples, &ChartSample::current, &ChartSample::currentLimit, QColor("#16a34a"), m_normalized, maxValue);
    }
    if (m_showTemperature) {
        drawSeries(painter, area, m_samples, &ChartSample::temperature, &ChartSample::temperatureLimit, QColor("#dc2626"), m_normalized, maxValue);
    }
    if (m_showBattery) {
        drawSeries(painter, area, m_samples, &ChartSample::battery, &ChartSample::batteryLimit, QColor("#7c3aed"), m_normalized, maxValue);
    }
}

void ChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QString text = tooltipTextAt(event->pos());
    if (text.isEmpty()) {
        QToolTip::hideText();
        return;
    }
    QToolTip::showText(event->globalPos(), text, this);
}

void ChartWidget::leaveEvent(QEvent *)
{
    QToolTip::hideText();
}

QRectF ChartWidget::plotArea() const
{
    return rect().adjusted(42, 92, -20, -30);
}

QString ChartWidget::tooltipTextAt(const QPoint &pos) const
{
    if (m_samples.isEmpty()) {
        return {};
    }
    const QRectF area = plotArea();
    if (!area.contains(pos)) {
        return {};
    }
    const int index = qBound(0,
                             qRound((pos.x() - area.left()) * (m_samples.size() - 1) / qMax(1.0, area.width())),
                             m_samples.size() - 1);
    const ChartSample &s = m_samples[index];
    auto percent = [](double value, double limit) {
        return value * 100.0 / safeLimit(limit);
    };
    return QStringLiteral("采样 #%1\n电压: %2/%3 V (%4%)\n电流: %5/%6 A (%7%)\n温度: %8/%9 °C (%10%)\n电量: %11/%12 kWh (%13%)")
        .arg(index + 1)
        .arg(s.voltage, 0, 'f', 0).arg(s.voltageLimit, 0, 'f', 0).arg(percent(s.voltage, s.voltageLimit), 0, 'f', 1)
        .arg(s.current, 0, 'f', 0).arg(s.currentLimit, 0, 'f', 0).arg(percent(s.current, s.currentLimit), 0, 'f', 1)
        .arg(s.temperature, 0, 'f', 0).arg(s.temperatureLimit, 0, 'f', 0).arg(percent(s.temperature, s.temperatureLimit), 0, 'f', 1)
        .arg(s.battery, 0, 'f', 0).arg(s.batteryLimit, 0, 'f', 0).arg(percent(s.battery, s.batteryLimit), 0, 'f', 1);
}

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(360);
}

void DashboardWidget::setSnapshot(const Services::ControllerSnapshot &snapshot)
{
    m_snapshot = snapshot;
    update();
}

void DashboardWidget::setSessions(const QVector<SessionRecord> &sessions)
{
    m_sessions = sessions;
    update();
}

void DashboardWidget::setDarkMode(bool dark)
{
    m_darkMode = dark;
    update();
}

void DashboardWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor page = themedColor(m_darkMode, "#f8fafc", "#101827");
    const QColor cardBg = themedColor(m_darkMode, "#ffffff", "#182235");
    const QColor border = themedColor(m_darkMode, "#d8dee8", "#2b3b52");
    const QColor text = themedColor(m_darkMode, "#0f172a", "#e5edf7");
    const QColor muted = themedColor(m_darkMode, "#64748b", "#9aa8ba");
    const QColor grid = themedColor(m_darkMode, "#e2e8f0", "#29384d");
    const QColor nodeIdle = themedColor(m_darkMode, "#f8fafc", "#202c42");
    const QColor nodeActive = themedColor(m_darkMode, "#dbeafe", "#17305f");
    painter.fillRect(rect(), page);
    painter.setPen(border);
    painter.setBrush(cardBg);
    painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 8, 8);

    QFont title = painter.font();
    title.setBold(true);
    title.setPointSize(10);
    painter.setFont(title);
    painter.setPen(text);
    painter.drawText(18, 28, QStringLiteral("充电流程状态图"));

    const QStringList nodes = {QStringLiteral("未插枪"), QStringLiteral("已插枪"), QStringLiteral("等待刷卡"),
                               QStringLiteral("正在充电"), QStringLiteral("满电/停机")};
    int active = 0;
    if (m_snapshot.plugged) {
        active = 1;
    }
    if (m_snapshot.waitingForCard) {
        active = 2;
    } else if (m_snapshot.charging) {
        active = 3;
    } else if (m_snapshot.progress >= 99.0 || m_snapshot.alarmText != QStringLiteral("正常")) {
        active = 4;
    }
    const int nodeY = 58;
    const int nodeW = qMax(82, (width() - 56) / nodes.size() - 8);
    for (int i = 0; i < nodes.size(); ++i) {
        const int x = 18 + i * (nodeW + 8);
        if (i > 0) {
            painter.setPen(QPen(themedColor(m_darkMode, "#94a3b8", "#607089"), 1.2));
            painter.drawLine(x - 8, nodeY + 20, x, nodeY + 20);
        }
        painter.setPen(i == active ? QColor("#2563eb") : border);
        painter.setBrush(i == active ? nodeActive : nodeIdle);
        painter.drawRoundedRect(QRect(x, nodeY, nodeW, 40), 8, 8);
        painter.setPen(i == active ? QColor("#6ea8fe") : text);
        painter.drawText(QRect(x, nodeY, nodeW, 40), Qt::AlignCenter, nodes[i]);
    }

    painter.setPen(text);
    painter.drawText(18, 138, QStringLiteral("安全裕量仪表"));
    auto drawMargin = [&painter, muted, border, nodeIdle](int x, int y, const QString &name, const QString &unit, int value, int limit) {
        const double ratio = qBound(0.0, value / safeLimit(limit), 1.0);
        const QColor color = value < 0 ? QColor("#dc2626") : (ratio < 0.10 ? QColor("#f59e0b") : QColor("#16a34a"));
        painter.setPen(muted);
        painter.drawText(x, y - 6, QString("%1余量 %2%3").arg(name).arg(value).arg(unit));
        painter.setPen(border);
        painter.setBrush(nodeIdle);
        painter.drawRoundedRect(QRect(x, y, 190, 14), 7, 7);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRect(x, y, qRound(190 * ratio), 14), 7, 7);
    };
    drawMargin(24, 164, QStringLiteral("电压"), QStringLiteral("V"), m_snapshot.voltageLimit - m_snapshot.voltage, m_snapshot.voltageLimit);
    drawMargin(240, 164, QStringLiteral("电流"), QStringLiteral("A"), m_snapshot.currentLimit - m_snapshot.current, m_snapshot.currentLimit);
    drawMargin(456, 164, QStringLiteral("温度"), QStringLiteral("°C"), m_snapshot.temperatureLimit - m_snapshot.temperature, m_snapshot.temperatureLimit);

    painter.setPen(text);
    painter.drawText(18, 228, QStringLiteral("最近会话电量对比"));
    const QRect chart(24, 250, width() - 48, qMax(70, height() - 270));
    painter.setPen(grid);
    painter.drawRect(chart);
    if (m_sessions.isEmpty()) {
        painter.setPen(muted);
        painter.drawText(chart, Qt::AlignCenter, QStringLiteral("暂无会话记录"));
        return;
    }
    const int count = qMin(8, m_sessions.size());
    int maxEnergy = 1;
    for (int i = 0; i < count; ++i) {
        maxEnergy = qMax(maxEnergy, qMax(0, m_sessions[i].endPower - m_sessions[i].startPower));
    }
    const int gap = 10;
    const int barW = qMax(18, (chart.width() - gap * (count + 1)) / count);
    for (int i = 0; i < count; ++i) {
        const int energy = qMax(0, m_sessions[i].endPower - m_sessions[i].startPower);
        const int h = qRound((chart.height() - 26) * energy / static_cast<double>(maxEnergy));
        const int x = chart.left() + gap + i * (barW + gap);
        const int y = chart.bottom() - h - 18;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#2563eb"));
        painter.drawRoundedRect(QRect(x, y, barW, h), 5, 5);
        painter.setPen(text);
        painter.drawText(QRect(x - 4, chart.bottom() - 16, barW + 8, 14), Qt::AlignCenter, QString::number(energy));
    }
}

FrameVisualWidget::FrameVisualWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(190);
}

void FrameVisualWidget::setFrame(const Transport::BusLogEntry &entry)
{
    m_entry = entry;
    m_hasFrame = true;
    update();
}

void FrameVisualWidget::setDarkMode(bool dark)
{
    m_darkMode = dark;
    update();
}

void FrameVisualWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor page = themedColor(m_darkMode, "#f8fafc", "#101827");
    const QColor cardBg = themedColor(m_darkMode, "#ffffff", "#182235");
    const QColor border = themedColor(m_darkMode, "#d8dee8", "#2b3b52");
    const QColor text = themedColor(m_darkMode, "#0f172a", "#e5edf7");
    const QColor muted = themedColor(m_darkMode, "#64748b", "#9aa8ba");
    painter.fillRect(rect(), page);
    painter.setPen(border);
    painter.setBrush(cardBg);
    painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 8, 8);
    painter.setPen(text);
    QFont title = painter.font();
    title.setBold(true);
    painter.setFont(title);
    painter.drawText(16, 26, QStringLiteral("Modbus 帧结构可视化"));
    if (!m_hasFrame || m_entry.payload.size() < 4) {
        painter.setPen(muted);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无有效帧"));
        return;
    }
    struct Part { QString name; QByteArray bytes; QColor color; };
    const QByteArray raw = m_entry.payload;
    const QVector<Part> parts = {
        {QStringLiteral("地址"), raw.left(1), themedColor(m_darkMode, "#dbeafe", "#17305f")},
        {QStringLiteral("功能码"), raw.mid(1, 1), themedColor(m_darkMode, "#dcfce7", "#123f2b")},
        {QStringLiteral("数据区"), raw.mid(2, raw.size() - 4), themedColor(m_darkMode, "#fef3c7", "#493816")},
        {QStringLiteral("CRC低"), raw.mid(raw.size() - 2, 1), themedColor(m_darkMode, "#ede9fe", "#30285f")},
        {QStringLiteral("CRC高"), raw.right(1), themedColor(m_darkMode, "#ede9fe", "#30285f")}
    };
    int x = 16;
    const int y = 58;
    const int h = 58;
    for (const Part &part : parts) {
        const int w = qBound(74, 16 + part.bytes.size() * 28, 210);
        painter.setPen(border);
        painter.setBrush(part.color);
        painter.drawRoundedRect(QRect(x, y, w, h), 7, 7);
        painter.setPen(text);
        painter.drawText(QRect(x + 6, y + 8, w - 12, 16), Qt::AlignCenter, part.name);
        painter.setPen(muted);
        painter.drawText(QRect(x + 6, y + 29, w - 12, 20), Qt::AlignCenter, Core::toHex(part.bytes));
        x += w + 8;
    }
    painter.setPen(Core::verifyCrc(raw) ? QColor("#16a34a") : QColor("#dc2626"));
    painter.drawText(QRect(16, 136, width() - 32, 24), Qt::AlignLeft,
                     QStringLiteral("%1  CRC %2").arg(m_entry.direction, Core::verifyCrc(raw) ? QStringLiteral("正确") : QStringLiteral("错误")));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_collector(1, this),
      m_bus(this),
      m_controller(m_bus.endpoint(), 1, this)
{
    m_bus.setHandler([this](const QByteArray &payload) {
        return m_collector.handleRequest(payload);
    });

    setWindowTitle(QStringLiteral("支持 Modbus-RTU 协议通信的充电系统 - Qt/C++"));
    QFont appFont(QStringLiteral("Microsoft YaHei UI"), 9);
    qApp->setFont(appFont);
    setFont(appFont);
    resize(1220, 720);
    setMinimumSize(1180, 680);
    applyTheme();

    QWidget *central = new QWidget(this);
    central->setObjectName("central");
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    QFrame *header = new QFrame();
    header->setObjectName("header");
    QHBoxLayout *top = new QHBoxLayout(header);
    top->setContentsMargins(14, 9, 14, 9);
    QLabel *title = new QLabel(QStringLiteral("Modbus 充电系统联调台"));
    title->setObjectName("title");
    QLabel *subtitle = new QLabel(QStringLiteral("控制器 + 参数采集器 + 协议监视器"));
    subtitle->setObjectName("subtitle");
    QVBoxLayout *titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(2);
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    m_statusLabel = new QLabel(QStringLiteral("系统就绪"));
    m_statusLabel->setObjectName("statusPill");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_linkModeLabel = new QLabel(QStringLiteral("虚拟链路"));
    m_linkModeLabel->setObjectName("statusPill");
    m_linkModeLabel->setAlignment(Qt::AlignCenter);
    m_themeButton = commandButton(QStringLiteral("深色"));
    m_themeButton->setObjectName("themeButton");
    m_themeButton->setCheckable(true);
    installHelpText(m_themeButton, QStringLiteral("切换浅色/深色界面"));
    top->addLayout(titleBlock);
    top->addStretch();
    top->addWidget(m_linkModeLabel);
    top->addWidget(m_statusLabel);
    top->addWidget(m_themeButton);
    root->addWidget(header);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setObjectName("mainSplitter");
    splitter->addWidget(wrapSidePanel(buildControllerPanel(), 300, 380));
    splitter->addWidget(wrapSidePanel(buildCollectorPanel(), 310, 400));
    splitter->addWidget(buildMonitorPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({320, 340, 900});
    root->addWidget(splitter, 1);
    setCentralWidget(central);

    connect(&m_timer, &QTimer::timeout, this, &MainWindow::refreshUi);
    connect(&m_bus, &Transport::VirtualBus::frameLogged, this, &MainWindow::updateLog);
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    m_timer.start(500);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (!widget) {
        return QMainWindow::eventFilter(watched, event);
    }

    const QString helpText = widget->property("helpText").toString();
    if (helpText.isEmpty()) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Enter || event->type() == QEvent::ToolTip) {
        showHelpText(widget, helpText);
        return event->type() == QEvent::ToolTip;
    }
    if (event->type() == QEvent::Leave) {
        QToolTip::hideText();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::applyTheme()
{
    const QString page = m_darkMode ? "#101827" : "#eef3f8";
    const QString surface = m_darkMode ? "#182235" : "#ffffff";
    const QString surfaceAlt = m_darkMode ? "#202c42" : "#f8fafc";
    const QString border = m_darkMode ? "#2b3b52" : "#d8dee8";
    const QString text = m_darkMode ? "#e5edf7" : "#0f172a";
    const QString muted = m_darkMode ? "#9aa8ba" : "#64748b";
    const QString field = m_darkMode ? "#111c2f" : "#ffffff";
    const QString focus = m_darkMode ? "#60a5fa" : "#2563eb";
    const QString pillBg = m_darkMode ? "#14304b" : "#e0f2fe";
    const QString pillText = m_darkMode ? "#bae6fd" : "#075985";
    const QString dangerBg = m_darkMode ? "#3a1720" : "#fff1f2";
    const QString dangerText = m_darkMode ? "#fecdd3" : "#b91c1c";
    const QString logBg = m_darkMode ? "#08111f" : "#0f172a";
    const QString logText = m_darkMode ? "#bfdbfe" : "#dbeafe";

    QString style = QStringLiteral(R"(
        QWidget { font-family: "Microsoft YaHei UI"; font-size: 9pt; color: %1; }
        QWidget#central { background: %2; color: %1; }
        QFrame#header { background: %3; border: 1px solid %4; border-radius: 8px; }
        QLabel#title { font-size: 14pt; font-weight: 700; color: %1; }
        QLabel#subtitle { color: %5; }
        QLabel#statusPill { min-width: 88px; padding: 4px 8px; color: %6; background: %7; border: 1px solid %4; border-radius: 7px; font-weight: 600; }
        QLabel#panelTitle { font-size: 9pt; font-weight: 700; color: %1; padding: 0 0 4px 0; }
        QLabel#mutedLabel { color: %5; }
        QLabel#strongLabel { color: %1; font-weight: 700; }
        QLabel#summaryNote { color: %5; font-size: 8pt; padding: 2px; }
        QLabel#accentLabel { color: %9; font-size: 9pt; font-weight: 700; }
        QLabel#okLabel { color: #16a34a; font-weight: 700; }
        QLabel { font-size: 9pt; color: %1; }
        QFrame#valueLine { background: transparent; border-bottom: 1px solid %4; }
        QFrame#valueLine QLabel { border: none; background: transparent; }
        QGroupBox { background: %3; color: %1; border: 1px solid %4; border-radius: 8px; margin-top: 9px; padding: 10px 9px 9px 9px; font-size: 9pt; font-weight: 650; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: %5; background: %3; }
        QLineEdit, QSpinBox, QComboBox { min-height: 22px; color: %1; border: 1px solid %4; border-radius: 6px; background: %8; padding: 1px 7px; selection-background-color: %9; font-size: 9pt; }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid %9; }
        QPushButton { min-height: 23px; color: %1; border: 1px solid %4; border-radius: 6px; background: %3; padding: 2px 8px; font-size: 9pt; font-weight: 600; }
        QPushButton:hover { background: %10; border-color: %9; }
        QPushButton:checked, QPushButton#themeButton { color: %6; background: %7; border-color: %9; }
        QPushButton[role="primary"] { color: #ffffff; background: #2563eb; border-color: #2563eb; }
        QPushButton[role="primary"]:hover { background: #1d4ed8; }
        QPushButton[role="danger"] { color: %11; background: %12; border-color: %4; }
        QPushButton[role="danger"]:hover { border-color: #fb7185; }
        QCheckBox { spacing: 6px; font-size: 9pt; color: %1; }
        QCheckBox#compactCheck { spacing: 4px; font-size: 8pt; }
        QPushButton#compactButton { min-height: 22px; padding: 1px 6px; font-size: 8pt; font-weight: 500; }
        QComboBox#compactCombo { min-height: 24px; padding: 1px 6px; font-size: 8pt; border: 1px solid %4; border-radius: 5px; background: %8; color: %1; }
        QProgressBar { height: 13px; color: %1; border: 1px solid %4; border-radius: 7px; background: %10; text-align: center; font-size: 8pt; }
        QProgressBar::chunk { border-radius: 8px; background: #7c3aed; }
        QPlainTextEdit { background: %13; color: %14; border: 1px solid %4; border-radius: 8px; padding: 5px; font-family: Consolas; font-size: 8pt; }
        QTableWidget { background: %3; alternate-background-color: %10; color: %1; gridline-color: %4; border: 1px solid %4; border-radius: 6px; }
        QHeaderView::section { background: %10; color: %1; border: 1px solid %4; padding: 4px; }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
        QSplitter::handle { background: %4; width: 2px; }
        QToolTip { color: %1; background: %3; border: 1px solid %4; padding: 5px; }
    )");
    style = style.arg(text)
                .arg(page)
                .arg(surface)
                .arg(border)
                .arg(muted)
                .arg(pillText)
                .arg(pillBg)
                .arg(field)
                .arg(focus)
                .arg(surfaceAlt)
                .arg(dangerText)
                .arg(dangerBg)
                .arg(logBg)
                .arg(logText);
    qApp->setStyleSheet(style);

    if (m_themeButton) {
        m_themeButton->setText(m_darkMode ? QStringLiteral("浅色") : QStringLiteral("深色"));
        m_themeButton->setChecked(m_darkMode);
    }
    if (m_chart) {
        m_chart->setDarkMode(m_darkMode);
    }
    if (m_dashboardWidget) {
        m_dashboardWidget->setDarkMode(m_darkMode);
    }
}

void MainWindow::toggleTheme()
{
    m_darkMode = !m_darkMode;
    applyTheme();
}

QPushButton *MainWindow::commandButton(const QString &text, const QString &role)
{
    QPushButton *button = new QPushButton(text);
    if (!role.isEmpty()) {
        button->setProperty("role", role);
    }
    return button;
}

QWidget *MainWindow::buildControllerPanel()
{
    QWidget *panel = new QWidget();
    panel->setMinimumWidth(292);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel *caption = new QLabel(QStringLiteral("充电控制器"));
    caption->setObjectName("panelTitle");
    caption->setMinimumHeight(22);
    layout->addWidget(caption);

    QGroupBox *actions = new QGroupBox(QStringLiteral("过程控制"));
    QGridLayout *grid = new QGridLayout(actions);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    m_plugCheck = new QCheckBox(QStringLiteral("插枪"));
    m_cardEdit = new QLineEdit(QStringLiteral("2428403001"));
    m_cardEdit->setMinimumWidth(176);
    m_cardEdit->setMaxLength(10);
    m_cardEdit->setAlignment(Qt::AlignLeft);
    m_cardButton = commandButton(QStringLiteral("刷卡"));
    m_startButton = commandButton(QStringLiteral("启动"), "primary");
    m_stopButton = commandButton(QStringLiteral("停止"), "danger");
    m_pollButton = commandButton(QStringLiteral("查询"));
    for (QPushButton *button : {m_cardButton, m_startButton, m_stopButton, m_pollButton}) {
        button->setMinimumWidth(86);
    }
    m_autoPollCheck = new QCheckBox(QStringLiteral("自动查询"));
    m_autoPollCheck->setChecked(true);
    grid->addWidget(m_plugCheck, 0, 0);
    grid->addWidget(m_cardEdit, 0, 1, 1, 3);
    grid->addWidget(m_cardButton, 1, 0, 1, 2);
    grid->addWidget(m_startButton, 1, 2, 1, 2);
    grid->addWidget(m_stopButton, 2, 0, 1, 2);
    grid->addWidget(m_pollButton, 2, 2, 1, 2);
    grid->addWidget(m_autoPollCheck, 3, 0, 1, 4);
    for (int column = 0; column < 4; ++column) {
        grid->setColumnStretch(column, 1);
    }
    layout->addWidget(actions);

    QGroupBox *readings = new QGroupBox(QStringLiteral("实时状态"));
    QGridLayout *readGrid = new QGridLayout(readings);
    readGrid->setHorizontalSpacing(8);
    readGrid->setVerticalSpacing(4);
    m_voltageLabel = valueLabel(QStringLiteral("电压"), readGrid, 0, 0);
    m_currentLabel = valueLabel(QStringLiteral("电流"), readGrid, 1, 0);
    m_temperatureLabel = valueLabel(QStringLiteral("温度"), readGrid, 2, 0);
    m_batteryLabel = valueLabel(QStringLiteral("电量"), readGrid, 3, 0);
    m_alarmLabel = valueLabel(QStringLiteral("报警"), readGrid, 4, 0);
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    readGrid->addWidget(m_progress, 5, 0);
    layout->addWidget(readings);

    QGroupBox *summary = new QGroupBox(QStringLiteral("运行摘要"));
    QGridLayout *summaryGrid = new QGridLayout(summary);
    summaryGrid->setHorizontalSpacing(8);
    summaryGrid->setVerticalSpacing(3);
    summaryGrid->setContentsMargins(10, 12, 10, 8);
    m_chargeModeLabel = valueLabel(QStringLiteral("模式"), summaryGrid, 0, 0);
    m_marginLabel = valueLabel(QStringLiteral("裕量"), summaryGrid, 1, 0);
    m_runtimeLabel = valueLabel(QStringLiteral("采样次数"), summaryGrid, 2, 0);
    m_energyUsedLabel = valueLabel(QStringLiteral("用电"), summaryGrid, 3, 0);
    m_energyLabel = valueLabel(QStringLiteral("费用"), summaryGrid, 4, 0);
    m_etaLabel = valueLabel(QStringLiteral("预计"), summaryGrid, 5, 0);
    m_riskLabel = valueLabel(QStringLiteral("风险"), summaryGrid, 6, 0);
    QLabel *summaryNote = new QLabel(QStringLiteral("裕量顺序：V电压 / A电流 / T温度"));
    summaryNote->setObjectName("summaryNote");
    summaryNote->setWordWrap(true);
    summaryGrid->addWidget(summaryNote, 7, 0);
    layout->addWidget(summary);
    layout->addStretch();

    connect(m_plugCheck, &QCheckBox::toggled, &m_controller, &Services::ControllerService::setPlugged);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_cardButton, &QPushButton::clicked, this, &MainWindow::onWriteCard);
    connect(m_pollButton, &QPushButton::clicked, this, &MainWindow::onPoll);
    connect(m_cardEdit, &QLineEdit::textChanged, this, [this]() {
        updateActionHints(m_controller.snapshot());
    });
    updateActionHints(m_controller.snapshot());
    return panel;
}

QWidget *MainWindow::buildCollectorPanel()
{
    QWidget *panel = new QWidget();
    panel->setMinimumWidth(300);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel *caption = new QLabel(QStringLiteral("充电参数采集器"));
    caption->setObjectName("panelTitle");
    caption->setMinimumHeight(22);
    layout->addWidget(caption);

    QGroupBox *state = new QGroupBox(QStringLiteral("工作状态"));
    QHBoxLayout *stateLayout = new QHBoxLayout(state);
    m_collectorStateLabel = new QLabel(QStringLiteral("空闲"));
    state->setMaximumHeight(74);
    state->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stateLayout->setContentsMargins(10, 6, 10, 6);
    m_collectorStateLabel->setObjectName("accentLabel");
    m_collectorAlarmLabel = new QLabel(QStringLiteral("正常"));
    m_collectorAlarmLabel->setObjectName("okLabel");
    m_collectorAlarmLabel->setAlignment(Qt::AlignRight);
    stateLayout->addWidget(m_collectorStateLabel);
    stateLayout->addStretch();
    stateLayout->addWidget(m_collectorAlarmLabel);
    layout->addWidget(state);

    QGroupBox *settings = new QGroupBox(QStringLiteral("参数设定与异常注入"));
    QFormLayout *form = new QFormLayout(settings);
    form->setSpacing(6);
    auto makeSpin = [](int value, int max = 1000) {
        QSpinBox *spin = new QSpinBox();
        spin->setRange(1, max);
        spin->setValue(value);
        return spin;
    };
    m_voltageLimitSpin = makeSpin(240);
    m_voltageLimitSpin->setSuffix(QStringLiteral(" V"));
    m_currentLimitSpin = makeSpin(90);
    m_currentLimitSpin->setSuffix(QStringLiteral(" A"));
    m_temperatureLimitSpin = makeSpin(70);
    m_temperatureLimitSpin->setSuffix(QStringLiteral(" °C"));
    m_batteryLimitSpin = makeSpin(200);
    m_batteryLimitSpin->setSuffix(QStringLiteral(" kWh"));
    m_batteryPowerSpin = makeSpin(30);
    m_batteryPowerSpin->setSuffix(QStringLiteral(" kWh"));
    form->addRow(QStringLiteral("电压上限(V)"), m_voltageLimitSpin);
    form->addRow(QStringLiteral("电流上限(A)"), m_currentLimitSpin);
    form->addRow(QStringLiteral("温度上限(°C)"), m_temperatureLimitSpin);
    form->addRow(QStringLiteral("最大电量(kWh)"), m_batteryLimitSpin);
    form->addRow(QStringLiteral("初始电量(kWh)"), m_batteryPowerSpin);
    QPushButton *applyButton = commandButton(QStringLiteral("应用参数"), "primary");
    form->addRow(applyButton);
    QHBoxLayout *presetLayout = new QHBoxLayout();
    QPushButton *slowPreset = commandButton(QStringLiteral("慢充"));
    QPushButton *fastPreset = commandButton(QStringLiteral("快充"));
    QPushButton *safePreset = commandButton(QStringLiteral("安全"));
    for (QPushButton *button : {slowPreset, fastPreset, safePreset}) {
        button->setMinimumWidth(76);
    }
    presetLayout->setSpacing(8);
    presetLayout->addWidget(slowPreset, 1);
    presetLayout->addWidget(fastPreset, 1);
    presetLayout->addWidget(safePreset, 1);
    form->addRow(presetLayout);
    m_overVoltageCheck = new QCheckBox(QStringLiteral("手动过压"));
    m_overCurrentCheck = new QCheckBox(QStringLiteral("手动过流"));
    m_overTemperatureCheck = new QCheckBox(QStringLiteral("手动高温"));
    form->addRow(m_overVoltageCheck, m_overCurrentCheck);
    form->addRow(m_overTemperatureCheck);
    layout->addWidget(settings, 1);

    connect(applyButton, &QPushButton::clicked, this, &MainWindow::onApplySettings);
    connect(slowPreset, &QPushButton::clicked, this, &MainWindow::applyPresetSlow);
    connect(fastPreset, &QPushButton::clicked, this, &MainWindow::applyPresetFast);
    connect(safePreset, &QPushButton::clicked, this, &MainWindow::applyPresetSafe);
    connect(m_overVoltageCheck, &QCheckBox::toggled, this, &MainWindow::onAlarmChanged);
    connect(m_overCurrentCheck, &QCheckBox::toggled, this, &MainWindow::onAlarmChanged);
    connect(m_overTemperatureCheck, &QCheckBox::toggled, this, &MainWindow::onAlarmChanged);
    return panel;
}

QWidget *MainWindow::buildMonitorPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    QLabel *caption = new QLabel(QStringLiteral("协议监视器与曲线"));
    caption->setObjectName("panelTitle");
    caption->setMinimumHeight(22);
    layout->addWidget(caption);

    QSplitter *vertical = new QSplitter(Qt::Vertical, panel);
    m_chart = new ChartWidget();
    vertical->addWidget(m_chart);

    QWidget *bottomPanel = new QWidget();
    QVBoxLayout *bottom = new QVBoxLayout(bottomPanel);
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->setSpacing(6);

    QGroupBox *chartControls = new QGroupBox(QStringLiteral("曲线控制"));
    QGridLayout *chartGrid = new QGridLayout(chartControls);
    chartControls->setMinimumHeight(116);
    chartControls->setMaximumHeight(128);
    chartControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    chartGrid->setContentsMargins(12, 14, 12, 10);
    chartGrid->setHorizontalSpacing(20);
    chartGrid->setVerticalSpacing(10);
    m_showVoltageCheck = new QCheckBox(QStringLiteral("电压"));
    m_showCurrentCheck = new QCheckBox(QStringLiteral("电流"));
    m_showTemperatureCheck = new QCheckBox(QStringLiteral("温度"));
    m_showBatteryCheck = new QCheckBox(QStringLiteral("电量"));
    m_showThresholdsCheck = new QCheckBox(QStringLiteral("阈值线"));
    m_pauseChartCheck = new QCheckBox(QStringLiteral("暂停"));
    m_chartModeCombo = new QComboBox();
    for (QCheckBox *check : {m_showVoltageCheck, m_showCurrentCheck, m_showTemperatureCheck, m_showBatteryCheck, m_showThresholdsCheck, m_pauseChartCheck}) {
        check->setObjectName("compactCheck");
        check->setMinimumWidth(70);
    }
    m_chartModeCombo->setObjectName("compactCombo");
    m_chartModeCombo->setMinimumWidth(150);
    m_chartModeCombo->addItems({QStringLiteral("归一化"), QStringLiteral("原始值")});
    m_showVoltageCheck->setChecked(true);
    m_showCurrentCheck->setChecked(true);
    m_showTemperatureCheck->setChecked(true);
    m_showBatteryCheck->setChecked(true);
    m_showThresholdsCheck->setChecked(true);
    QPushButton *clearChartButton = commandButton(QStringLiteral("清空曲线"));
    clearChartButton->setObjectName("compactButton");
    clearChartButton->setMinimumWidth(96);
    chartGrid->addWidget(m_showVoltageCheck, 0, 0);
    chartGrid->addWidget(m_showCurrentCheck, 0, 1);
    chartGrid->addWidget(m_showTemperatureCheck, 0, 2);
    chartGrid->addWidget(m_showBatteryCheck, 0, 3);
    chartGrid->addWidget(m_showThresholdsCheck, 1, 0);
    chartGrid->addWidget(m_pauseChartCheck, 1, 1);
    chartGrid->addWidget(m_chartModeCombo, 1, 2, 1, 2);
    chartGrid->addWidget(clearChartButton, 1, 4);
    for (int column = 0; column < 5; ++column) {
        chartGrid->setColumnStretch(column, 1);
    }
    bottom->addWidget(chartControls);

    m_logEdit = new QPlainTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logEdit->setFont(QFont("Consolas", 8));
    m_logEdit->setMaximumHeight(68);
    m_logEdit->setMinimumHeight(58);
    bottom->addWidget(m_logEdit);

    QGroupBox *stats = new QGroupBox(QStringLiteral("通信统计"));
    stats->setMinimumHeight(116);
    stats->setMaximumHeight(128);
    stats->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QGridLayout *statsGrid = new QGridLayout(stats);
    statsGrid->setContentsMargins(12, 14, 12, 10);
    statsGrid->setHorizontalSpacing(36);
    statsGrid->setVerticalSpacing(10);
    auto statLine = [](const QString &title, QLabel **valueLabel) {
        QWidget *box = new QWidget();
        QHBoxLayout *line = new QHBoxLayout(box);
        line->setContentsMargins(0, 0, 0, 0);
        QLabel *name = new QLabel(title + QStringLiteral("："));
        name->setObjectName("mutedLabel");
        name->setMinimumWidth(62);
        *valueLabel = new QLabel("-");
        (*valueLabel)->setObjectName("strongLabel");
        (*valueLabel)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        (*valueLabel)->setMinimumWidth(120);
        line->addWidget(name);
        line->addWidget(*valueLabel, 1);
        return box;
    };
    statsGrid->addWidget(statLine(QStringLiteral("TX帧数"), &m_txCountLabel), 0, 0);
    statsGrid->addWidget(statLine(QStringLiteral("RX帧数"), &m_rxCountLabel), 0, 1);
    statsGrid->addWidget(statLine(QStringLiteral("CRC通过"), &m_crcOkLabel), 1, 0);
    statsGrid->addWidget(statLine(QStringLiteral("最后帧"), &m_lastFrameLabel), 1, 1);
    statsGrid->setColumnStretch(0, 1);
    statsGrid->setColumnStretch(1, 1);
    bottom->addWidget(stats);

    QGridLayout *buttons = new QGridLayout();
    buttons->setSpacing(6);
    QPushButton *exportLogButton = commandButton(QStringLiteral("导出日志"));
    QPushButton *exportHistoryButton = commandButton(QStringLiteral("导出曲线"));
    QPushButton *exportBillButton = commandButton(QStringLiteral("导出账单"));
    QPushButton *explainButton = commandButton(QStringLiteral("解释帧"));
    QPushButton *visualButton = commandButton(QStringLiteral("看板"));
    QPushButton *linkButton = commandButton(QStringLiteral("通信"));
    QPushButton *sessionsButton = commandButton(QStringLiteral("会话"));
    QPushButton *labButton = commandButton(QStringLiteral("实验室"), "primary");
    installHelpText(exportLogButton, QStringLiteral("导出通信日志"));
    installHelpText(exportHistoryButton, QStringLiteral("导出曲线数据"));
    installHelpText(exportBillButton, QStringLiteral("导出会话账单"));
    installHelpText(explainButton, QStringLiteral("解释最后一帧"));
    installHelpText(visualButton, QStringLiteral("打开可视化看板"));
    installHelpText(linkButton, QStringLiteral("通信设置"));
    installHelpText(sessionsButton, QStringLiteral("会话记录"));
    installHelpText(labButton, QStringLiteral("协议实验室"));
    for (QPushButton *button : {exportLogButton, exportHistoryButton, exportBillButton, explainButton, visualButton, linkButton, sessionsButton, labButton}) {
        button->setObjectName("compactButton");
        button->setMinimumHeight(24);
        button->setMinimumWidth(64);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMouseTracking(true);
    }
    buttons->addWidget(exportLogButton, 0, 0);
    buttons->addWidget(exportHistoryButton, 0, 1);
    buttons->addWidget(exportBillButton, 0, 2);
    buttons->addWidget(explainButton, 0, 3);
    buttons->addWidget(visualButton, 1, 0);
    buttons->addWidget(linkButton, 1, 1);
    buttons->addWidget(sessionsButton, 1, 2);
    buttons->addWidget(labButton, 1, 3);
    for (int column = 0; column < 4; ++column) {
        buttons->setColumnStretch(column, 1);
    }
    bottom->addLayout(buttons);
    QScrollArea *monitorScroll = new QScrollArea();
    monitorScroll->setWidget(bottomPanel);
    monitorScroll->setWidgetResizable(true);
    monitorScroll->setFrameShape(QFrame::NoFrame);
    monitorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    monitorScroll->setMinimumHeight(220);
    monitorScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vertical->addWidget(monitorScroll);
    vertical->setStretchFactor(0, 1);
    vertical->setStretchFactor(1, 0);
    vertical->setSizes({560, 370});
    bottomPanel->setMinimumHeight(360);
    layout->addWidget(vertical, 1);

    connect(exportLogButton, &QPushButton::clicked, this, &MainWindow::exportLog);
    connect(exportHistoryButton, &QPushButton::clicked, this, &MainWindow::exportHistory);
    connect(exportBillButton, &QPushButton::clicked, this, &MainWindow::exportBill);
    connect(explainButton, &QPushButton::clicked, this, &MainWindow::explainLastFrame);
    connect(visualButton, &QPushButton::clicked, this, &MainWindow::showVisualDashboard);
    connect(linkButton, &QPushButton::clicked, this, &MainWindow::configureLink);
    connect(sessionsButton, &QPushButton::clicked, this, &MainWindow::showSessionRecords);
    connect(labButton, &QPushButton::clicked, this, &MainWindow::openProtocolLab);
    connect(clearChartButton, &QPushButton::clicked, this, &MainWindow::clearChart);
    connect(m_showVoltageCheck, &QCheckBox::toggled, this, &MainWindow::updateChartOptions);
    connect(m_showCurrentCheck, &QCheckBox::toggled, this, &MainWindow::updateChartOptions);
    connect(m_showTemperatureCheck, &QCheckBox::toggled, this, &MainWindow::updateChartOptions);
    connect(m_showBatteryCheck, &QCheckBox::toggled, this, &MainWindow::updateChartOptions);
    connect(m_showThresholdsCheck, &QCheckBox::toggled, this, &MainWindow::updateChartOptions);
    connect(m_chartModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateChartOptions);
    return panel;
}

QLabel *MainWindow::valueLabel(const QString &title, QGridLayout *layout, int row, int col)
{
    QFrame *box = new QFrame();
    box->setObjectName("valueLine");
    QHBoxLayout *line = new QHBoxLayout(box);
    line->setContentsMargins(2, 3, 2, 3);
    QLabel *name = new QLabel(title + QStringLiteral("："));
    name->setObjectName("mutedLabel");
    name->setMinimumWidth(54);
    QLabel *value = new QLabel("-");
    value->setObjectName("strongLabel");
    value->setAlignment(Qt::AlignRight);
    value->setMinimumWidth(92);
    value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    value->setWordWrap(false);
    value->setMouseTracking(true);
    box->setMouseTracking(true);
    name->setMouseTracking(true);
    line->addWidget(name);
    line->addStretch();
    line->addWidget(value);
    layout->addWidget(box, row, col);
    return value;
}

void MainWindow::onStart()
{
    if (m_cardEdit->text().trimmed().isEmpty()) {
        setStatus(QStringLiteral("诊断：卡号为空，请先输入卡号"));
        return;
    }
    const Services::ControllerSnapshot beforeStart = m_controller.snapshot();
    if (beforeStart.alarmText != QStringLiteral("正常") || beforeStart.progress >= 99.0) {
        setStatus(QStringLiteral("诊断：请先点击停止复位，再重新启动"));
        return;
    }
    if (!m_controller.startCharge()) {
        setStatus(m_controller.snapshot().lastError);
        return;
    }
    m_autoPollCheck->setChecked(true);
    const Services::ControllerSnapshot snapshot = m_controller.pollParameters();
    if (snapshot.charging) {
        appendHistory(snapshot);
    }
    setStatus(QStringLiteral("已启动，请刷卡后开始充电"));
}

void MainWindow::onStop()
{
    m_controller.stopCharge();
    if (m_overVoltageCheck) {
        m_overVoltageCheck->setChecked(false);
    }
    if (m_overCurrentCheck) {
        m_overCurrentCheck->setChecked(false);
    }
    if (m_overTemperatureCheck) {
        m_overTemperatureCheck->setChecked(false);
    }
    m_controller.pollParameters();
    finishSession(QStringLiteral("手动停止"));
    setStatus(QStringLiteral("充电已停止"));
    updateActionHints(m_controller.snapshot());
}

void MainWindow::onWriteCard()
{
    if (!m_controller.snapshot().waitingForCard) {
        setStatus(QStringLiteral("诊断：请先启动充电流程，再执行刷卡"));
        return;
    }
    if (m_controller.writeCard(m_cardEdit->text())) {
        const Services::ControllerSnapshot snapshot = m_controller.pollParameters();
        appendHistory(snapshot);
        rememberSessionStart(snapshot);
        setStatus(QStringLiteral("刷卡成功，进入充电状态"));
    } else {
        setStatus(m_controller.snapshot().lastError);
    }
}

void MainWindow::onPoll()
{
    const Services::ControllerSnapshot snapshot = m_controller.pollParameters();
    appendHistory(snapshot);
    setStatus(snapshot.lastError.isEmpty() ? QStringLiteral("查询完成") : snapshot.lastError);
}

void MainWindow::onApplySettings()
{
    QString validationMessage;
    if (!validateSettings(&validationMessage)) {
        setStatus(validationMessage);
        return;
    }
    Core::ChargingParameters &p = m_collector.simulator().parameters();
    p.voltageLimit = m_voltageLimitSpin->value();
    p.currentLimit = m_currentLimitSpin->value();
    p.temperatureLimit = m_temperatureLimitSpin->value();
    p.batteryPowerLimit = m_batteryLimitSpin->value();
    p.batteryPower = m_batteryPowerSpin->value();
    p.initialPower = p.batteryPower;
    m_controller.pollParameters();
    resetChartForParameterChange(QStringLiteral("采集器参数已应用，曲线已重置"));
}

void MainWindow::applyPresetSlow()
{
    m_voltageLimitSpin->setValue(220);
    m_currentLimitSpin->setValue(60);
    m_temperatureLimitSpin->setValue(65);
    m_batteryLimitSpin->setValue(200);
    onApplySettings();
    setStatus(QStringLiteral("已应用慢充预设，曲线已重置"));
}

void MainWindow::applyPresetFast()
{
    m_voltageLimitSpin->setValue(300);
    m_currentLimitSpin->setValue(140);
    m_temperatureLimitSpin->setValue(78);
    m_batteryLimitSpin->setValue(240);
    onApplySettings();
    setStatus(QStringLiteral("已应用快充预设，曲线已重置"));
}

void MainWindow::applyPresetSafe()
{
    m_voltageLimitSpin->setValue(180);
    m_currentLimitSpin->setValue(55);
    m_temperatureLimitSpin->setValue(55);
    m_batteryLimitSpin->setValue(180);
    onApplySettings();
    setStatus(QStringLiteral("已应用安全预设，曲线已重置"));
}

void MainWindow::onAlarmChanged()
{
    m_collector.simulator().setManualAlarms(m_overVoltageCheck->isChecked(),
                                            m_overCurrentCheck->isChecked(),
                                            m_overTemperatureCheck->isChecked());
    setStatus(diagnosisText(m_controller.snapshot()));
}

void MainWindow::refreshUi()
{
    m_collector.simulator().tick();
    if (m_autoPollCheck->isChecked() && (m_controller.snapshot().charging || m_controller.snapshot().waitingForCard)) {
        appendHistory(m_controller.pollParameters());
    }
    const Services::ControllerSnapshot snapshot = m_controller.snapshot();
    if (m_hasActiveSession && !snapshot.charging && !snapshot.waitingForCard) {
        const QString result = snapshot.progress >= 99.0 ? QStringLiteral("满电完成")
                                                         : (snapshot.alarmText == QStringLiteral("正常") ? QStringLiteral("已停止") : snapshot.alarmText);
        finishSession(result);
    }
    m_voltageLabel->setText(QString("%1/%2 V").arg(snapshot.voltage).arg(snapshot.voltageLimit));
    m_currentLabel->setText(QString("%1/%2 A").arg(snapshot.current).arg(snapshot.currentLimit));
    m_temperatureLabel->setText(QString("%1/%2 °C").arg(snapshot.temperature).arg(snapshot.temperatureLimit));
    m_batteryLabel->setText(QString("%1/%2 kWh").arg(snapshot.batteryPower).arg(snapshot.batteryPowerLimit));
    m_alarmLabel->setText(snapshot.alarmText);
    m_progress->setValue(qRound(snapshot.progress));
    const QString mode = snapshot.progress < 80.0 ? QStringLiteral("恒流") : QStringLiteral("恒压");
    const int voltageMargin = snapshot.voltageLimit - snapshot.voltage;
    const int currentMargin = snapshot.currentLimit - snapshot.current;
    const int temperatureMargin = snapshot.temperatureLimit - snapshot.temperature;
    const int minMargin = qMin(voltageMargin, qMin(currentMargin, temperatureMargin));
    m_chargeModeLabel->setText(mode);
    m_marginLabel->setText(QString("V%1/A%2/T%3").arg(voltageMargin).arg(currentMargin).arg(temperatureMargin));
    installHelpText(m_marginLabel, QStringLiteral("电压裕量 %1，电流裕量 %2，温度裕量 %3")
                                       .arg(voltageMargin)
                                       .arg(currentMargin)
                                       .arg(temperatureMargin));
    m_runtimeLabel->setText(QString("%1 次").arg(m_snapshots.size()));
    double energy = 0.0;
    if (m_snapshots.size() >= 2) {
        energy = qMax(0, m_snapshots.last().batteryPower - m_snapshots.first().batteryPower);
    }
    m_energyUsedLabel->setText(QString("%1 kWh").arg(energy, 0, 'f', 1));
    const double fee = energy * 1.2;
    m_energyLabel->setText(QString("%1元").arg(fee, 0, 'f', 2));
    installHelpText(m_energyLabel, QStringLiteral("本次电量 %1 kWh，估算电费 %2 元")
                                       .arg(energy, 0, 'f', 1)
                                       .arg(fee, 0, 'f', 2));
    QString etaText = QStringLiteral("--");
    if (m_snapshots.size() >= 4 && snapshot.charging && snapshot.batteryPower < snapshot.batteryPowerLimit) {
        const int firstIndex = qMax(0, m_snapshots.size() - 10);
        const double deltaPower = m_snapshots.last().batteryPower - m_snapshots[firstIndex].batteryPower;
        const double deltaSeconds = qMax(0.5, (m_snapshots.size() - 1 - firstIndex) * 0.5);
        const double rate = deltaPower / deltaSeconds;
        if (rate > 0.01) {
            const int seconds = qCeil((snapshot.batteryPowerLimit - snapshot.batteryPower) / rate);
            etaText = QString("%1分%2秒").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
        }
    }
    m_etaLabel->setText(etaText);
    QString riskText = QStringLiteral("正常");
    if (snapshot.alarmText != QStringLiteral("正常") || minMargin < 0) {
        riskText = QStringLiteral("停机风险");
    } else if (voltageMargin < snapshot.voltageLimit * 0.10
               || currentMargin < snapshot.currentLimit * 0.10
               || temperatureMargin < snapshot.temperatureLimit * 0.10) {
        riskText = QStringLiteral("接近上限");
    } else if (snapshot.temperature >= 55 || snapshot.progress >= 90.0) {
        riskText = QStringLiteral("需关注");
    }
    m_riskLabel->setText(riskText);
    m_collectorStateLabel->setText(m_collector.simulator().stateText());
    m_collectorAlarmLabel->setText(m_collector.simulator().effectiveAlarmText());
    const Services::ControllerSnapshot latestSnapshot = m_controller.snapshot();
    updateActionHints(latestSnapshot);
    const QString diagnosis = diagnosisText(snapshot);
    if (snapshot.charging || snapshot.waitingForCard || snapshot.alarmText != QStringLiteral("正常") || snapshot.progress >= 99.0) {
        setStatus(diagnosis);
    }
}

void MainWindow::updateLog()
{
    QStringList lines;
    const QVector<Transport::BusLogEntry> entries = m_bus.log();
    int txCount = 0;
    int rxCount = 0;
    int crcOk = 0;
    const int start = qMax(0, entries.size() - 120);
    for (int i = start; i < entries.size(); ++i) {
        lines << entries[i].format();
    }
    for (const Transport::BusLogEntry &entry : entries) {
        if (entry.direction == "TX") {
            ++txCount;
        } else if (entry.direction == "RX") {
            ++rxCount;
        }
        if (Core::verifyCrc(entry.payload)) {
            ++crcOk;
        }
    }
    m_logEdit->setPlainText(lines.join('\n'));
    m_logEdit->moveCursor(QTextCursor::End);
    m_txCountLabel->setText(QString::number(txCount));
    m_rxCountLabel->setText(QString::number(rxCount));
    m_crcOkLabel->setText(QString("%1 / %2").arg(crcOk).arg(entries.size()));
    m_lastFrameLabel->setText(entries.isEmpty() ? "-" : QString("%1 %2").arg(entries.last().direction, Core::toHex(entries.last().payload.left(4))));
}

void MainWindow::explainLastFrame()
{
    const QVector<Transport::BusLogEntry> entries = m_bus.log();
    if (entries.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("帧解释"), QStringLiteral("暂无通信帧"));
        return;
    }
    const auto entry = entries.last();
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("帧解释"));
    dialog.resize(760, 430);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    FrameVisualWidget *visual = new FrameVisualWidget();
    visual->setDarkMode(m_darkMode);
    visual->setFrame(entry);
    layout->addWidget(visual);
    QPlainTextEdit *detail = new QPlainTextEdit();
    detail->setReadOnly(true);
    detail->setMaximumHeight(150);
    detail->setPlainText(entry.direction + " " + Core::toHex(entry.payload) + "\n\n" + Core::explainFrame(entry.payload));
    layout->addWidget(detail);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    dialog.exec();
}

void MainWindow::exportLog()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出通信日志"), "modbus_log.txt", "Text (*.txt)");
    if (!path.isEmpty()) {
        m_exportService.exportBusLog(m_bus.log(), path);
    }
}

void MainWindow::exportHistory()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出曲线数据"), "charging_history.csv", "CSV (*.csv)");
    if (!path.isEmpty()) {
        m_exportService.exportHistory(m_snapshots, path);
    }
}

void MainWindow::exportBill()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出会话账单"), "charging_bill.csv", "CSV (*.csv)");
    if (!path.isEmpty()) {
        m_exportService.exportBill(m_cardEdit->text(), m_snapshots, path);
    }
}

void MainWindow::configureLink()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("通信设置"));
    dialog.resize(360, 180);
    QFormLayout *form = new QFormLayout(&dialog);
    QComboBox *modeCombo = new QComboBox();
    modeCombo->addItems({QStringLiteral("虚拟链路"), QStringLiteral("真实串口")});
    modeCombo->setCurrentIndex(m_serialMode ? 1 : 0);
    QComboBox *portCombo = new QComboBox();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        portCombo->addItem(info.portName() + QStringLiteral(" - ") + info.description(), info.portName());
    }
    QComboBox *baudCombo = new QComboBox();
    baudCombo->addItems({QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("115200")});
    baudCombo->setCurrentText(QStringLiteral("9600"));
    form->addRow(QStringLiteral("通信模式"), modeCombo);
    form->addRow(QStringLiteral("串口"), portCombo);
    form->addRow(QStringLiteral("波特率"), baudCombo);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (modeCombo->currentIndex() == 0) {
        if (m_serial.isOpen()) {
            m_serial.close();
        }
        m_serialMode = false;
        m_controller.setEndpoint(m_bus.endpoint());
        m_linkModeLabel->setText(QStringLiteral("虚拟链路"));
        setStatus(QStringLiteral("已切换到虚拟链路"));
        return;
    }
    if (portCombo->count() == 0) {
        setStatus(QStringLiteral("诊断：未发现可用串口"));
        return;
    }
    if (m_serial.isOpen()) {
        m_serial.close();
    }
    m_serial.setPortName(portCombo->currentData().toString());
    m_serial.setBaudRate(baudCombo->currentText().toInt());
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial.open(QIODevice::ReadWrite)) {
        setStatus(QStringLiteral("串口打开失败：%1").arg(m_serial.errorString()));
        return;
    }
    m_serialMode = true;
    m_controller.setEndpoint(Transport::Endpoint([this](const QByteArray &payload) {
        return exchangeSerialFrame(payload);
    }));
    m_linkModeLabel->setText(QStringLiteral("串口 %1").arg(m_serial.portName()));
    setStatus(QStringLiteral("已切换到真实串口"));
}

void MainWindow::showSessionRecords()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("会话记录"));
    dialog.resize(760, 560);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    DashboardWidget *miniChart = new DashboardWidget();
    miniChart->setDarkMode(m_darkMode);
    miniChart->setMaximumHeight(250);
    miniChart->setSnapshot(currentSnapshotForVisuals());
    miniChart->setSessions(m_sessionRecords);
    layout->addWidget(miniChart);
    QTableWidget *table = new QTableWidget(m_sessionRecords.size(), 6);
    table->setHorizontalHeaderLabels({QStringLiteral("开始时间"), QStringLiteral("卡号"), QStringLiteral("开始电量"),
                                      QStringLiteral("结束电量"), QStringLiteral("费用"), QStringLiteral("结果")});
    for (int row = 0; row < m_sessionRecords.size(); ++row) {
        const SessionRecord &r = m_sessionRecords[row];
        table->setItem(row, 0, new QTableWidgetItem(r.startedAt));
        table->setItem(row, 1, new QTableWidgetItem(r.cardId));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(r.startPower)));
        table->setItem(row, 3, new QTableWidgetItem(QString::number(r.endPower)));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(r.fee, 'f', 2)));
        table->setItem(row, 5, new QTableWidgetItem(r.result));
    }
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(table);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    dialog.exec();
}

void MainWindow::showVisualDashboard()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("可视化看板"));
    dialog.resize(760, 520);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    DashboardWidget *dashboard = new DashboardWidget();
    dashboard->setDarkMode(m_darkMode);
    dashboard->setSnapshot(currentSnapshotForVisuals());
    dashboard->setSessions(m_sessionRecords);
    layout->addWidget(dashboard);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    dialog.exec();
}

void MainWindow::openProtocolLab()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Modbus 协议实验室"));
    dialog.resize(820, 560);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QPlainTextEdit *edit = new QPlainTextEdit();
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setFont(QFont("Consolas", 10));
    edit->setPlainText(m_labService.examplesText(m_controller.address()));
    layout->addWidget(edit);
    dialog.exec();
}

void MainWindow::clearChart()
{
    m_chartSamples.clear();
    m_snapshots.clear();
    m_chart->setSamples(m_chartSamples);
    setStatus(QStringLiteral("曲线已清空"));
}

void MainWindow::updateChartOptions()
{
    m_chart->setOptions(m_showVoltageCheck->isChecked(),
                        m_showCurrentCheck->isChecked(),
                        m_showTemperatureCheck->isChecked(),
                        m_showBatteryCheck->isChecked(),
                        m_chartModeCombo->currentIndex() == 0,
                        m_showThresholdsCheck->isChecked());
}

void MainWindow::installHelpText(QWidget *widget, const QString &text)
{
    if (!widget) {
        return;
    }
    auto installOne = [this, &text](QWidget *target) {
        if (!target) {
            return;
        }
        target->setToolTip(text);
        target->setProperty("helpText", text);
        target->setMouseTracking(true);
        target->removeEventFilter(this);
        target->installEventFilter(this);
    };
    installOne(widget);
    if (widget->parentWidget()) {
        installOne(widget->parentWidget());
    }
    const QList<QWidget *> children = widget->parentWidget()
                                      ? widget->parentWidget()->findChildren<QWidget *>()
                                      : widget->findChildren<QWidget *>();
    for (QWidget *child : children) {
        installOne(child);
    }
}

void MainWindow::showHelpText(QWidget *widget, const QString &text)
{
    if (!widget || text.isEmpty()) {
        return;
    }
    setStatus(text);
    const QPoint pos = widget->mapToGlobal(QPoint(qMin(widget->width() - 4, 16), qMax(4, widget->height() / 2)));
    QToolTip::showText(pos, text, widget);
}

void MainWindow::setStatus(const QString &text)
{
    m_statusLabel->setText(text.isEmpty() ? QStringLiteral("系统就绪") : text);
}

void MainWindow::updateActionHints(const Services::ControllerSnapshot &snapshot)
{
    if (!m_startButton || !m_stopButton || !m_cardButton || !m_pollButton) {
        return;
    }
    const bool hasCard = !m_cardEdit->text().trimmed().isEmpty();
    const bool terminalState = snapshot.alarmText != QStringLiteral("正常") || snapshot.progress >= 99.0;
    const bool canStart = snapshot.plugged && !snapshot.charging && !snapshot.waitingForCard && hasCard && !terminalState;
    const bool canStopOrReset = snapshot.charging || snapshot.waitingForCard
                                || terminalState;
    m_startButton->setEnabled(canStart);
    m_stopButton->setEnabled(canStopOrReset);
    m_cardButton->setEnabled(hasCard && snapshot.waitingForCard);
    m_pollButton->setEnabled(true);

    m_startButton->setToolTip(canStart ? QStringLiteral("启动充电流程")
                                       : QStringLiteral("请先输入卡号并勾选插枪，且当前未在充电"));
    m_stopButton->setToolTip(canStopOrReset ? QStringLiteral("停止或复位当前流程")
                                            : QStringLiteral("当前未在充电"));
    m_cardButton->setToolTip(hasCard ? QStringLiteral("写入卡号并开始充电")
                                     : QStringLiteral("请输入卡号"));
    m_pollButton->setToolTip(QStringLiteral("读取采集器实时参数"));
}

QString MainWindow::diagnosisText(const Services::ControllerSnapshot &snapshot) const
{
    if (!snapshot.plugged) {
        return QStringLiteral("诊断：未插枪，请先勾选插枪后启动");
    }
    if (snapshot.alarmText != QStringLiteral("正常")) {
        return QStringLiteral("诊断：%1，建议解除异常注入或下调参数后重新启动").arg(snapshot.alarmText);
    }
    if (snapshot.progress >= 99.0) {
        return QStringLiteral("诊断：电量已满，系统已自动停机");
    }
    if (snapshot.waitingForCard) {
        return QStringLiteral("诊断：已启动，等待刷卡后开始充电");
    }
    if (!snapshot.charging) {
        return QStringLiteral("诊断：待机，可点击启动并刷卡");
    }
    const int voltageMargin = snapshot.voltageLimit - snapshot.voltage;
    const int currentMargin = snapshot.currentLimit - snapshot.current;
    const int temperatureMargin = snapshot.temperatureLimit - snapshot.temperature;
    if (voltageMargin < snapshot.voltageLimit * 0.10
        || currentMargin < snapshot.currentLimit * 0.10
        || temperatureMargin < snapshot.temperatureLimit * 0.10) {
        return QStringLiteral("诊断：接近安全上限，建议关注裕量或切换安全预设");
    }
    return QStringLiteral("诊断：充电正常，参数在安全范围内");
}

bool MainWindow::validateSettings(QString *message) const
{
    auto fail = [message](const QString &text) {
        if (message) {
            *message = text;
        }
        return false;
    };
    if (m_batteryPowerSpin->value() >= m_batteryLimitSpin->value()) {
        return fail(QStringLiteral("参数错误：初始电量必须小于最大电量"));
    }
    if (m_voltageLimitSpin->value() < 120) {
        return fail(QStringLiteral("参数错误：电压上限过低，建议不低于 120"));
    }
    if (m_currentLimitSpin->value() < 10) {
        return fail(QStringLiteral("参数错误：电流上限过低，建议不低于 10"));
    }
    if (m_temperatureLimitSpin->value() < 35) {
        return fail(QStringLiteral("参数错误：温度上限过低，建议不低于 35"));
    }
    return true;
}

void MainWindow::rememberSessionStart(const Services::ControllerSnapshot &snapshot)
{
    m_activeSession = {};
    m_activeSession.startedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    m_activeSession.cardId = m_cardEdit->text();
    m_activeSession.startPower = snapshot.batteryPower;
    m_activeSession.endPower = snapshot.batteryPower;
    m_activeSession.result = QStringLiteral("进行中");
    m_hasActiveSession = true;
}

void MainWindow::finishSession(const QString &result)
{
    if (!m_hasActiveSession) {
        return;
    }
    const Services::ControllerSnapshot snapshot = m_controller.snapshot();
    m_activeSession.endPower = snapshot.batteryPower;
    const int energy = qMax(0, m_activeSession.endPower - m_activeSession.startPower);
    m_activeSession.fee = energy * 1.2;
    m_activeSession.result = result;
    m_sessionRecords.prepend(m_activeSession);
    while (m_sessionRecords.size() > 20) {
        m_sessionRecords.removeLast();
    }
    m_hasActiveSession = false;
}

QByteArray MainWindow::exchangeSerialFrame(const QByteArray &payload)
{
    m_bus.appendLog(QStringLiteral("TX"), payload);
    if (!m_serial.isOpen()) {
        setStatus(QStringLiteral("诊断：串口未打开，已切回虚拟链路"));
        m_serialMode = false;
        m_controller.setEndpoint(m_bus.endpoint());
        m_linkModeLabel->setText(QStringLiteral("虚拟链路"));
        return {};
    }
    m_serial.write(payload);
    if (!m_serial.waitForBytesWritten(200)) {
        setStatus(QStringLiteral("串口发送超时"));
        return {};
    }
    if (!m_serial.waitForReadyRead(500)) {
        setStatus(QStringLiteral("串口响应超时"));
        return {};
    }
    QByteArray response = m_serial.readAll();
    while (m_serial.waitForReadyRead(40)) {
        response += m_serial.readAll();
    }
    if (!response.isEmpty()) {
        m_bus.appendLog(QStringLiteral("RX"), response);
    }
    return response;
}

Services::ControllerSnapshot MainWindow::currentSnapshotForVisuals() const
{
    Services::ControllerSnapshot snapshot = m_controller.snapshot();
    const Core::ChargingParameters &p = m_collector.simulator().parameters();
    if (snapshot.voltageLimit <= 0) {
        snapshot.voltage = p.voltage;
        snapshot.current = p.current;
        snapshot.voltageLimit = p.voltageLimit;
        snapshot.currentLimit = p.currentLimit;
        snapshot.temperature = p.temperature;
        snapshot.temperatureLimit = p.temperatureLimit;
        snapshot.batteryPower = p.batteryPower;
        snapshot.batteryPowerLimit = p.batteryPowerLimit;
        snapshot.progress = qMin(100.0, snapshot.batteryPower * 100.0 / qMax(1, snapshot.batteryPowerLimit));
        snapshot.alarmText = m_collector.simulator().effectiveAlarmText();
    }
    return snapshot;
}

ChartSample MainWindow::normalizeSnapshot(const Services::ControllerSnapshot &snapshot) const
{
    ChartSample sample;
    sample.voltage = snapshot.voltage;
    sample.current = snapshot.current;
    sample.temperature = snapshot.temperature;
    sample.battery = snapshot.batteryPower;
    sample.voltageLimit = m_voltageLimitSpin ? m_voltageLimitSpin->value() : snapshot.voltageLimit;
    sample.currentLimit = m_currentLimitSpin ? m_currentLimitSpin->value() : snapshot.currentLimit;
    sample.temperatureLimit = m_temperatureLimitSpin ? m_temperatureLimitSpin->value() : snapshot.temperatureLimit;
    sample.batteryLimit = m_batteryLimitSpin ? m_batteryLimitSpin->value() : snapshot.batteryPowerLimit;
    return sample;
}

void MainWindow::resetChartForParameterChange(const QString &message)
{
    m_chartSamples.clear();
    m_snapshots.clear();
    if (m_chart) {
        m_chart->setSamples(m_chartSamples);
        updateChartOptions();
    }
    setStatus(message);
}

void MainWindow::appendHistory(const Services::ControllerSnapshot &snapshot)
{
    if (m_pauseChartCheck && m_pauseChartCheck->isChecked()) {
        return;
    }
    if (snapshot.batteryPowerLimit <= 1 || snapshot.voltageLimit <= 0 || snapshot.currentLimit <= 0 || snapshot.temperatureLimit <= 0) {
        return;
    }
    if (!snapshot.charging) {
        return;
    }
    m_snapshots.append(snapshot);
    m_chartSamples.append(normalizeSnapshot(snapshot));
    while (m_chartSamples.size() > MaxHistorySamples) {
        m_chartSamples.removeFirst();
    }
    m_chart->setSamples(m_chartSamples);
    updateChartOptions();
}

} // namespace Ui
