#include "VoiceGenerator.h"
#include <QApplication>
#include <QStandardPaths>
#include <QUuid>
#include <QFileInfo>
#include <QMessageAuthenticationCode>

VoiceGenerator::VoiceGenerator(QObject *parent)
    : QObject(parent)
    , m_webSocket(nullptr)
    , m_voice("x4_yezi")      // 默认音色：叶子
    , m_audioFormat("raw")     // 原始音频格式
    , m_sampleRate("16000")    // 16kHz采样率
    , m_encoding("utf8")       // UTF-8编码
    , m_isSynthesizing(false)
{
    qDebug() << "VoiceGenerator初始化";
}

VoiceGenerator::~VoiceGenerator()
{
    if (m_webSocket) {
        m_webSocket->close();
        m_webSocket->deleteLater();
    }
}

void VoiceGenerator::setCredentials(const QString &appId, const QString &apiKey, const QString &apiSecret)
{
    m_appId = appId;
    m_apiKey = apiKey;
    m_apiSecret = apiSecret;
    
    qDebug() << "VoiceGenerator设置认证信息:";
    qDebug() << "App ID:" << m_appId;
    qDebug() << "API Key:" << m_apiKey;
    qDebug() << "API Secret:" << m_apiSecret;
}

void VoiceGenerator::setVoice(const QString &voice)
{
    m_voice = voice;
    qDebug() << "设置音色为:" << m_voice;
}

void VoiceGenerator::synthesizeText(const QString &text, const QString &outputPath)
{
    if (m_isSynthesizing) {
        qDebug() << "正在合成中，请等待当前合成完成";
        emit synthesisError("正在合成中，请等待当前合成完成");
        return;
    }
    
    if (text.isEmpty()) {
        qDebug() << "文本为空，无法进行语音合成";
        emit synthesisError("文本为空，无法进行语音合成");
        return;
    }
    
    if (m_appId.isEmpty() || m_apiKey.isEmpty() || m_apiSecret.isEmpty()) {
        qDebug() << "API认证信息未设置";
        emit synthesisError("API认证信息未设置");
        return;
    }
    
    m_currentText = text;
    m_outputPath = outputPath.isEmpty() ? generateOutputPath(text) : outputPath;
    clearAudioBuffer();
    
    qDebug() << "开始语音合成:";
    qDebug() << "文本:" << text;
    qDebug() << "输出路径:" << m_outputPath;
    qDebug() << "音色:" << m_voice;
    
    // 创建WebSocket连接
    if (m_webSocket) {
        m_webSocket->deleteLater();
    }
    
    m_webSocket = new QWebSocket();
    
    connect(m_webSocket, &QWebSocket::connected, this, &VoiceGenerator::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &VoiceGenerator::onWebSocketDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &VoiceGenerator::onWebSocketMessage);
    connect(m_webSocket, &QWebSocket::errorOccurred,
            this, &VoiceGenerator::onWebSocketError);
    
    // 创建WebSocket URL并连接
    QString wsUrl = createWebSocketUrl();
    qDebug() << "连接到TTS WebSocket URL:" << wsUrl;
    
    m_webSocket->open(QUrl(wsUrl));
    m_isSynthesizing = true;
    emit synthesisStarted();
}

void VoiceGenerator::stopSynthesis()
{
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->close();
    }
    
    m_isSynthesizing = false;
    qDebug() << "语音合成已停止";
}

QString VoiceGenerator::createWebSocketUrl()
{
    QString url = "wss://tts-api.xfyun.cn/v2/tts";
    
    // 生成RFC1123格式的时间戳
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString date = formatHttpDate(now);
    
    // 构建签名字符串
    QString signatureOrigin = QString("host: ws-api.xfyun.cn\n");
    signatureOrigin += QString("date: %1\n").arg(date);
    signatureOrigin += QString("GET /v2/tts HTTP/1.1");
    
    // 进行HMAC-SHA256加密
    QByteArray secretKey = m_apiSecret.toUtf8();
    QByteArray signature = QMessageAuthenticationCode::hash(
        signatureOrigin.toUtf8(), secretKey, QCryptographicHash::Sha256);
    QString signatureBase64 = signature.toBase64();
    
    // 构建authorization字符串
    QString authorization = QString("api_key=\"%1\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%2\"")
                           .arg(m_apiKey, signatureBase64);
    QString authorizationBase64 = authorization.toUtf8().toBase64();
    
    // 构建URL查询参数
    QUrlQuery query;
    query.addQueryItem("authorization", authorizationBase64);
    query.addQueryItem("date", date);
    query.addQueryItem("host", "ws-api.xfyun.cn");
    
    url += "?" + query.toString();
    return url;
}

void VoiceGenerator::onWebSocketConnected()
{
    qDebug() << "TTS WebSocket连接成功";
    
    // 构建合成请求
    QJsonObject request = buildSynthesisRequest(m_currentText);
    
    // 发送请求
    QJsonDocument doc(request);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);
    
    qDebug() << "发送TTS请求:" << jsonStr;
    m_webSocket->sendTextMessage(jsonStr);
}

