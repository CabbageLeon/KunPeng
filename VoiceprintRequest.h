#ifndef VOICEPRINTREQUEST_H
#define VOICEPRINTREQUEST_H

#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QUrlQuery>
#include <QObject>
#include <QRegularExpression>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>

class VoiceprintRequest : public QObject
{
    Q_OBJECT

public:
    explicit VoiceprintRequest(QObject *parent = nullptr);
    ~VoiceprintRequest();

    // 设置API认证信息
    void setCredentials(const QString &appId, const QString &apiKey, const QString &apiSecret);
    
    // API接口方法
    void createGroup(const QString &groupId = "iFLYTEK_examples_groupId", 
                     const QString &groupName = "iFLYTEK_examples_groupName",
                     const QString &groupInfo = "iFLYTEK_examples_groupInfo");
    
    void createFeature(const QString &audioFilePath,
                       const QString &groupId = "iFLYTEK_examples_groupId",
                       const QString &featureId = "iFLYTEK_examples_featureId",
                       const QString &featureInfo = "iFLYTEK_examples_featureInfo");
    
    void queryFeatureList(const QString &groupId = "iFLYTEK_examples_groupId");
    
    void searchFeature(const QString &audioFilePath,
                       const QString &groupId = "iFLYTEK_examples_groupId",
                       int topK = 1);
    
    // 直接使用音频数据进行声纹识别（实时识别）
    void searchFeatureWithData(const QByteArray &audioData,
                               const QString &groupId = "volunteer",
                               int topK = 1);
    
    void searchScoreFeature(const QString &audioFilePath,
                            const QString &groupId = "iFLYTEK_examples_groupId",
                            const QString &dstFeatureId = "iFLYTEK_examples_featureId");
    
    void updateFeature(const QString &audioFilePath,
                       const QString &groupId = "iFLYTEK_examples_groupId",
                       const QString &featureId = "iFLYTEK_examples_featureId",
                       const QString &featureInfo = "iFLYTEK_examples_featureInfo_update");
    
    void deleteFeature(const QString &groupId = "iFLYTEK_examples_groupId",
                       const QString &featureId = "iFLYTEK_examples_featureId");
    
    void deleteGroup(const QString &groupId = "iFLYTEK_examples_groupId");
    
    // 测试认证信息
    void testAuthentication();

signals:
    void requestFinished(const QJsonObject &result);
    void requestError(const QString &error);

private slots:
    void onNetworkReplyFinished();

private:
    // 工具方法
    QString assembleAuthUrl(const QString &requestUrl, const QString &method = "POST");
    QString generateSignature(const QString &host, const QString &date, 
                              const QString &method, const QString &path);
    QJsonObject generateRequestBody(const QString &apiName, const QString &audioFilePath = QString());
    QString encodeAudioFile(const QString &filePath);
    QString encodeAudioData(const QByteArray &audioData); // 直接编码音频数据
    QByteArray convertPCMToMP3(const QByteArray &pcmData); // PCM转MP3
    bool saveRawAudioAsWav(const QByteArray &rawData, const QString &filePath); // 保存原始音频为WAV
    void parseUrl(const QString &url, QString &host, QString &path);
    QString formatHttpDate(const QDateTime &dateTime);
    void sendRequest(const QJsonObject &body);

private:
    QString m_appId;
    QString m_apiKey;
    QString m_apiSecret;
    QByteArray m_apiSecretBytes;  // 存储解码后的API Secret字节
    QString m_baseUrl;
    
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
};

#endif // VOICEPRINTREQUEST_H
