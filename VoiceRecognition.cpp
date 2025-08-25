#include "VoiceRecognition.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QNetworkRequest>

VoiceRecognition::VoiceRecognition(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_audioTimer(new QTimer(this))
    , m_audioFile(nullptr)
    , m_audioSource(nullptr)
    , m_audioDevice(nullptr)
    , m_bufferSize(32000)  // 2秒的音频数据 (16000 * 2)
    , m_isRealtimeMode(false)
    , m_isRecording(false)
    , m_frameIndex(0)
{
    // 预设API参数 - 请替换为您的实际参数
    m_appId = "581ffbe4";
    m_apiKey = "c43e133e41862c3aa2495bae6c2268ef";
    m_apiSecret = "OTgxYWRlNDdiYzFmZTBhNDRhM2NlYTE1";

    // 连接网络管理器信号
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &VoiceRecognition::onRecognitionFinished);

    // 连接定时器信号（用于定期处理音频缓冲区）
    connect(m_audioTimer, &QTimer::timeout, this, &VoiceRecognition::processAudioBuffer);

    // 初始化音频输入
    initAudioInput();

    qDebug() << "科大讯飞语音识别初始化完成 - 使用HTTP API";
}

VoiceRecognition::~VoiceRecognition()
{
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
    }

    if (m_audioFile) {
        m_audioFile->close();
        delete m_audioFile;
    }
}

void VoiceRecognition::startRealtimeRecognition()
{
    if (m_appId.isEmpty() || m_apiKey.isEmpty() || m_apiSecret.isEmpty()) {
        emit recognitionError("API参数未设置，请在代码中配置您的科大讯飞API密钥");
        return;
    }

    if (!m_audioSource) {
        emit recognitionError("音频输入设备未初始化");
        return;
    }

    if (m_isRecording) {
        emit recognitionError("已经在录音中");
        return;
    }

    m_isRealtimeMode = true;
    m_isRecording = true;
    m_audioBuffer.clear();
    m_frameIndex = 0;

    // 开始录音
    m_audioDevice = m_audioSource->start();
    if (m_audioDevice) {
        connect(m_audioDevice, &QIODevice::readyRead,
                this, &VoiceRecognition::onAudioDataReady, Qt::UniqueConnection);

        // 启动定时器，每2秒处理一次音频缓冲区
        m_audioTimer->start(2000);

        qDebug() << "开始实时语音识别";
    } else {
        emit recognitionError("无法启动音频录制");
        m_isRecording = false;
    }
}

void VoiceRecognition::startFileRecognition(const QString &audioFilePath)
{
    if (m_appId.isEmpty() || m_apiKey.isEmpty() || m_apiSecret.isEmpty()) {
        emit recognitionError("API参数未设置，请在代码中配置您的科大讯飞API密钥");
        return;
    }

    m_isRealtimeMode = false;
    m_frameIndex = 0;

    // 检查音频文件
    if (m_audioFile) {
        m_audioFile->close();
        delete m_audioFile;
    }

    m_audioFile = new QFile(audioFilePath, this);
    if (!m_audioFile->open(QIODevice::ReadOnly)) {
        emit recognitionError("无法打开音频文件: " + audioFilePath);
        return;
    }

    // 读取整个文件进行识别
    QByteArray audioData = m_audioFile->readAll();
    if (audioData.isEmpty()) {
        emit recognitionError("音频文件为空");
        return;
    }

    // 发送整个文件
    sendAudioForRecognition(audioData, true);
}

void VoiceRecognition::stopRecognition()
{
    m_audioTimer->stop();
    m_isRecording = false;

    if (m_audioSource && m_isRealtimeMode) {
        m_audioSource->stop();

        // 处理剩余的音频数据
        if (!m_audioBuffer.isEmpty()) {
            sendAudioForRecognition(m_audioBuffer, true);
            m_audioBuffer.clear();
        }
    }

    if (m_audioFile) {
        m_audioFile->close();
    }

    qDebug() << "语音识别已停止";
}

void VoiceRecognition::onAudioDataReady()
{
    if (!m_audioDevice || !m_isRealtimeMode || !m_isRecording) {
        return;
    }

    QByteArray audioData = m_audioDevice->readAll();
    if (audioData.isEmpty()) {
        return;
    }

    m_audioBuffer.append(audioData);

    // 当缓冲区达到设定大小时立即处理
    if (m_audioBuffer.size() >= m_bufferSize) {
        processAudioBuffer();
    }
}

void VoiceRecognition::processAudioBuffer()
{
    if (m_audioBuffer.isEmpty()) {
        return;
    }

    // 发送当前缓冲区的音频数据
    QByteArray audioToSend = m_audioBuffer;
    m_audioBuffer.clear();

    sendAudioForRecognition(audioToSend, false);
}