void VoiceGenerator::onWebSocketDisconnected()
{
    qDebug() << "TTS WebSocket连接已断开";
    
    if (m_isSynthesizing) {
        // 保存音频文件
        if (!m_audioBuffer.isEmpty()) {
            if (saveAudioAsWav(m_audioBuffer, m_outputPath)) {
                qDebug() << "语音合成完成，音频文件已保存:" << m_outputPath;
                emit synthesisFinished(m_outputPath);
            } else {
                qDebug() << "音频文件保存失败";
                emit synthesisError("音频文件保存失败");
            }
        } else {
            qDebug() << "没有接收到音频数据";
            emit synthesisError("没有接收到音频数据");
        }
        
        m_isSynthesizing = false;
    }
}

void VoiceGenerator::onWebSocketMessage(const QString &message)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "解析TTS响应消息失败:" << parseError.errorString();
        emit synthesisError("解析TTS响应消息失败");
        return;
    }
    
    QJsonObject obj = doc.object();
    
    int code = obj.value("code").toInt();
    QString sid = obj.value("sid").toString();
    
    qDebug() << "收到TTS响应，code:" << code << "sid:" << sid;
    
    if (code != 0) {
        QString errMsg = obj.value("message").toString();
        qDebug() << "TTS错误，code:" << code << "message:" << errMsg;
        emit synthesisError(QString("TTS API错误: %1 (code: %2)").arg(errMsg).arg(code));
        m_isSynthesizing = false;
        return;
    }
    
    // 解析音频数据
    QJsonObject data = obj.value("data").toObject();
    QString audioBase64 = data.value("audio").toString();
    int status = data.value("status").toInt();
    
    if (!audioBase64.isEmpty()) {
        QByteArray audioData = QByteArray::fromBase64(audioBase64.toUtf8());
        m_audioBuffer.append(audioData);
        
        // 发射实时音频数据信号
        emit audioDataReceived(audioData);
        
        qDebug() << "接收到音频数据:" << audioData.size() << "字节，总计:" << m_audioBuffer.size() << "字节";
    }
    
    // 状态2表示合成完成
    if (status == 2) {
        qDebug() << "语音合成完成，准备关闭连接";
        m_webSocket->close();
    }
    
    // 计算进度（简单估算）
    if (status == 1 || status == 2) {
        int progress = (status == 2) ? 100 : 50;
        emit synthesisProgress(progress);
    }
}

void VoiceGenerator::onWebSocketError(QAbstractSocket::SocketError error)
{
    qDebug() << "TTS WebSocket错误:" << error;
    emit synthesisError(QString("WebSocket连接错误: %1").arg(error));
    m_isSynthesizing = false;
}

QJsonObject VoiceGenerator::buildSynthesisRequest(const QString &text)
{
    QJsonObject request;
    
    // 构建common参数
    QJsonObject common;
    common["app_id"] = m_appId;
    request["common"] = common;
    
    // 构建business参数
    QJsonObject business;
    business["aue"] = m_audioFormat;      // 音频编码
    business["auf"] = QString("audio/L16;rate=%1").arg(m_sampleRate); // 音频格式
    business["vcn"] = m_voice;            // 发音人
    business["tte"] = m_encoding;         // 文本编码
    request["business"] = business;
    
    // 构建data参数
    QJsonObject data;
    data["status"] = 2; // 一次性发送
    data["text"] = encodeTextToBase64(text);
    request["data"] = data;
    
    return request;
}

QString VoiceGenerator::encodeTextToBase64(const QString &text)
{
    return text.toUtf8().toBase64();
}

QString VoiceGenerator::generateOutputPath(const QString &text)
{
    // 在应用程序目录下生成唯一的文件名
    QString appDir = QApplication::applicationDirPath();
    QString fileName = QString("tts_output_%1.wav")
                      .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"));
    return QDir(appDir).absoluteFilePath(fileName);
}

bool VoiceGenerator::saveAudioAsWav(const QByteArray &audioData, const QString &filePath)
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
        quint32 sampleRate = 16000; // 采样率 (固定16kHz)
        quint32 byteRate = 16000 * 1 * 16 / 8; // 字节率
        quint16 blockAlign = 1 * 16 / 8; // 块对齐
        quint16 bitsPerSample = 16; // 位深
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

void VoiceGenerator::clearAudioBuffer()
{
    m_audioBuffer.clear();
}

QString VoiceGenerator::formatHttpDate(const QDateTime &dateTime)
{
    // 格式化为RFC1123 HTTP日期格式
    static const char* const dayNames[] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    static const char* const monthNames[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    QDate date = dateTime.date();
    QTime time = dateTime.time();
    
    return QString("%1, %2 %3 %4 %5:%6:%7 GMT")
           .arg(dayNames[date.dayOfWeek() - 1])
           .arg(date.day(), 2, 10, QChar('0'))
           .arg(monthNames[date.month() - 1])
           .arg(date.year())
           .arg(time.hour(), 2, 10, QChar('0'))
           .arg(time.minute(), 2, 10, QChar('0'))
           .arg(time.second(), 2, 10, QChar('0'));
}

void VoiceGenerator::parseUrl(const QString &url, QString &host, QString &path)
{
    QUrl qurl(url);
    host = qurl.host();
    path = qurl.path();
    if (path.isEmpty()) {
        path = "/";
    }
}
