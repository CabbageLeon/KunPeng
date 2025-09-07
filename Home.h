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
#include <QPushButton>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "VoiceRecognition.h"
#include "Backend.h"
#include "VoiceprintRequest.h"
#include "VoiceGenerator.h"
#include "DeviceControl.h"

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
    void onVoiceprintTimer(); // 声纹识别定时器槽函数
    void onOpenBackendClicked(); // 打开后台管理界面
    void onVoiceprintApiFinished(const QJsonObject &result); // 声纹识别API完成槽函数
    void onVoiceprintApiError(const QString &error); // 声纹识别API错误槽函数
    void onTtsSynthesisFinished(const QString &filePath); // TTS合成完成槽函数
    void onPhotoSaved(const QString &filePath); // 照片保存完成槽函数
    void onPhotoCaptureFailed(const QString &error); // 拍照失败槽函数

private:
    void setupUI();
    void connectSignals();
    void startContinuousRecognition();
    void checkForWakeupWord(const QString &text);
    void showVideoPlaceholder(); // 显示视频占位符
    void playAwakeAnimation(); // 播放唤醒动画
    void returnToWaitingAnimation(); // 返回等待动画
    void performVoiceprintRecognition(const QByteArray &audioData); // 使用Backend执行声纹识别
    bool hasAudioContent(const QByteArray &audioData); // 检测音频内容质量
    QString removePunctuation(const QString &text); // 去除标点符号
    void processVoiceprintSearchResult(const QJsonDocument &doc); // 处理声纹识别搜索结果
    void updateVoiceprintResult(const QString &result, bool isError); // 更新声纹识别结果显示
    void addVisitorVoiceprint(const QByteArray &audioData); // 添加访客声纹
    bool saveAudioAsWav(const QByteArray &audioData, const QString &filePath); // 保存音频为WAV文件
    QString readVisitorTitle(); // 读取访客称谓
    void checkForDoorOpenCommand(const QString &text); // 检查开门指令

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
    
    // 声纹识别结果标签
    QLabel *m_voiceprintResultLabel;
    
    // 后台管理按钮
    QPushButton *m_backendButton;
    
    // 后台管理界面
    Backend *m_backendWindow;
    
    // 状态标志
    bool m_isWakeUpActivated;
    bool m_isRecognitionActive;
    bool m_isProcessingCommand; // 是否正在处理用户指令
    
    // 声纹识别相关
    QTimer *m_voiceprintTimer; // 声纹识别定时器
    QByteArray m_audioBuffer; // 音频缓冲区
    VoiceprintRequest *m_voiceprintApi; // 声纹识别API实例
    QString m_groupName; // 声纹组名
    bool m_isProcessingVoiceprint; // 是否正在处理声纹识别请求
    QByteArray m_lastAudioForVisitor; // 保存最后一次用于识别的音频数据（用于添加访客声纹）
    bool m_hasVisitorAdded; // 是否已经添加过访客声纹
    
    // 两帧数据收集机制
    bool m_isCollectingSecondFrame; // 是否正在收集第二帧数据
    QByteArray m_firstFrameAudio; // 第一帧音频数据
    QByteArray m_secondFrameAudio; // 第二帧音频数据
    bool m_hasUsedDoubleFrame; // 是否已经使用过双帧数据
    
    // TTS语音合成
    VoiceGenerator *m_voiceGenerator; // TTS语音合成器
    bool m_isVisitorDetected; // 是否检测到访客声纹
    
    // 音频播放
    QMediaPlayer *m_mediaPlayer; // 音频播放器
    QAudioOutput *m_audioOutput; // 音频输出
    
    // 设备控制
    DeviceControl *m_deviceControl; // 设备控制器
    
    // 唤醒状态管理
    QTimer *m_wakeupResetTimer; // 唤醒状态重置定时器
};

#endif // HOME_H