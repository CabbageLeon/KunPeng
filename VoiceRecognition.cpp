#include "VoiceRecognition.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <QMessageAuthenticationCode>

VoiceRecognition::VoiceRecognition(QObject *parent)
    : QObject(parent)
    , m_webSocket(nullptr)
    , m_audioSource(nullptr)
    , m_audioIODevice(nullptr)
    , m_audioTimer(new QTimer(this))
    , m_audioStatus(STATUS_FIRST_FRAME)
{
    // 预设API参数 - 请替换为您的实际参数
    m_appId = "581ffbe4";
    m_apiKey = "c43e133e41862c3aa2495bae6c2268ef";
    m_apiSecret = "OTgxYWRlNDdiYzFmZTBhNDRhM2NlYTE1";
    
    setupAudioFormat();
    initAudioInput();
    
    // 设置音频处理定时器
    m_audioTimer->setInterval(40); // 40ms 对应Python中的0.04秒
    connect(m_audioTimer, &QTimer::timeout, this, &VoiceRecognition::processAudioBuffer);
}

VoiceRecognition::~VoiceRecognition()
{
    stopRecognition();
    if (m_audioSource) {  // 替换 m_audioInput
        delete m_audioSource;
    }
    if (m_webSocket) {
        delete m_webSocket;
    }
}

void VoiceRecognition::setupAudioFormat()
{
    // 按照科大讯飞API要求设置音频格式: 16kHz, 16bit, 单声道
    m_audioFormat.setSampleRate(16000);
    m_audioFormat.setChannelCount(1);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);
}

void VoiceRecognition::initAudioInput()
{
    QAudioDevice audioDevice = QMediaDevices::defaultAudioInput();
    if (audioDevice.isNull()) {
        emit recognitionError("未找到可用的音频输入设备");
        return;
    }
    
    m_audioDevice = audioDevice;
    
    if (!m_audioDevice.isFormatSupported(m_audioFormat)) {
        emit recognitionError("音频设备不支持所需格式");
        return;
    }
    
    // 使用QAudioSource而不是QAudioInput
    m_audioSource = new QAudioSource(m_audioDevice, m_audioFormat, this);
}

void VoiceRecognition::startRealtimeRecognition()
{
    if (!m_audioSource) {
        emit recognitionError("音频输入未初始化");
        return;
    }
    
    // 创建WebSocket连接
    if (m_webSocket) {
        m_webSocket->deleteLater();
    }
    
    m_webSocket = new QWebSocket();
    
    // 连接WebSocket信号
    connect(m_webSocket, &QWebSocket::connected, this, &VoiceRecognition::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &VoiceRecognition::onWebSocketDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &VoiceRecognition::onWebSocketMessage);
    connect(m_webSocket, &QWebSocket::errorOccurred, this, &VoiceRecognition::onWebSocketError);
    
    // 连接到科大讯飞WebSocket服务器
    QString wsUrl = createWebSocketUrl();
    qDebug() << "连接到WebSocket URL:" << wsUrl;
    m_webSocket->open(QUrl(wsUrl));
}

void VoiceRecognition::stopRecognition()
{
    m_audioTimer->stop();
    
    if (m_audioIODevice) {
        // 停止音频输入的正确方式
        m_audioSource->stop();  // 停止音频源
        disconnect(m_audioIODevice, &QIODevice::readyRead, this, &VoiceRecognition::onAudioDataReady);
        m_audioIODevice = nullptr;
    }
    
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        // 发送最后一帧
        if (!m_audioBuffer.isEmpty()) {
            sendLastFrame(m_audioBuffer);
            m_audioBuffer.clear();
        } else {
            // 发送空的最后一帧
            sendLastFrame(QByteArray());
        }
        
        // 等待一段时间后关闭连接
        QThread::msleep(100);
        m_webSocket->close();
    }
    
    // 发出累积的音频数据用于声纹识别
    if (!m_completeAudioData.isEmpty()) {
        emit audioDataReceived(m_completeAudioData);
        m_completeAudioData.clear(); // 清空音频数据
    }
}

