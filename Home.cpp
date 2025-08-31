#include "Home.h"
#include <QApplication>
#include <QFileInfo>
#include <QDebug>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QMovie>
#include <QFile>
#include <QDateTime>

Home::Home(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_gifLabel(nullptr)
    , m_gifMovie(nullptr)
    , m_awakeMovie(nullptr)
    , m_videoPlaceholder(nullptr)
    , m_animationTimer(nullptr)
    , m_opacityAnimation(nullptr)
    , m_instructionLabel(nullptr)
    , m_option1Label(nullptr)
    , m_option2Label(nullptr)
    , m_recognitionResultLabel(nullptr)
    , m_voiceRecognition(nullptr)
    , m_voiceprintResultLabel(nullptr)
    , m_backendButton(nullptr)
    , m_backendWindow(nullptr)
    , m_isWakeUpActivated(false)
    , m_isRecognitionActive(false)
    , m_isProcessingCommand(false)
    , m_voiceprintTimer(new QTimer(this))
    , m_wakeupResetTimer(new QTimer(this))
    , m_voiceprintApi(nullptr)
    , m_groupName("student")
    , m_isProcessingVoiceprint(false)
    , m_hasVisitorAdded(false)
{
    setupUI();
    
    // 创建Backend实例用于声纹识别（但不显示）
    m_backendWindow = new Backend();
    
    // 创建独立的声纹识别API实例
    m_voiceprintApi = new VoiceprintRequest(this);
    m_voiceprintApi->setCredentials("581ffbe4", "c43e133e41862c3aa2495bae6c2268ef", "OTgxYWRlNDdiYzFmZTBhNDRhM2NlYTE1");
    m_groupName = "student"; // 使用与Backend相同的组名
    m_isProcessingVoiceprint = false;
    
    // 连接声纹识别API的信号
    connect(m_voiceprintApi, &VoiceprintRequest::requestFinished,
            this, &Home::onVoiceprintApiFinished);
    connect(m_voiceprintApi, &VoiceprintRequest::requestError,
            this, &Home::onVoiceprintApiError);
    
    // 连接Backend的声纹识别结果信号到Home的显示更新（保留作为备用）
    connect(m_backendWindow, &Backend::voiceprintRecognitionResult,
            this, [this](const QString &result) {
                qDebug() << "收到Backend声纹识别结果:" << result;
                updateVoiceprintResult(result, false);
            });
    
    connect(m_backendWindow, &Backend::voiceprintRecognitionError,
            this, [this](const QString &error) {
                qDebug() << "收到Backend声纹识别错误:" << error;
                updateVoiceprintResult(QString("声纹识别错误: %1").arg(error), true);
            });
    
    connectSignals();
    startContinuousRecognition();
}

Home::~Home()
{
    // 停止声纹识别定时器
    if (m_voiceprintTimer) {
        m_voiceprintTimer->stop();
    }
    
    // 停止语音识别
    if (m_voiceRecognition) {
        m_voiceRecognition->stopRecognition();
        disconnect(m_voiceRecognition, nullptr, this, nullptr); // 断开所有来自语音识别的信号
    }
    
    // 停止动画
    if (m_opacityAnimation) {
        m_opacityAnimation->stop();
    }
    
    // 停止GIF动画
    if (m_gifMovie) {
        m_gifMovie->stop();
    }
    
    // 停止唤醒动画
    if (m_awakeMovie) {
        m_awakeMovie->stop();
    }
    
    // 停止语音识别
    if (m_voiceRecognition) {
        m_voiceRecognition->stopRecognition();
    }
    
    // 清理声纹识别API
    if (m_voiceprintApi) {
        m_voiceprintApi->deleteLater();
        m_voiceprintApi = nullptr;
    }
    
    // 清理后台管理窗口
    if (m_backendWindow) {
        m_backendWindow->close();
        delete m_backendWindow;
        m_backendWindow = nullptr;
    }
    
    // Qt会自动清理所有以this为parent的对象
}

void Home::setupUI()
{
    setWindowTitle("鲲鹏智能助手");
    setMinimumSize(1200, 800);
    
    // 设置整个窗口的背景色
    setStyleSheet("QWidget { background-color: rgb(223, 228, 229); }");
    
    // 创建网格布局
    m_mainLayout = new QGridLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 左上角GIF动画播放器
    m_gifLabel = new QLabel(this);
    m_gifLabel->setFixedSize(400, 300);
    m_gifLabel->setStyleSheet(
        "QLabel {"
        "    border: 2px solid #34495e;"
        "    border-radius: 10px;"
        "    background-color: #2c3e50;"
        "}"
    );
    m_gifLabel->setAlignment(Qt::AlignCenter);
    m_gifLabel->setScaledContents(true); // 自动缩放内容填充标签
    
    // 设置GIF文件路径
    QString gifPath = "D:/qt/KunPeng/material/waiting.gif";
    qDebug() << "GIF文件路径:" << gifPath;
    
    QFileInfo gifFile(gifPath);
    if (gifFile.exists()) {
        m_gifMovie = new QMovie(gifPath, QByteArray(), this);
        
        // 检查GIF是否有效
        if (m_gifMovie->isValid()) {
            // 设置缩放模式，确保GIF填充整个标签
            m_gifMovie->setScaledSize(m_gifLabel->size());
            m_gifLabel->setMovie(m_gifMovie);
            m_gifMovie->start();
            qDebug() << "GIF动画开始播放";
        } else {
            qDebug() << "GIF文件无效";
            showVideoPlaceholder();
        }
    } else {
        qDebug() << "GIF文件不存在:" << gifPath;
        showVideoPlaceholder();
    }
    
    // 右侧最上方的指导标签
    m_instructionLabel = new QLabel("我是鲲鹏 你可以对我说你好鲲鹏来和我对话", this);
    m_instructionLabel->setWordWrap(true);
    m_instructionLabel->setAlignment(Qt::AlignCenter);
    m_instructionLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "    background-color: rgba(255, 255, 255, 200);"
        "    padding: 15px;"
        "    border-radius: 10px;"
        "    border: 2px solid #bdc3c7;"
        "}"
    );
    m_instructionLabel->setFixedHeight(80);
    
    // 选项1标签 - 圆角矩形包围
    m_option1Label = new QLabel("1 开个门", this);
    m_option1Label->setAlignment(Qt::AlignCenter);
    m_option1Label->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "    background-color: rgba(255, 255, 255, 200);"
        "    padding: 15px 30px;"
        "    border-radius: 25px;"
        "    border: 2px solid #3498db;"
        "}"
    );
    m_option1Label->setFixedHeight(60);
    
    // 选项2标签 - 圆角矩形包围
    m_option2Label = new QLabel("2 介绍一下实验室", this);
    m_option2Label->setAlignment(Qt::AlignCenter);
    m_option2Label->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "    background-color: rgba(255, 255, 255, 200);"
        "    padding: 15px 30px;"
        "    border-radius: 25px;"
        "    border: 2px solid #e74c3c;"
        "}"
    );
    m_option2Label->setFixedHeight(60);
    
    // 底部语音识别结果标签
    m_recognitionResultLabel = new QLabel("等待唤醒词 你好鲲鹏", this);
    m_recognitionResultLabel->setAlignment(Qt::AlignCenter);
    m_recognitionResultLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 18px;"
        "    font-weight: bold;"
        "    color: #34495e;"
        "    background-color: rgba(255, 255, 255, 220);"
        "    padding: 15px;"
        "    border: 2px solid #95a5a6;"
        "    border-radius: 10px;"
        "}"
    );
    m_recognitionResultLabel->setFixedHeight(80);
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(m_gifLabel);
    leftLayout->addStretch();
    // 创建右侧布局
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(20);
    rightLayout->addWidget(m_instructionLabel);
    rightLayout->addWidget(m_option1Label);
    rightLayout->addWidget(m_option2Label);
    
    // 创建后台管理按钮
    m_backendButton = new QPushButton("后台管理", this);
    m_backendButton->setStyleSheet(
        "QPushButton {"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    color: white;"
        "    background-color: #34495e;"
        "    padding: 10px 20px;"
        "    border-radius: 15px;"
        "    border: 2px solid #2c3e50;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2c3e50;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1e2732;"
        "}"
    );
    m_backendButton->setFixedHeight(50);
    rightLayout->addWidget(m_backendButton);
    
    rightLayout->addStretch();
    
    // 将组件添加到网格布局
    // m_mainLayout->addWidget(m_gifLabel, 0, 0, 0, 0); // GIF标签占据左上角两行
    m_mainLayout->addLayout(leftLayout,0,0,2,1);
    m_mainLayout->addLayout(rightLayout, 0, 1, 2, 1);  // 右侧内容占据两行
    m_mainLayout->addWidget(m_recognitionResultLabel, 2, 0, 1, 2); // 底部标签跨两列
    
    // 设置行列比例
    m_mainLayout->setRowStretch(0, 2);
    m_mainLayout->setRowStretch(1, 2);
    m_mainLayout->setRowStretch(2, 1);
    m_mainLayout->setColumnStretch(0, 1);
    m_mainLayout->setColumnStretch(1, 1);
    
    // 创建语音识别对象
    m_voiceRecognition = new VoiceRecognition(this);
    
    // 创建声纹识别结果标签
    m_voiceprintResultLabel = new QLabel("声纹识别结果将显示在这里", this);
    m_voiceprintResultLabel->setAlignment(Qt::AlignCenter);
    m_voiceprintResultLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "    background-color: rgba(255, 255, 255, 200);"
        "    padding: 10px;"
        "    border: 2px solid #9b59b6;"
        "    border-radius: 8px;"
        "}"
    );
    m_voiceprintResultLabel->setFixedHeight(60);
    
    // 将声纹识别结果标签添加到布局（在语音识别结果标签下方）
    m_mainLayout->addWidget(m_voiceprintResultLabel, 3, 0, 1, 2);
    
    // 更新行列比例
    m_mainLayout->setRowStretch(3, 1);
    
    // 设置唤醒状态重置定时器（30秒后自动重置）
    m_wakeupResetTimer->setSingleShot(true);
    m_wakeupResetTimer->setInterval(30000); // 30秒
    connect(m_wakeupResetTimer, &QTimer::timeout, this, [this]() {
        // 只有在没有正在处理指令时才执行重置
        if (m_isWakeUpActivated && !m_isProcessingCommand) {
            m_isWakeUpActivated = false;
            m_isProcessingCommand = false; // 重置处理状态
            
            // 返回等待动画
            returnToWaitingAnimation();
            
            m_recognitionResultLabel->setText("等待唤醒词 你好鲲鹏");
            m_recognitionResultLabel->setStyleSheet(
                "QLabel {"
                "    font-size: 18px;"
                "    font-weight: bold;"
                "    color: #34495e;"
                "    background-color: rgba(255, 255, 255, 220);"
                "    padding: 15px;"
                "    border: 2px solid #95a5a6;"
                "    border-radius: 10px;"
                "}"
            );
            qDebug() << "唤醒状态已重置，返回等待状态";
        } else if (m_isProcessingCommand) {
            // 如果正在处理指令，延长超时时间
            qDebug() << "正在处理指令，延长超时时间";
            m_wakeupResetTimer->start(); // 重新启动定时器
        }
    });
    
    // 设置声纹识别定时器
    m_voiceprintTimer->setInterval(3000); // 每3秒进行一次声纹识别（降低频率以确保有足够音频数据）
    connect(m_voiceprintTimer, &QTimer::timeout, this, &Home::onVoiceprintTimer);
    m_voiceprintTimer->start(); // 启动声纹识别定时器
}

