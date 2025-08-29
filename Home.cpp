#include "Home.h"
#include <QApplication>
#include <QFileInfo>
#include <QDebug>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QMovie>

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
    , m_voiceprintRequest(nullptr)
    , m_voiceprintResultLabel(nullptr)
    , m_isWakeUpActivated(false)
    , m_isRecognitionActive(false)
    , m_isProcessingCommand(false)
    , m_wakeupResetTimer(new QTimer(this))
{
    setupUI();
    connectSignals();
    startContinuousRecognition();
}

Home::~Home()
{
    // 停止语音识别
    if (m_voiceRecognition) {
        m_voiceRecognition->stopRecognition();
        disconnect(m_voiceRecognition, nullptr, this, nullptr); // 断开所有来自语音识别的信号
    }
    
    // 断开声纹识别信号
    if (m_voiceprintRequest) {
        disconnect(m_voiceprintRequest, nullptr, this, nullptr);
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
    
    // 创建声纹识别对象
    m_voiceprintRequest = new VoiceprintRequest(this);
    // 设置API认证信息（请填写您的实际信息）
    m_voiceprintRequest->setCredentials("581ffbe4", 
                                       "c43e133e41862c3aa2495bae6c2268ef", 
                                       "OTgxYWRlNDdiYzFmZTBhNDRhM2NlYTE1");
    
    // 确保"volunteer"特征库存在
    qDebug() << "初始化时创建volunteer特征库...";
    m_voiceprintRequest->createGroup("volunteer", "志愿者特征库", "用于志愿者声纹识别的特征库");
    
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
    connect(m_voiceRecognition, &VoiceRecognition::audioDataReceived,
            this, &Home::saveAudioForVoiceprint);
    
    // 连接声纹识别信号
    connect(m_voiceprintRequest, &VoiceprintRequest::requestFinished,
            this, &Home::onVoiceprintResult);
    connect(m_voiceprintRequest, &VoiceprintRequest::requestError,
            this, &Home::onVoiceprintError);
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

void Home::onVoiceprintResult(const QJsonObject &result)
{
    qDebug() << "声纹识别结果:" << result;
    
    // 解析声纹识别结果
    if (result.contains("header")) {
        QJsonObject header = result["header"].toObject();
        int code = header["code"].toInt();
        
        if (code == 0) {
            // 成功
            if (result.contains("payload")) {
                QJsonObject payload = result["payload"].toObject();
                if (payload.contains("searchFeaRes")) {
                    QJsonObject searchFeaRes = payload["searchFeaRes"].toObject();
                    if (searchFeaRes.contains("text")) {
                        QString text = searchFeaRes["text"].toString();
                        QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
                        QJsonObject data = doc.object();
                        
                        if (data.contains("data") && data["data"].isArray()) {
                            QJsonArray dataArray = data["data"].toArray();
                            if (!dataArray.isEmpty()) {
                                QJsonObject firstResult = dataArray[0].toObject();
                                QString featureId = firstResult["featureId"].toString();
                                double score = firstResult["score"].toDouble();
                                
                                QString displayText = QString("声纹识别: %1 (置信度: %2%)")
                                                    .arg(featureId)
                                                    .arg(QString::number(score * 100, 'f', 1));
                                
                                m_voiceprintResultLabel->setText(displayText);
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
                                
                                qDebug() << QString("声纹识别成功: %1, 置信度: %2").arg(featureId).arg(score);
                                return;
                            }
                        }
                    }
                }
            }
            
            // 没有匹配结果
            m_voiceprintResultLabel->setText("声纹识别: 未找到匹配用户");
            m_voiceprintResultLabel->setStyleSheet(
                "QLabel {"
                "    font-size: 16px;"
                "    font-weight: bold;"
                "    color: #fff;"
                "    background-color: #f39c12;"
                "    padding: 10px;"
                "    border: 2px solid #e67e22;"
                "    border-radius: 8px;"
                "}"
            );
        } else {
            // 错误
            QString message = header["message"].toString();
            m_voiceprintResultLabel->setText(QString("声纹识别错误: %1").arg(message));
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
        }
    }
}

void Home::onVoiceprintError(const QString &error)
{
    qDebug() << "声纹识别网络错误:" << error;
    m_voiceprintResultLabel->setText(QString("声纹识别网络错误: %1").arg(error));
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
}

void Home::saveAudioForVoiceprint(const QByteArray &audioData)
{
    // 检查对象是否仍然有效
    if (!this || !m_voiceprintRequest) {
        qDebug() << "Home对象或声纹识别对象已无效，跳过声纹识别";
        return;
    }
    
    if (audioData.isEmpty()) {
        qDebug() << "音频数据为空，跳过声纹识别";
        return;
    }
    
    qDebug() << "开始实时声纹识别，音频数据大小:" << audioData.size() << "字节";
    
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
    
    // 直接使用音频数据进行实时声纹识别，不保存文件
    m_voiceprintRequest->searchFeatureWithData(audioData, "volunteer", 1);
}