QString VoiceRecognition::createWebSocketUrl()
{
    // 生成RFC1123格式的时间戳
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString date = now.toString("ddd, dd MMM yyyy hh:mm:ss 'GMT'");
    
    // 拼接字符串 (对应Python中的signature_origin)
    QString signatureOrigin = QString("host: %1\n").arg("ws-api.xfyun.cn");
    signatureOrigin += QString("date: %1\n").arg(date);
    signatureOrigin += "GET /v2/iat HTTP/1.1";
    
    // 进行HMAC-SHA256加密
    QByteArray key = m_apiSecret.toUtf8();
    QByteArray data = signatureOrigin.toUtf8();
    QByteArray signature = QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
    QString signatureSha = signature.toBase64();
    
    // 生成authorization字符串
    QString authorizationOrigin = QString("api_key=\"%1\", algorithm=\"%2\", headers=\"%3\", signature=\"%4\"")
                                      .arg(m_apiKey)
                                      .arg("hmac-sha256")
                                      .arg("host date request-line")
                                      .arg(signatureSha);
    
    QString authorization = authorizationOrigin.toUtf8().toBase64();
    
    // 组装URL参数
    QUrlQuery query;
    query.addQueryItem("authorization", authorization);
    query.addQueryItem("date", date);
    query.addQueryItem("host", "ws-api.xfyun.cn");
    
    QString url = "wss://ws-api.xfyun.cn/v2/iat?" + query.toString();
    return url;
}

void VoiceRecognition::onWebSocketConnected()
{
    qDebug() << "WebSocket连接成功";

    // 重置音频状态
    m_audioStatus = STATUS_FIRST_FRAME;
    m_audioBuffer.clear();
    m_completeAudioData.clear(); // 清空之前的音频数据

    // 开始音频输入的正确方式
    m_audioIODevice = m_audioSource->start();  // 这会返回一个QIODevice
    connect(m_audioIODevice, &QIODevice::readyRead, this, &VoiceRecognition::onAudioDataReady);

    // 开始定时器处理音频缓冲区
    m_audioTimer->start();

    qDebug() << "开始录音和识别";
}


void VoiceRecognition::onWebSocketDisconnected()
{
    qDebug() << "WebSocket连接断开";
    m_audioTimer->stop();
    
    if (m_audioIODevice) {
        // 停止音频输入的正确方式
        m_audioSource->stop();
        disconnect(m_audioIODevice, &QIODevice::readyRead, this, &VoiceRecognition::onAudioDataReady);
        m_audioIODevice = nullptr;
    }
    
    emit recognitionFinished();
}

void VoiceRecognition::onWebSocketMessage(const QString &message)
{
    qDebug() << "收到WebSocket消息:" << message;
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit recognitionError("解析服务器响应失败: " + error.errorString());
        return;
    }
    
    QJsonObject obj = doc.object();
    int code = obj.value("code").toInt();
    QString sid = obj.value("sid").toString();
    
    if (code != 0) {
        QString errMsg = obj.value("message").toString();
        emit recognitionError(QString("API调用错误: %1 (code: %2)").arg(errMsg).arg(code));
        return;
    }
    
    // 解析识别结果
    QJsonObject data = obj.value("data").toObject();
    QJsonObject result = data.value("result").toObject();
    QJsonArray ws = result.value("ws").toArray();
    
    QString recognizedText;
    for (const QJsonValue &wsValue : ws) {
        QJsonObject wsObj = wsValue.toObject();
        QJsonArray cw = wsObj.value("cw").toArray();
        
        for (const QJsonValue &cwValue : cw) {
            QJsonObject cwObj = cwValue.toObject();
            QString w = cwObj.value("w").toString();
            recognizedText += w;
        }
    }
    
    if (!recognizedText.isEmpty()) {
        emit recognitionResult(recognizedText);
        qDebug() << "识别结果:" << recognizedText;
    }
}