void Home::playAwakeAnimation()
{
    // 设置唤醒GIF文件路径
    QString awakeGifPath = "D:/qt/KunPeng/material/awake.gif";
    qDebug() << "唤醒GIF文件路径:" << awakeGifPath;
    
    QFileInfo awakeGifFile(awakeGifPath);
    if (awakeGifFile.exists()) {
        // 停止当前播放的动画
        if (m_gifMovie) {
            m_gifMovie->stop();
        }
        
        // 创建或重新设置唤醒动画
        if (!m_awakeMovie) {
            m_awakeMovie = new QMovie(awakeGifPath, QByteArray(), this);
        } else {
            m_awakeMovie->setFileName(awakeGifPath);
        }
        
        // 检查唤醒GIF是否有效
        if (m_awakeMovie->isValid()) {
            // 设置缓存模式
            m_awakeMovie->setCacheMode(QMovie::CacheAll);
            
            // 设置缩放模式，确保GIF填充整个标签
            m_awakeMovie->setScaledSize(m_gifLabel->size());
            
            // 先断开之前的连接，避免重复连接
            disconnect(m_awakeMovie, &QMovie::finished, this, nullptr);
            
            // 连接动画完成信号，实现只播放一次的效果
            connect(m_awakeMovie, &QMovie::finished, this, [this]() {
                qDebug() << "唤醒动画播放完成";
                // 停止动画，确保只播放一次
                if (m_awakeMovie) {
                    m_awakeMovie->stop();
                }
                // 只有在没有正在处理指令且未被唤醒时才返回等待动画
                if (!m_isProcessingCommand && !m_isWakeUpActivated) {
                    returnToWaitingAnimation();
                }
                // 如果正在处理指令，保持当前状态，等待处理完成
                qDebug() << "处理指令状态:" << m_isProcessingCommand << ", 唤醒状态:" << m_isWakeUpActivated;
            });
            
            m_gifLabel->setMovie(m_awakeMovie);
            m_awakeMovie->start();
            qDebug() << "唤醒动画开始播放";
        } else {
            qDebug() << "唤醒GIF文件无效";
        }
    } else {
        qDebug() << "唤醒GIF文件不存在:" << awakeGifPath;
    }
}