void VoiceRecognition::sendAudioForRecognition(const QByteArray &audioData, bool isEnd)
{
    if (audioData.isEmpty()) {
        return;
    }

    // 科大讯飞语音识别HTTP API URL (使用HTTPS)
    QString url = "https://raasr.xfyun.cn/v2/iat";

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 设置请求头
    QDateTime now = QDateTime::currentDateTime().toUTC();
    QString date = now.toString("ddd, dd MMM yyyy hh:mm:ss") + " GMT";

    request.setRawHeader("Host", "raasr.xfyun.cn");
    request.setRawHeader("Date", date.toUtf8());
    request.setRawHeader("Authorization", createAuthParams().toUtf8());

    // 构建JSON请求体
    QJsonObject requestBody;
    QJsonObject common;
    common.insert("app_id", m_appId);
    requestBody.insert("common", common);

    QJsonObject business;
    business.insert("language", "zh_cn");
    business.insert("domain", "iat");
    business.insert("accent", "mandarin");
    requestBody.insert("business", business);

    QJsonObject data;
    data.insert("status", isEnd ? 2 : (m_frameIndex == 0 ? 0 : 1)); // 0:开始, 1:继续, 2:结束
    data.insert("format", "audio/L16;rate=16000");
    data.insert("encoding", "raw");
    data.insert("audio", base64Encode(audioData).toUtf8().constData());
    requestBody.insert("data", data);

    QJsonDocument doc(requestBody);
    QByteArray postData = doc.toJson(QJsonDocument::Compact);

    qDebug() << "发送音频数据进行识别，大小:" << audioData.size() << "字节, 状态:" << (isEnd ? "结束" : "继续");

    // 发送POST请求
    m_networkManager->post(request, postData);
    m_frameIndex++;
}

QString VoiceRecognition::createAuthParams()
{
    QDateTime now = QDateTime::currentDateTime().toUTC();
    QString date = now.toString("ddd, dd MMM yyyy hh:mm:ss") + " GMT";

    // 生成签名
    QString signature = generateSignature(date);

    // 科大讯飞HTTP API的认证格式
    QString authOrigin = QString("api_key=\"%1\", algorithm=\"%2\", headers=\"%3\", signature=\"%4\"")
                        .arg(m_apiKey)
                        .arg("hmac-sha256")
                        .arg("host date request-line")
                        .arg(signature);

    return base64Encode(authOrigin.toUtf8());
}

QString VoiceRecognition::generateSignature(const QString &date)
{
    QString host = "raasr.xfyun.cn";
    QString path = "/v2/iat";

    // 拼接签名字符串
    QString signatureOrigin = QString("host: %1\ndate: %2\nPOST %3 HTTP/1.1")
                            .arg(host)
                            .arg(date)
                            .arg(path);

    // HMAC-SHA256加密
    QByteArray signatureSha = hmacSha256(m_apiSecret.toUtf8(), signatureOrigin.toUtf8());

    return base64Encode(signatureSha);
}

QByteArray VoiceRecognition::hmacSha256(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

QString VoiceRecognition::base64Encode(const QByteArray &data)
{
    return data.toBase64();
}

void VoiceRecognition::onRecognitionFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit recognitionError("网络请求错误: " + reply->errorString());
        return;
    }

    QByteArray responseData = reply->readAll();
    qDebug() << "收到识别响应:" << responseData;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &error);

    if (error.error != QJsonParseError::NoError) {
        emit recognitionError("JSON解析错误: " + error.errorString());
        return;
    }

    QJsonObject response = doc.object();
    int code = response["code"].toInt();

    if (code != 0) {
        QString errMsg = response["message"].toString();
        emit recognitionError(QString("API调用错误: %1 (code: %2)").arg(errMsg).arg(code));
        return;
    }

    // 解析识别结果
    QJsonObject data = response["data"].toObject();
    QJsonObject result = data["result"].toObject();
    QJsonArray ws = result["ws"].toArray();

    QString recognizedText;
    for (const QJsonValue &wsValue : ws) {
        QJsonObject wsObj = wsValue.toObject();
        QJsonArray cw = wsObj["cw"].toArray();

        for (const QJsonValue &cwValue : cw) {
            QJsonObject cwObj = cwValue.toObject();
            recognizedText += cwObj["w"].toString();
        }
    }

    if (!recognizedText.isEmpty()) {
        emit recognitionResult(recognizedText);
        qDebug() << "识别结果:" << recognizedText;
    }

    // 如果不是实时模式，表示识别完成
    if (!m_isRealtimeMode) {
        emit recognitionFinished();
    }
}

void VoiceRecognition::initAudioInput()
{
    // 设置音频格式
    m_audioFormat.setSampleRate(16000);        // 16kHz
    m_audioFormat.setChannelCount(1);          // 单声道
    m_audioFormat.setSampleFormat(QAudioFormat::Int16); // 16位

    // 获取默认音频输入设备
    QAudioDevice audioDevice = QMediaDevices::defaultAudioInput();
    if (audioDevice.isNull()) {
        qDebug() << "没有找到音频输入设备";
        return;
    }

    // 检查格式是否支持
    if (!audioDevice.isFormatSupported(m_audioFormat)) {
        qDebug() << "音频格式不支持，使用最接近的格式";
        m_audioFormat = audioDevice.preferredFormat();
    }

    // 创建音频源 (Qt 6的API)
    m_audioSource = new QAudioSource(audioDevice, m_audioFormat, this);

    qDebug() << "音频输入初始化完成";
    qDebug() << "采样率:" << m_audioFormat.sampleRate();
    qDebug() << "声道数:" << m_audioFormat.channelCount();
    qDebug() << "采样格式:" << (int)m_audioFormat.sampleFormat();
}