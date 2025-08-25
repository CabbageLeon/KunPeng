#include "Home.h"
#include <QFileDialog>
#include <QMessageBox>

Home::Home(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_startBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_resultEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_voiceRecognition(nullptr)
{
    setupUI();
    connectSignals();
}

Home::~Home()
{
    // Qt的父子关系会自动清理UI组件
}

void Home::setupUI()
{
    setWindowTitle("实时语音识别Demo");
    resize(600, 400);
    
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    
    // 标题
    QLabel *titleLabel = new QLabel("实时语音识别系统");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; margin: 10px;");
    m_mainLayout->addWidget(titleLabel);
    
    // 连接提示
    QLabel *connectionInfo = new QLabel("科大讯飞语音识别 - 使用HTTP API，无需SSL配置");
    connectionInfo->setStyleSheet("color: #28a745; background-color: #d4edda; padding: 8px; border-radius: 4px;");
    connectionInfo->setWordWrap(true);
    m_mainLayout->addWidget(connectionInfo);
    
    // 状态显示
    m_statusLabel = new QLabel("状态: 就绪");
    m_statusLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0; border-radius: 3px;");
    m_mainLayout->addWidget(m_statusLabel);
    
    // 控制按钮
    m_startBtn = new QPushButton("开始实时识别");
    m_startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 10px; font-size: 14px; border-radius: 5px; } QPushButton:hover { background-color: #45a049; }");
    
    m_stopBtn = new QPushButton("停止识别");
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 10px; font-size: 14px; border-radius: 5px; } QPushButton:hover { background-color: #da190b; }");
    m_stopBtn->setEnabled(false);
    
    m_mainLayout->addWidget(m_startBtn);
    m_mainLayout->addWidget(m_stopBtn);
    
    // 结果显示区域
    m_mainLayout->addWidget(new QLabel("识别结果:"));
    m_resultEdit = new QTextEdit();
    m_resultEdit->setReadOnly(true);
    m_resultEdit->setStyleSheet("font-family: '微软雅黑'; font-size: 12px; padding: 10px;");
    m_mainLayout->addWidget(m_resultEdit);
    
    // 使用说明
    QLabel *infoLabel = new QLabel("说明: 点击开始按钮后对着麦克风说话，识别结果将实时显示在上方文本框中。");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; font-size: 11px; padding: 5px;");
    m_mainLayout->addWidget(infoLabel);
    
    // 创建语音识别对象
    m_voiceRecognition = new VoiceRecognition(this);
}

void Home::connectSignals()
{
    // 连接按钮信号
    connect(m_startBtn, &QPushButton::clicked, this, &Home::onStartRealtimeRecognition);
    connect(m_stopBtn, &QPushButton::clicked, this, &Home::onStopRecognition);
    
    // 连接语音识别信号
    connect(m_voiceRecognition, &VoiceRecognition::recognitionResult, 
            this, &Home::onRecognitionResult);
    connect(m_voiceRecognition, &VoiceRecognition::recognitionError, 
            this, &Home::onRecognitionError);
    connect(m_voiceRecognition, &VoiceRecognition::recognitionFinished, 
            this, &Home::onRecognitionFinished);
}

void Home::onStartRealtimeRecognition()
{
    // 开始实时识别
    m_voiceRecognition->startRealtimeRecognition();
    
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_statusLabel->setText("状态: 正在识别中...");
    m_statusLabel->setStyleSheet("padding: 5px; background-color: #e7f3ff; border-radius: 3px; color: #0066cc;");
    m_resultEdit->clear();
    m_resultEdit->append("开始实时语音识别，请对着麦克风说话...\n");
}

void Home::onStopRecognition()
{
    m_voiceRecognition->stopRecognition();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("状态: 已停止");
    m_statusLabel->setStyleSheet("padding: 5px; background-color: #fff2e7; border-radius: 3px; color: #cc6600;");
    m_resultEdit->append("识别已停止\n");
}

void Home::onRecognitionResult(const QString &result)
{
    m_resultEdit->append("识别到: " + result);
    // 自动滚动到底部
    QTextCursor cursor = m_resultEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_resultEdit->setTextCursor(cursor);
}

void Home::onRecognitionError(const QString &error)
{
    m_resultEdit->append("错误: " + error);
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("状态: 发生错误");
    m_statusLabel->setStyleSheet("padding: 5px; background-color: #ffe7e7; border-radius: 3px; color: #cc0000;");
}

void Home::onRecognitionFinished()
{
    m_resultEdit->append("识别完成\n");
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("状态: 就绪");
    m_statusLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0; border-radius: 3px;");
}
