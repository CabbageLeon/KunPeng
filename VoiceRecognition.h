#ifndef VOICERECOGNITION_H
#define VOICERECOGNITION_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QCryptographicHash>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include<QAudioSource>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>
#include <QMessageBox>
#include <QDebug>
#include <QAbstractSocket>

// 前向声明
class QMediaPlayer;
class QAudioOutput;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QWidget;

// 简单的音频测试类
class AudioTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioTestWidget(QWidget *parent = nullptr);
    ~AudioTestWidget();

    // 设置测试用的音频内存数据
    void setTestAudioData(const char* dataPtr, size_t dataSize);

private slots:
    void onRecordClicked();
    void onPlayClicked();
    void onStopClicked();
    void onAudioDataRead();
    void updateRecordingStatus();

private:
    void setupUI();
    void setupAudio();
    bool saveAudioToTempFile(const QByteArray& audioData, QString& filePath);

private:
    // UI组件
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_recordButton;
    QPushButton* m_playButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;
    QLabel* m_infoLabel;

    // 音频相关
    const char* m_audioDataPtr;
    size_t m_audioDataSize;
    size_t m_audioDataOffset;
    QTimer* m_readTimer;
    QByteArray m_recordedData;
    
    // 播放相关
    QMediaPlayer* m_mediaPlayer;
    QAudioOutput* m_audioOutput;
    QString m_tempAudioFile;
    
    // 状态
    bool m_isRecording;
    bool m_isPlaying;
};

class VoiceRecognition : public QObject
{
    Q_OBJECT

public:
    explicit VoiceRecognition(QObject *parent = nullptr);
    ~VoiceRecognition();

    void startRealtimeRecognition();
    void stopRecognition();

signals:
    void recognitionResult(const QString &result);
    void recognitionError(const QString &error);
    void recognitionFinished();
    void audioDataReceived(const QByteArray &audioData); // 新增：用于声纹识别的音频数据

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketMessage(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onAudioDataReady();
    void processAudioBuffer();

private:
    // 科大讯飞API配置
    QString m_appId;
    QString m_apiKey;
    QString m_apiSecret;
    
    // WebSocket相关
    QWebSocket *m_webSocket;
    QString createWebSocketUrl();
    QString createAuthorization();
    
    // 音频状态常量 (对应Python代码中的STATUS_*)
    enum AudioStatus {
        STATUS_FIRST_FRAME = 0,
        STATUS_CONTINUE_FRAME = 1,
        STATUS_LAST_FRAME = 2
    };
    
    // 音频处理
    QAudioSource *m_audioSource;
    QAudioDevice m_audioDevice;
    QAudioFormat m_audioFormat;
    QIODevice *m_audioIODevice;
    QByteArray m_audioBuffer;
    QByteArray m_completeAudioData; // 新增：累积完整的音频数据用于声纹识别
    AudioStatus m_audioStatus;
    
    // 定时器
    QTimer *m_audioTimer;
    
    void initAudioInput();
    void setupAudioFormat();
    void sendAudioFrame(const QByteArray &audioData);
    void sendFirstFrame(const QByteArray &audioData);
    void sendContinueFrame(const QByteArray &audioData);
    void sendLastFrame(const QByteArray &audioData);
};

#endif // VOICERECOGNITION_H