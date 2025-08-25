#ifndef VOICERECOGNITION_H
#define VOICERECOGNITION_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
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
#include <QAudioSource>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>

class VoiceRecognition : public QObject
{
    Q_OBJECT

public:
    explicit VoiceRecognition(QObject *parent = nullptr);
    ~VoiceRecognition();

    // 开始实时语音识别（从麦克风）
    void startRealtimeRecognition();
    
    // 停止语音识别
    void stopRecognition();

private slots:
    void onAudioDataReady();
    void onRecognitionFinished(QNetworkReply *reply);
    void processAudioBuffer();

signals:
    void recognitionResult(const QString &result);
    void recognitionError(const QString &error);
    void recognitionFinished();

private:
    // 生成认证参数
    QString createAuthParams();
    
    // 生成签名
    QString generateSignature(const QString &date);

    // 发送音频数据进行识别
    void sendAudioForRecognition(const QByteArray &audioData);

    // Base64编码
    QString base64Encode(const QByteArray &data);

    // HMAC-SHA256加密
    QByteArray hmacSha256(const QByteArray &key, const QByteArray &data);

    // 初始化音频输入
    void initAudioInput();

private:
    QNetworkAccessManager *m_networkManager;
    QTimer *m_audioTimer;

    // 实时音频输入
    QAudioSource *m_audioSource;
    QIODevice *m_audioDevice;
    QByteArray m_audioBuffer;
    QAudioFormat m_audioFormat;

    // API参数 (预设)
    QString m_appId;
    QString m_apiKey;
    QString m_apiSecret;

    // 音频参数
    int m_bufferSize;       // 音频缓冲区大小
    bool m_isRecording;     // 是否正在录音
};

#endif // VOICERECOGNITION_H