void Home::returnToWaitingAnimation()
{
    // 停止唤醒动画
    if (m_awakeMovie) {
        m_awakeMovie->stop();
        // 断开信号连接，避免重复触发
        disconnect(m_awakeMovie, &QMovie::finished, this, nullptr);
    }
    
    // 恢复等待动画
    if (m_gifMovie && m_gifMovie->isValid()) {
        m_gifMovie->setScaledSize(m_gifLabel->size());
        m_gifLabel->setMovie(m_gifMovie);
        m_gifMovie->start();
        qDebug() << "已切换回等待动画";
    } else {
        qDebug() << "等待动画无效，显示占位符";
        showVideoPlaceholder();
    }
}

void Home::connectSignals()
{
    // 连接语音识别信号
    connect(m_voiceRecognition, &VoiceRecognition::recognitionResult, 
            this, &Home::onRecognitionResult);
    connect(m_voiceRecognition, &VoiceRecognition::recognitionError, 
            this, &Home::onRecognitionError);
    connect(m_voiceRecognition, &VoiceRecognition::recognitionFinished, 
            this, &Home::onRecognitionFinished);
    
    // 连接音频数据接收信号，将音频数据存储到缓冲区
    connect(m_voiceRecognition, &VoiceRecognition::audioDataReceived,
            this, [this](const QByteArray &audioData) {
                if (!audioData.isEmpty()) {
                    // 限制单次添加的数据大小，避免一次性添加过大的数据块
                    const int maxChunkSize = 16000 * 2; // 1秒的音频数据
                    QByteArray dataToAdd = audioData;
                    
                    if (dataToAdd.size() > maxChunkSize) {
                        // 如果数据块太大，只取最后1秒的数据
                        dataToAdd = dataToAdd.right(maxChunkSize);
                        qDebug() << "音频数据块过大，截取最后" << maxChunkSize << "字节";
                    }
                    
                    m_audioBuffer.append(dataToAdd);
                    
                    // 保持最近5秒的音频数据
                    const int maxBufferSize = 16000 * 2 * 5; // 5秒的音频数据
                    if (m_audioBuffer.size() > maxBufferSize) {
                        int removeSize = m_audioBuffer.size() - maxBufferSize;
                        m_audioBuffer.remove(0, removeSize);
                        qDebug() << "音频缓冲区维持在" << m_audioBuffer.size() << "字节";
                    }
                    
                    // qDebug() << "接收到音频数据:" << audioData.size() << "字节，添加" << dataToAdd.size() << "字节，当前缓冲区大小:" << m_audioBuffer.size() << "字节";
                }
            });
    
    // 连接后台管理按钮信号
    connect(m_backendButton, &QPushButton::clicked, 
            this, &Home::onOpenBackendClicked);
}

