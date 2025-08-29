#ifndef HOME_H
#define HOME_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QGridLayout>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include "VoiceRecognition.h"
#include "VoiceprintRequest.h"

class Home : public QWidget
{
    Q_OBJECT

public:
    explicit Home(QWidget *parent = nullptr);
    ~Home();

private slots:
    void onRecognitionResult(const QString &result);
    void onRecognitionError(const QString &error);
    void onRecognitionFinished();
    void onVoiceprintResult(const QJsonObject &result);
    void onVoiceprintError(const QString &error);

private:
    void setupUI();
    void connectSignals();
    void startContinuousRecognition();
    void checkForWakeupWord(const QString &text);
    void showVideoPlaceholder(); // 显示视频占位符
    void playAwakeAnimation(); // 播放唤醒动画
    void returnToWaitingAnimation(); // 返回等待动画
    void performVoiceprintRecognition(const QString &audioFilePath); // 执行声纹识别
    void saveAudioForVoiceprint(const QByteArray &audioData); // 保存音频用于声纹识别
    QString removePunctuation(const QString &text); // 去除标点符号

private:
    // UI组件
    QGridLayout *m_mainLayout;
    
    // GIF动画组件
    QLabel *m_gifLabel;
    QMovie *m_gifMovie;
    QMovie *m_awakeMovie; // 唤醒动画
    QLabel *m_videoPlaceholder; // GIF播放失败时的占位符
    QTimer *m_animationTimer; // 动画定时器
    QPropertyAnimation *m_opacityAnimation; // 不透明度动画
    
    // 文本标签
    QLabel *m_instructionLabel;
    QLabel *m_option1Label;
    QLabel *m_option2Label;
    QLabel *m_recognitionResultLabel;
    
    // 语音识别对象
    VoiceRecognition *m_voiceRecognition;
    
    // 声纹识别对象
    VoiceprintRequest *m_voiceprintRequest;
    
    // 声纹识别结果标签
    QLabel *m_voiceprintResultLabel;
    
    // 状态标志
    bool m_isWakeUpActivated;
    bool m_isRecognitionActive;
    bool m_isProcessingCommand; // 是否正在处理用户指令
    
    // 唤醒状态管理
    QTimer *m_wakeupResetTimer; // 唤醒状态重置定时器
};

#endif // HOME_H