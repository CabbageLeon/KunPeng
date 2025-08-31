#ifndef VOICEGENERATOR_H
#define VOICEGENERATOR_H

#include <QObject>
#include <QWebSocket>
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QCryptographicHash>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QAbstractSocket>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QMessageAuthenticationCode>

class VoiceGenerator : public QObject
{
    Q_OBJECT

public:
    explicit VoiceGenerator(QObject *parent = nullptr);
    ~VoiceGenerator();

    // 设置API认证信息
    void setCredentials(const QString &appId, const QString &apiKey, const QString &apiSecret);
    
    // 设置音色（发音人）
    void setVoice(const QString &voice);
    
    // 语音合成接口
    void synthesizeText(const QString &text, const QString &outputPath = QString());
    
    // 停止合成
    void stopSynthesis();
    
    // 获取当前状态
    bool isSynthesizing() const { return m_isSynthesizing; }

signals:
    void synthesisStarted();
    void synthesisProgress(int percentage);
    void synthesisFinished(const QString &audioFilePath);
    void synthesisError(const QString &error);
    void audioDataReceived(const QByteArray &audioData); // 实时音频数据

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketMessage(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);

private:
    // 科大讯飞TTS API配置
    QString m_appId;
    QString m_apiKey;
    QString m_apiSecret;
    
    // WebSocket相关
    QWebSocket *m_webSocket;
    QString createWebSocketUrl();
    QString createAuthorization();
    
    // 合成参数
    QString m_voice;        // 发音人
    QString m_audioFormat;  // 音频格式
    QString m_sampleRate;   // 采样率
    QString m_encoding;     // 编码格式
    
    // 状态管理
    bool m_isSynthesizing;
    QString m_currentText;
    QString m_outputPath;
    QByteArray m_audioBuffer;
    
    // 音频文件处理
    QString generateOutputPath(const QString &text);
    bool saveAudioAsWav(const QByteArray &audioData, const QString &filePath);
    void clearAudioBuffer();
    
    // 请求构建
    QJsonObject buildSynthesisRequest(const QString &text);
    QString encodeTextToBase64(const QString &text);
    
    // 工具方法
    QString formatHttpDate(const QDateTime &dateTime);
    void parseUrl(const QString &url, QString &host, QString &path);
};

#endif // VOICEGENERATOR_H