void Home::startContinuousRecognition()
{
    if (!m_isRecognitionActive && m_voiceRecognition) {
        m_voiceRecognition->startRealtimeRecognition();
        m_isRecognitionActive = true;
    }
}

void Home::checkForWakeupWord(const QString &text)
{
    // 去除标点符号并转换为小写
    QString cleanText = removePunctuation(text).toLower();
    
    qDebug() << "原始文本:" << text;
    qDebug() << "清理后文本:" << cleanText;
    
    // 检查多种唤醒词变体（不需要标点符号干扰）
    QStringList wakeupWords = {
        "你好鲲鹏", "你好昆鹏", "你好坤鹏", 
        "您好鲲鹏", "您好昆鹏", "您好坤鹏",
        "哈喽鲲鹏", "嗨鲲鹏"
    };
    
    bool isWakeupDetected = false;
    for (const QString &wakeup : wakeupWords) {
        if (cleanText.contains(wakeup)) {
            isWakeupDetected = true;
            break;
        }
    }
    
    if (isWakeupDetected) {
        m_isWakeUpActivated = true;
        m_isProcessingCommand = false; // 重置处理状态
        
        // 播放唤醒动画
        playAwakeAnimation();
        
        m_recognitionResultLabel->setText("唤醒成功 请说出您的指令");
        m_recognitionResultLabel->setStyleSheet(
            "QLabel {"
            "    font-size: 18px;"
            "    color: #fff;"
            "    background-color: #27ae60;"
            "    padding: 15px;"
            "    border: 2px solid #2ecc71;"
            "    border-radius: 10px;"
            "}"
        );
        qDebug() << "检测到唤醒词，原始文本:" << text;
        
        // 启动唤醒状态重置定时器
        m_wakeupResetTimer->start();
    } else if (m_isWakeUpActivated) {
        // 已经唤醒，显示识别到的内容（去除标点符号后显示）
        m_isProcessingCommand = true; // 标记正在处理指令
        
        QString cleanDisplayText = removePunctuation(text);
        QString displayText = QString("识别到 %1").arg(cleanDisplayText);
        m_recognitionResultLabel->setText(displayText);
        m_recognitionResultLabel->setStyleSheet(
            "QLabel {"
            "    font-size: 18px;"
            "    color: #2c3e50;"
            "    background-color: #e8f6f3;"
            "    padding: 15px;"
            "    border: 2px solid #1abc9c;"
            "    border-radius: 10px;"
            "}"
        );
        qDebug() << "唤醒后识别到:" << text;
        
        // 开始处理指令的延时逻辑
        QTimer::singleShot(5000, this, [this]() {
            m_isProcessingCommand = false;
            qDebug() << "指令处理完成，返回等待状态";
            
            // 重置界面显示
            m_recognitionResultLabel->setText("等待唤醒词 你好鲲鹏");
            m_recognitionResultLabel->setStyleSheet(
                "QLabel {"
                "    font-size: 18px;"
                "    font-weight: bold;"
                "    color: #34495e;"
                "    background-color: rgba(255, 255, 255, 220);"
                "    padding: 15px;"
                "    border: 2px solid #95a5a6;"
                "    border-radius: 10px;"
                "}"
            );
            
            // 重置唤醒状态
            m_isWakeUpActivated = false;
            
            // 立即返回等待动画
            returnToWaitingAnimation();
        });
        
        // 不重新启动定时器，让指令处理完成后统一重置
    } else {
        // 未唤醒状态，不显示具体内容，只显示等待状态
        // 但不更新界面显示，避免频繁闪烁
        qDebug() << "未唤醒状态，收到文本:" << text;
    }
}