void VoiceRecognition::onWebSocketError(QAbstractSocket::SocketError error)
{
    QString errorString;
    switch (error) {
        case QAbstractSocket::RemoteHostClosedError:
            errorString = "远程主机关闭连接";
            break;
        case QAbstractSocket::HostNotFoundError:
            errorString = "未找到主机";
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorString = "连接被拒绝";
            break;
        default:
            errorString = QString("WebSocket错误: %1").arg(error);
            break;
    }
    
    emit recognitionError(errorString);
    qDebug() << "WebSocket错误:" << errorString;
}

void VoiceRecognition::onAudioDataReady()
{
    if (!m_audioIODevice) {
        return;
    }
    
    QByteArray data = m_audioIODevice->readAll();
    if (!data.isEmpty()) {
        m_audioBuffer.append(data);
        m_completeAudioData.append(data); // 累积完整音频数据用于声纹识别
    }
}

void VoiceRecognition::processAudioBuffer()
{
    if (m_audioBuffer.isEmpty() || !m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    // 每次处理8000字节 (对应Python中的frameSize)
    const int frameSize = 8000;
    
    if (m_audioBuffer.size() >= frameSize) {
        QByteArray frame = m_audioBuffer.left(frameSize);
        m_audioBuffer.remove(0, frameSize);
        
        sendAudioFrame(frame);
    }
}

void VoiceRecognition::sendAudioFrame(const QByteArray &audioData)
{
    switch (m_audioStatus) {
        case STATUS_FIRST_FRAME:
            sendFirstFrame(audioData);
            m_audioStatus = STATUS_CONTINUE_FRAME;
            break;
        case STATUS_CONTINUE_FRAME:
            sendContinueFrame(audioData);
            break;
        case STATUS_LAST_FRAME:
            sendLastFrame(audioData);
            break;
    }
}

void VoiceRecognition::sendFirstFrame(const QByteArray &audioData)
{
    QJsonObject commonArgs;
    commonArgs["app_id"] = m_appId;
    
    QJsonObject businessArgs;
    businessArgs["domain"] = "iat";
    businessArgs["language"] = "zh_cn";
    businessArgs["accent"] = "mandarin";
    businessArgs["vinfo"] = 1;
    businessArgs["vad_eos"] = 10000;
    
    QJsonObject dataArgs;
    dataArgs["status"] = 0;
    dataArgs["format"] = "audio/L16;rate=16000";
    dataArgs["audio"] = QString::fromUtf8(audioData.toBase64());
    dataArgs["encoding"] = "raw";
    
    QJsonObject message;
    message["common"] = commonArgs;
    message["business"] = businessArgs;
    message["data"] = dataArgs;
    
    QString jsonString = QJsonDocument(message).toJson(QJsonDocument::Compact);
    m_webSocket->sendTextMessage(jsonString);
    
    qDebug() << "发送第一帧音频数据";
}

void VoiceRecognition::sendContinueFrame(const QByteArray &audioData)
{
    QJsonObject dataArgs;
    dataArgs["status"] = 1;
    dataArgs["format"] = "audio/L16;rate=16000";
    dataArgs["audio"] = QString::fromUtf8(audioData.toBase64());
    dataArgs["encoding"] = "raw";
    
    QJsonObject message;
    message["data"] = dataArgs;
    
    QString jsonString = QJsonDocument(message).toJson(QJsonDocument::Compact);
    m_webSocket->sendTextMessage(jsonString);
    
}

void VoiceRecognition::sendLastFrame(const QByteArray &audioData)
{
    QJsonObject dataArgs;
    dataArgs["status"] = 2;
    dataArgs["format"] = "audio/L16;rate=16000";
    dataArgs["audio"] = QString::fromUtf8(audioData.toBase64());
    dataArgs["encoding"] = "raw";
    
    QJsonObject message;
    message["data"] = dataArgs;
    
    QString jsonString = QJsonDocument(message).toJson(QJsonDocument::Compact);
    m_webSocket->sendTextMessage(jsonString);
    
    qDebug() << "发送最后一帧音频数据";
}