void Home::onRecognitionResult(const QString &result)
{
    qDebug() << "收到语音识别结果:" << result;
    
    // 只有当结果不为空时才处理
    if (!result.trimmed().isEmpty()) {
        checkForWakeupWord(result);
    }
}

void Home::onRecognitionError(const QString &error)
{
    // 只在控制台输出错误信息，不在界面显示
    qDebug() << "语音识别错误(不显示在界面):" << error;
    
    m_isRecognitionActive = false;
    
    // 根据错误类型决定重试策略
    if (error.contains("网络") || error.contains("连接") || error.contains("超时")) {
        // 网络相关错误，延长重试时间
        QTimer::singleShot(5000, this, &Home::startContinuousRecognition);
    } else {
        // 其他错误，正常重试
        QTimer::singleShot(3000, this, &Home::startContinuousRecognition);
    }
}

void Home::onRecognitionFinished()
{
    m_isRecognitionActive = false;
    // 重新开始识别以保持连续监听
    QTimer::singleShot(1000, this, &Home::startContinuousRecognition);
}

void Home::showVideoPlaceholder()
{
    // 停止GIF动画
    if (m_gifMovie) {
        m_gifMovie->stop();
    }
    
    // 如果还没有创建占位符标签，就创建一个
    if (!m_videoPlaceholder) {
        m_videoPlaceholder = new QLabel(this);
        m_videoPlaceholder->setText("鲲鹏动画");
        m_videoPlaceholder->setAlignment(Qt::AlignCenter);
        m_videoPlaceholder->setFixedSize(400, 300);
        m_videoPlaceholder->setStyleSheet(
            "QLabel {"
            "    border: 2px solid #34495e;"
            "    border-radius: 10px;"
            "    background-color: #2c3e50;"
            "    color: #ecf0f1;"
            "    font-size: 24px;"
            "    font-weight: bold;"
            "}"
        );
        
        // 将占位符放在GIF标签的位置
        m_videoPlaceholder->move(m_gifLabel->pos());
        m_videoPlaceholder->raise(); // 置于最前面
        
        // 创建动画效果
        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect();
        m_videoPlaceholder->setGraphicsEffect(effect);
        
        m_opacityAnimation = new QPropertyAnimation(effect, "opacity");
        m_opacityAnimation->setDuration(2000); // 2秒
        m_opacityAnimation->setStartValue(0.3);
        m_opacityAnimation->setEndValue(1.0);
        m_opacityAnimation->setLoopCount(-1); // 无限循环
        m_opacityAnimation->start();
    }
    
    // 隐藏GIF标签，显示占位符
    m_gifLabel->hide();
    m_videoPlaceholder->show();
    
    qDebug() << "已切换到GIF占位符模式";
}

QString Home::removePunctuation(const QString &text)
{
    QString result = text;
    // 定义要去除的标点符号（包括中英文标点和特殊符号）
    QString punctuation = "！？。，、；：''""（）【】《》〈〉「」『』〖〗［］｛｝!?.,;:\"'()[]{}…—～·@#$%^&*+-=_|\\/<>~`";
    
    // 去除所有标点符号
    for (const QChar &punct : punctuation) {
        result = result.remove(punct);
    }
    
    // 去除多余的空格，包括全角空格
    result = result.replace("　", " "); // 替换全角空格为半角空格
    result = result.simplified();
    
    return result;
}

void Home::onVoiceprintTimer()
{
    qDebug() << "onVoiceprintTimer被调用，当前音频缓冲区大小:" << m_audioBuffer.size() << "字节";
    
    // 如果正在处理声纹识别请求，跳过本次
    if (m_isProcessingVoiceprint) {
        qDebug() << "正在处理声纹识别请求，跳过本次识别";
        return;
    }
    
    // 检查是否有足够的音频数据（降低要求）
    const int requiredSize = 16000 * 2 * 2; // 2秒的16kHz 16bit单声道数据
    
    if (m_audioBuffer.size() < requiredSize) {
        // 音频数据不足，显示等待状态
        if (m_audioBuffer.size() > 0) {
            QString statusText = QString("缓冲音频: %1/%2 秒")
                .arg(m_audioBuffer.size() / (16000.0 * 2), 0, 'f', 1)
                .arg(2);
            m_voiceprintResultLabel->setText(statusText);
            qDebug() << statusText;
        } else {
            m_voiceprintResultLabel->setText("等待音频数据...");
            qDebug() << "等待音频数据...";
        }
        return;
    }
    
    // 取最后2秒的音频数据进行识别
    QByteArray audioForTest = m_audioBuffer.right(requiredSize);
    
    // 检查音频质量
    if (!hasAudioContent(audioForTest)) {
        m_voiceprintResultLabel->setText("音频太安静，等待有效音频...");
        qDebug() << "音频质量检测未通过，跳过识别";
        return;
    }
    
    // 保存音频数据，用于潜在的访客声纹添加
    m_lastAudioForVisitor = audioForTest;
    
    // 设置处理状态，防止并发请求
    m_isProcessingVoiceprint = true;
    
    // 开始声纹识别
    qDebug() << "开始实时声纹识别，音频数据大小:" << audioForTest.size() << "字节";
    
    // 更新UI显示
    m_voiceprintResultLabel->setText("正在进行声纹识别...");
    m_voiceprintResultLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "    background-color: rgba(255, 255, 255, 200);"
        "    padding: 10px;"
        "    border: 2px solid #3498db;"
        "    border-radius: 8px;"
        "}"
    );
    
    // 直接使用API进行声纹识别
    if (m_voiceprintApi) {
        m_voiceprintApi->searchFeatureWithData(audioForTest, m_groupName, 1);
    } else {
        qDebug() << "声纹识别API未初始化";
        m_voiceprintResultLabel->setText("声纹识别API未初始化");
        m_isProcessingVoiceprint = false;
    }
}

// 音频质量检测函数（从Backend复制）
bool Home::hasAudioContent(const QByteArray &audioData)
{
    if (audioData.isEmpty() || audioData.size() < 1000) {
        return false;
    }
    
    // 将音频数据转换为16位整数进行分析
    const int16_t* samples = reinterpret_cast<const int16_t*>(audioData.constData());
    int sampleCount = audioData.size() / 2;
    
    // 计算平均能量和有效样本比例
    double totalEnergy = 0.0;
    int validSamples = 0;
    const int16_t threshold = 50; // 有效音频的最小阈值
    
    for (int i = 0; i < sampleCount; ++i) {
        int16_t sample = qAbs(samples[i]);
        totalEnergy += sample;
        if (sample > threshold) {
            validSamples++;
        }
    }
    
    double avgEnergy = totalEnergy / sampleCount;
    double validRatio = (double)validSamples / sampleCount * 100.0;
    
    qDebug() << QString::fromUtf8("音频质量检测 - 平均能量:") << avgEnergy 
             << QString::fromUtf8("有效样本比例:") << validRatio << "%";
    
    // 调整后的阈值：平均能量大于50，有效样本比例大于0.1%
    if (avgEnergy > 50 && validRatio > 0.1) {
        qDebug() << QString::fromUtf8("质量判定:") << QString::fromUtf8("通过");
        return true;
    } else {
        qDebug() << QString::fromUtf8("质量判定:") << QString::fromUtf8("不通过");
        return false;
    }
}

void Home::onOpenBackendClicked()
{
    qDebug() << "打开后台管理界面";
    
    // 如果后台管理窗口不存在，创建它
    if (!m_backendWindow) {
        m_backendWindow = new Backend();
    }
    
    // 显示后台管理窗口
    m_backendWindow->show();
    m_backendWindow->raise();
    m_backendWindow->activateWindow();
}

void Home::onVoiceprintApiFinished(const QJsonObject &result)
{
    // 重置处理状态
    m_isProcessingVoiceprint = false;
    
    qDebug() << "Home收到声纹识别API响应:" << QJsonDocument(result).toJson(QJsonDocument::Compact);
    
    if (!result.contains("payload")) {
        updateVoiceprintResult("声纹识别完成，但无识别结果", true);
        return;
    }
    
    const QJsonObject payload = result.value("payload").toObject();
    
    if (payload.contains("searchFeaRes") || payload.contains("searchScoreFeaRes")) {
        // 处理声纹识别结果
        QJsonObject searchRes;
        if (payload.contains("searchFeaRes")) {
            searchRes = payload.value("searchFeaRes").toObject();
        } else {
            searchRes = payload.value("searchScoreFeaRes").toObject();
        }
        
        const QString text = searchRes.value("text").toString();
        if (!text.isEmpty()) {
            // 解码Base64编码的识别结果
            QByteArray decodedData = QByteArray::fromBase64(text.toUtf8());
            QJsonDocument doc = QJsonDocument::fromJson(decodedData);
            
            if (doc.isNull()) {
                doc = QJsonDocument::fromJson(text.toUtf8());
            }
            
            if (!doc.isNull()) {
                processVoiceprintSearchResult(doc);
            } else {
                updateVoiceprintResult("声纹识别结果解析失败", true);
            }
        } else {
            updateVoiceprintResult("声纹识别结果为空", true);
        }
    } else {
        updateVoiceprintResult("未知的声纹识别响应格式", true);
    }
}

void Home::onVoiceprintApiError(const QString &error)
{
    // 重置处理状态
    m_isProcessingVoiceprint = false;
    
    qDebug() << "Home收到声纹识别API错误:" << error;
    
    // 根据错误类型显示不同的信息
    if (error.contains("网络") || error.contains("连接") || error.contains("超时")) {
        updateVoiceprintResult("网络连接错误，稍后重试", true);
    } else if (error.contains("音频")) {
        updateVoiceprintResult("音频质量不够，继续监听", true);
    } else {
        updateVoiceprintResult(QString("识别错误: %1").arg(error), true);
    }
}

void Home::processVoiceprintSearchResult(const QJsonDocument &doc)
{
    qDebug() << "处理声纹识别结果，数据:" << doc.toJson(QJsonDocument::Compact);
    
    QString resultText = "识别结果: ";
    bool needAddVisitor = false;
    double confidence = 0.0;
    QString featureId;
    
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        if (arr.isEmpty()) {
            resultText += "未识别到已知用户";
            needAddVisitor = true;
        } else {
            const QJsonObject item = arr[0].toObject();
            featureId = item.value("featureId").toString();
            confidence = item.value("score").toDouble();
            
            if (confidence < 0.4) {
                // 检查是否是访客
                if (featureId == "visitor") {
                    resultText += QString("访客 (相似度: %1)").arg(confidence, 0, 'f', 2);
                } else {
                    resultText += QString("置信度过低(%1)，识别为访客").arg(confidence, 0, 'f', 2);
                    needAddVisitor = true;
                }
            } else {
                resultText += QString("%1 (相似度: %2)").arg(featureId).arg(confidence, 0, 'f', 2);
            }
        }
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("scoreList") && obj.value("scoreList").isArray()) {
            // 处理 {"scoreList": []} 格式 - 这是iFlytek API的实际返回格式
            QJsonArray scoreList = obj.value("scoreList").toArray();
            if (scoreList.isEmpty()) {
                resultText += "未识别到已知用户";
                needAddVisitor = true;
            } else {
                const QJsonObject item = scoreList[0].toObject();
                featureId = item.value("featureId").toString();
                confidence = item.value("score").toDouble();
                
                if (confidence < 0.4) {
                    // 检查是否是访客
                    if (featureId == "visitor") {
                        resultText += QString("访客 (相似度: %1)").arg(confidence, 0, 'f', 2);
                    } else {
                        resultText += QString("置信度过低(%1)，识别为访客").arg(confidence, 0, 'f', 2);
                        needAddVisitor = true;
                    }
                } else {
                    resultText += QString("%1 (相似度: %2)").arg(featureId).arg(confidence, 0, 'f', 2);
                }
            }
        } else if (obj.contains("data") && obj.value("data").isArray()) {
            // 处理 {"data": []} 格式
            processVoiceprintSearchResult(QJsonDocument(obj.value("data").toArray()));
            return;
        } else {
            // 处理单个结果
            featureId = obj.value("featureId").toString();
            confidence = obj.value("score").toDouble();
            
            if (featureId.isEmpty()) {
                resultText += "未识别到已知用户";
                needAddVisitor = true;
            } else if (confidence < 0.4) {
                // 检查是否是访客
                if (featureId == "visitor") {
                    resultText += QString("访客 (相似度: %1)").arg(confidence, 0, 'f', 2);
                } else {
                    resultText += QString("置信度过低(%1)，识别为访客").arg(confidence, 0, 'f', 2);
                    needAddVisitor = true;
                }
            } else {
                resultText += QString("%1 (相似度: %2)").arg(featureId).arg(confidence, 0, 'f', 2);
            }
        }
    } else {
        resultText += "格式错误";
    }
    
    qDebug() << "最终识别结果:" << resultText;
    qDebug() << "是否需要添加访客声纹:" << needAddVisitor;
    qDebug() << "是否已经添加过访客:" << m_hasVisitorAdded;
    
    // 如果需要添加访客声纹，且之前没有添加过，则自动添加
    if (needAddVisitor && !m_hasVisitorAdded && !m_lastAudioForVisitor.isEmpty()) {
        resultText += " - 正在添加为访客...";
        addVisitorVoiceprint(m_lastAudioForVisitor);
    } else if (needAddVisitor && m_hasVisitorAdded) {
        resultText += " - 识别为访客";
        qDebug() << "检测到陌生声纹，但访客已存在，不重复添加";
    }
    
    updateVoiceprintResult(resultText, false);
}

void Home::updateVoiceprintResult(const QString &result, bool isError)
{
    if (!m_voiceprintResultLabel) {
        return;
    }
    
    m_voiceprintResultLabel->setText(result);
    
    if (isError) {
        m_voiceprintResultLabel->setStyleSheet(
            "QLabel {"
            "    font-size: 16px;"
            "    font-weight: bold;"
            "    color: #fff;"
            "    background-color: #e74c3c;"
            "    padding: 10px;"
            "    border: 2px solid #c0392b;"
            "    border-radius: 8px;"
            "}"
        );
    } else {
        m_voiceprintResultLabel->setStyleSheet(
            "QLabel {"
            "    font-size: 16px;"
            "    font-weight: bold;"
            "    color: #fff;"
            "    background-color: #27ae60;"
            "    padding: 10px;"
            "    border: 2px solid #2ecc71;"
            "    border-radius: 8px;"
            "}"
        );
    }
}

void Home::addVisitorVoiceprint(const QByteArray &audioData)
{
    if (audioData.isEmpty()) {
        qDebug() << "音频数据为空，无法添加访客声纹";
        return;
    }
    
    if (m_hasVisitorAdded) {
        qDebug() << "访客声纹已存在，不重复添加";
        return;
    }
    
    // 使用固定的访客ID
    QString visitorId = "visitor";
    
    qDebug() << "准备添加访客声纹，ID:" << visitorId << "音频大小:" << audioData.size() << "字节";
    
    // 创建临时文件保存音频
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString tempAudioPath = QApplication::applicationDirPath() + QString("/temp_visitor_%1.wav").arg(timestamp);
    
    if (!saveAudioAsWav(audioData, tempAudioPath)) {
        qDebug() << "保存访客临时音频文件失败";
        updateVoiceprintResult("访客声纹添加失败：音频保存错误", true);
        return;
    }
    
    qDebug() << "访客音频文件已保存:" << tempAudioPath;
    
    // 使用API添加声纹
    if (m_voiceprintApi) {
        // 使用单次连接来处理这次添加声纹的结果
        connect(m_voiceprintApi, &VoiceprintRequest::requestFinished,
                this, [this, visitorId, tempAudioPath](const QJsonObject &result) {
                    qDebug() << "访客声纹添加结果:" << QJsonDocument(result).toJson(QJsonDocument::Compact);
                    
                    // 检查是否成功
                    if (result.contains("header")) {
                        const QJsonObject header = result.value("header").toObject();
                        const int code = header.value("code").toInt();
                        if (code == 0) {
                            m_hasVisitorAdded = true; // 设置访客已添加标志
                            updateVoiceprintResult(QString("已添加访客声纹: %1").arg(visitorId), false);
                            qDebug() << "访客声纹添加成功:" << visitorId;
                        } else {
                            const QString message = header.value("message").toString();
                            updateVoiceprintResult(QString("访客声纹添加失败: %1").arg(message), true);
                            qDebug() << "访客声纹添加失败，错误码:" << code << "错误信息:" << message;
                        }
                    } else {
                        m_hasVisitorAdded = true; // 即使格式未知，也认为添加成功
                        updateVoiceprintResult("访客声纹添加完成", false);
                        qDebug() << "访客声纹添加完成（未知结果格式）";
                    }
                    
                    // 清理临时文件
                    QFile::remove(tempAudioPath);
                    qDebug() << "已清理临时文件:" << tempAudioPath;
                }, Qt::SingleShotConnection);
        
        // 使用单次连接来处理错误
        connect(m_voiceprintApi, &VoiceprintRequest::requestError,
                this, [this, tempAudioPath](const QString &error) {
                    updateVoiceprintResult(QString("访客声纹添加失败: %1").arg(error), true);
                    qDebug() << "访客声纹添加失败:" << error;
                    
                    // 清理临时文件
                    QFile::remove(tempAudioPath);
                    qDebug() << "已清理临时文件:" << tempAudioPath;
                }, Qt::SingleShotConnection);
        
        // 调用API添加声纹
        m_voiceprintApi->createFeature(tempAudioPath, m_groupName, visitorId, QString("访客声纹-%1").arg(visitorId));
    } else {
        qDebug() << "声纹识别API未初始化，无法添加访客声纹";
        updateVoiceprintResult("访客声纹添加失败：API未初始化", true);
        QFile::remove(tempAudioPath);
    }
}

bool Home::saveAudioAsWav(const QByteArray &audioData, const QString &filePath)
{
    qDebug() << "保存音频文件:" << filePath << "大小:" << audioData.size() << "字节";
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法创建文件:" << filePath;
        return false;
    }
    
    // WAV文件头
    struct WavHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        quint32 fileSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        quint32 fmtSize = 16;
        quint16 audioFormat = 1; // PCM
        quint16 numChannels = 1; // 单声道
        quint32 sampleRate = 16000; // 16kHz
        quint32 byteRate = 32000; // sampleRate * numChannels * bitsPerSample / 8
        quint16 blockAlign = 2; // numChannels * bitsPerSample / 8
        quint16 bitsPerSample = 16; // 16位
        char data[4] = {'d', 'a', 't', 'a'};
        quint32 dataSize;
    } header;
    
    header.dataSize = audioData.size();
    header.fileSize = sizeof(header) - 8 + audioData.size();
    
    // 写入头部
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // 写入音频数据
    qint64 written = file.write(audioData);
    file.close();
    
    if (written != audioData.size()) {
        qDebug() << "音频数据写入不完整，期望:" << audioData.size() << "实际:" << written;
        return false;
    }
    
    qDebug() << "音频文件保存成功:" << filePath;
    return true;
}