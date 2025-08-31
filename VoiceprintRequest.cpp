#include "VoiceprintRequest.h"
#include <QFile>
#include <QIODevice>
#include <QByteArray>
#include <QDebug>
#include <QLocale>
#include <QApplication>
#include <QDataStream>


VoiceprintRequest::VoiceprintRequest(QObject *parent)
    : QObject(parent)
    , m_baseUrl("https://api.xf-yun.com/v1/private/s782b4996")
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
{
    qDebug() << QString::fromUtf8("VoiceprintRequest初始化，基础URL:") << m_baseUrl;
    
    // 配置网络管理器
    m_networkManager->setTransferTimeout(30000); // 30秒超时
    
    // 配置SSL
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // 临时禁用SSL验证以排除SSL问题
    QSslConfiguration::setDefaultConfiguration(sslConfig);
}

VoiceprintRequest::~VoiceprintRequest()
{
    // 断开所有网络连接
    if (m_networkManager) {
        disconnect(m_networkManager, nullptr, this, nullptr);
    }
    
    // 安全地清理当前请求
    if (m_currentReply) {
        disconnect(m_currentReply, nullptr, this, nullptr);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void VoiceprintRequest::setCredentials(const QString &appId, const QString &apiKey, const QString &apiSecret)
{
    m_appId = appId;
    m_apiKey = apiKey;
    m_apiSecret = apiSecret; // 直接使用原始API Secret，不进行任何转换
    
    qDebug() << QString::fromUtf8("设置认证信息:");
    qDebug() << QString::fromUtf8("App ID:") << m_appId;
    qDebug() << QString::fromUtf8("API Key:") << m_apiKey;
    qDebug() << QString::fromUtf8("API Secret:") << m_apiSecret;
    
    // 验证基本格式
    if (m_appId.isEmpty() || m_apiKey.isEmpty() || m_apiSecret.isEmpty()) {
        qWarning() << QString::fromUtf8("警告: 认证信息不完整");
    }
    
    // 测试认证信息
    testAuthentication();
}

void VoiceprintRequest::createGroup(const QString &groupId, const QString &groupName, const QString &groupInfo)
{
    QJsonObject body = generateRequestBody("createGroup");
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["groupName"] = groupName;
    s782b4996["groupInfo"] = groupInfo;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::createFeature(const QString &audioFilePath, const QString &groupId, 
                                      const QString &featureId, const QString &featureInfo)
{
    QJsonObject body = generateRequestBody("createFeature", audioFilePath);
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["featureId"] = featureId;
    s782b4996["featureInfo"] = featureInfo;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::queryFeatureList(const QString &groupId)
{
    QJsonObject body = generateRequestBody("queryFeatureList");
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::searchFeature(const QString &audioFilePath, const QString &groupId, int topK)
{
    qDebug() << QString::fromUtf8("开始声纹识别，音频文件:") << audioFilePath;
    
    // 检查文件是否存在
    QFile file(audioFilePath);
    if (!file.exists()) {
        qDebug() << QString::fromUtf8("音频文件不存在:") << audioFilePath;
        emit requestError(QString::fromUtf8("音频文件不存在"));
        return;
    }
    
    qDebug() << QString::fromUtf8("音频文件大小:") << file.size() << QString::fromUtf8("字节");
    
    QJsonObject body = generateRequestBody("searchFea", audioFilePath);
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["topK"] = topK;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::searchFeatureWithData(const QByteArray &audioData, const QString &groupId, int topK)
{
    qDebug() << QString::fromUtf8("开始声纹识别，音频数据大小:") << audioData.size() << QString::fromUtf8("字节");
    
    if (audioData.isEmpty()) {
        emit requestError(QString::fromUtf8("音频数据为空"));
        return;
    }
    
    // 讯飞声纹识别要求：至少2秒，最多60秒的音频
    const int minAudioSize = 16000 * 2 * 2; // 2秒的音频数据 (16kHz * 2字节 * 2秒)
    const int maxAudioSize = 16000 * 2 * 60; // 60秒的音频数据
    
    if (audioData.size() < minAudioSize) {
        qDebug() << QString::fromUtf8("音频数据太小，需要至少2秒的数据");
        emit requestError(QString::fromUtf8("音频数据太小，声纹识别至少需要2秒的录音数据"));
        return;
    }
    
    QByteArray processedData = audioData;
    if (processedData.size() > maxAudioSize) {
        qDebug() << QString::fromUtf8("音频数据太大，截取前60秒");
        processedData = processedData.left(maxAudioSize);
    }
    
    // 确保音频长度为偶数（16位音频的要求）
    if (processedData.size() % 2 != 0) {
        processedData = processedData.left(processedData.size() - 1);
    }
    
    qDebug() << QString::fromUtf8("处理后的音频数据大小:") << processedData.size() << QString::fromUtf8("字节");
    qDebug() << QString::fromUtf8("音频时长约:") << (processedData.size() / (16000.0 * 2)) << QString::fromUtf8("秒");
    
    // 生成请求体
    QJsonObject body = generateRequestBody("searchFea");
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["topK"] = topK;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    // 直接使用PCM数据
    QJsonObject payload;
    QJsonObject resource;
    resource["encoding"] = "raw";
    resource["sample_rate"] = 16000;
    resource["channels"] = 1;
    resource["bit_depth"] = 16;
    resource["status"] = 3;
    resource["audio"] = QString(processedData.toBase64());
    payload["resource"] = resource;
    body["payload"] = payload;
    
    qDebug() << QString::fromUtf8("音频数据Base64长度:") << QString(processedData.toBase64()).length();
    
    sendRequest(body);
}

void VoiceprintRequest::searchScoreFeature(const QString &audioFilePath, const QString &groupId, 
                                           const QString &dstFeatureId)
{
    QJsonObject body = generateRequestBody("searchScoreFea", audioFilePath);
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["dstFeatureId"] = dstFeatureId;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::updateFeature(const QString &audioFilePath, const QString &groupId, 
                                      const QString &featureId, const QString &featureInfo)
{
    QJsonObject body = generateRequestBody("updateFeature", audioFilePath);
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["featureId"] = featureId;
    s782b4996["featureInfo"] = featureInfo;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::deleteFeature(const QString &groupId, const QString &featureId)
{
    QJsonObject body = generateRequestBody("deleteFeature");
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    s782b4996["featureId"] = featureId;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::deleteGroup(const QString &groupId)
{
    QJsonObject body = generateRequestBody("deleteGroup");
    
    // 修改默认参数
    QJsonObject parameter = body["parameter"].toObject();
    QJsonObject s782b4996 = parameter["s782b4996"].toObject();
    s782b4996["groupId"] = groupId;
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    sendRequest(body);
}

void VoiceprintRequest::testAuthentication()
{
    qDebug() << QString::fromUtf8("=== 认证信息测试 ===");
    qDebug() << QString::fromUtf8("App ID:") << m_appId;
    qDebug() << QString::fromUtf8("API Key:") << m_apiKey;
    qDebug() << QString::fromUtf8("API Secret:") << m_apiSecret;
    
    // 测试签名生成
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString date = formatHttpDate(now);
    QString signature = generateSignature("api.xf-yun.com", date, "POST", "/v1/private/s782b4996");
    qDebug() << QString::fromUtf8("测试签名:") << signature;
    qDebug() << QString::fromUtf8("==================");
}

QString VoiceprintRequest::assembleAuthUrl(const QString &requestUrl, const QString &method)
{
    // 这个方法现在主要用于调试，实际认证在sendRequest中处理
    QString host, path;
    parseUrl(requestUrl, host, path);
    
    // 获取UTC时间并确保时区正确
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString date = formatHttpDate(now);
    
    QString signature = generateSignature(host, date, method, path);
    
    QString authorizationOrigin = QString("api_key=\"%1\", algorithm=\"%2\", headers=\"%3\", signature=\"%4\"")
                                  .arg(m_apiKey)
                                  .arg("hmac-sha256")
                                  .arg("host date request-line")
                                  .arg(signature);
    
    QString authorization = QByteArray(authorizationOrigin.toUtf8()).toBase64();
    
    // 按照Python示例添加查询参数到URL
    QUrlQuery query;
    query.addQueryItem("host", host);
    query.addQueryItem("date", date);
    query.addQueryItem("authorization", authorization);
    
    QString finalUrl = requestUrl + "?" + query.toString();
    
    return finalUrl;
}

QString VoiceprintRequest::generateSignature(const QString &host, const QString &date, 
                                             const QString &method, const QString &path)
{
    // 按照iFlytek API文档的要求构建签名字符串
    // 格式：host: $host\ndate: $date\n$request-line
    QString requestLine = QString("%1 %2 HTTP/1.1").arg(method.toUpper()).arg(path);
    QString signatureOrigin = QString("host: %1\ndate: %2\n%3")
                              .arg(host)
                              .arg(date)
                              .arg(requestLine);
    
    // 直接使用API Secret字符串
    QByteArray secretBytes = m_apiSecret.toUtf8();
    QByteArray dataBytes = signatureOrigin.toUtf8();
    
    QByteArray signature = QMessageAuthenticationCode::hash(dataBytes, secretBytes, QCryptographicHash::Sha256);
    QString base64Signature = signature.toBase64();
    
    return base64Signature;
}

QJsonObject VoiceprintRequest::generateRequestBody(const QString &apiName, const QString &audioFilePath)
{
    QJsonObject body;
    
    // 构建header
    QJsonObject header;
    header["app_id"] = m_appId;
    header["status"] = 3;
    body["header"] = header;
    
    // 构建parameter
    QJsonObject parameter;
    QJsonObject s782b4996;
    s782b4996["func"] = apiName;
    
    if (apiName == "createFeature") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        s782b4996["featureId"] = "iFLYTEK_examples_featureId";
        s782b4996["featureInfo"] = "iFLYTEK_examples_featureInfo";
        
        QJsonObject createFeatureRes;
        createFeatureRes["encoding"] = "utf8";
        createFeatureRes["compress"] = "raw";
        createFeatureRes["format"] = "json";
        s782b4996["createFeatureRes"] = createFeatureRes;
        
        // 构建payload
        if (!audioFilePath.isEmpty()) {
            QJsonObject payload;
            QJsonObject resource;
            resource["encoding"] = "raw";
            resource["sample_rate"] = 16000;
            resource["channels"] = 1;
            resource["bit_depth"] = 16;
            resource["status"] = 3;
            resource["audio"] = encodeAudioFile(audioFilePath);
            payload["resource"] = resource;
            body["payload"] = payload;
        }
    }
    else if (apiName == "createGroup") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        s782b4996["groupName"] = "iFLYTEK_examples_groupName";
        s782b4996["groupInfo"] = "iFLYTEK_examples_groupInfo";
        
        QJsonObject createGroupRes;
        createGroupRes["encoding"] = "utf8";
        createGroupRes["compress"] = "raw";
        createGroupRes["format"] = "json";
        s782b4996["createGroupRes"] = createGroupRes;
    }
    else if (apiName == "deleteFeature") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        s782b4996["featureId"] = "iFLYTEK_examples_featureId";
        
        QJsonObject deleteFeatureRes;
        deleteFeatureRes["encoding"] = "utf8";
        deleteFeatureRes["compress"] = "raw";
        deleteFeatureRes["format"] = "json";
        s782b4996["deleteFeatureRes"] = deleteFeatureRes;
    }
    else if (apiName == "queryFeatureList") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        
        QJsonObject queryFeatureListRes;
        queryFeatureListRes["encoding"] = "utf8";
        queryFeatureListRes["compress"] = "raw";
        queryFeatureListRes["format"] = "json";
        s782b4996["queryFeatureListRes"] = queryFeatureListRes;
    }
    else if (apiName == "searchFea") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        s782b4996["topK"] = 1;
        
        QJsonObject searchFeaRes;
        searchFeaRes["encoding"] = "utf8";
        searchFeaRes["compress"] = "raw";
        searchFeaRes["format"] = "json";
        s782b4996["searchFeaRes"] = searchFeaRes;
        
        // 构建payload
        if (!audioFilePath.isEmpty()) {
            QJsonObject payload;
            QJsonObject resource;
            resource["encoding"] = "raw";
            resource["sample_rate"] = 16000;
            resource["channels"] = 1;
            resource["bit_depth"] = 16;
            resource["status"] = 3;
            resource["audio"] = encodeAudioFile(audioFilePath);
            payload["resource"] = resource;
            body["payload"] = payload;
        }
    }
    else if (apiName == "searchScoreFea") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        s782b4996["dstFeatureId"] = "iFLYTEK_examples_featureId";
        
        QJsonObject searchScoreFeaRes;
        searchScoreFeaRes["encoding"] = "utf8";
        searchScoreFeaRes["compress"] = "raw";
        searchScoreFeaRes["format"] = "json";
        s782b4996["searchScoreFeaRes"] = searchScoreFeaRes;
        
        // 构建payload
        if (!audioFilePath.isEmpty()) {
            QJsonObject payload;
            QJsonObject resource;
            resource["encoding"] = "raw";
            resource["sample_rate"] = 16000;
            resource["channels"] = 1;
            resource["bit_depth"] = 16;
            resource["status"] = 3;
            resource["audio"] = encodeAudioFile(audioFilePath);
            payload["resource"] = resource;
            body["payload"] = payload;
        }
    }
    else if (apiName == "updateFeature") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        s782b4996["featureId"] = "iFLYTEK_examples_featureId";
        s782b4996["featureInfo"] = "iFLYTEK_examples_featureInfo_update";
        
        QJsonObject updateFeatureRes;
        updateFeatureRes["encoding"] = "utf8";
        updateFeatureRes["compress"] = "raw";
        updateFeatureRes["format"] = "json";
        s782b4996["updateFeatureRes"] = updateFeatureRes;
        
        // 构建payload
        if (!audioFilePath.isEmpty()) {
            QJsonObject payload;
            QJsonObject resource;
            resource["encoding"] = "raw";
            resource["sample_rate"] = 16000;
            resource["channels"] = 1;
            resource["bit_depth"] = 16;
            resource["status"] = 3;
            resource["audio"] = encodeAudioFile(audioFilePath);
            payload["resource"] = resource;
            body["payload"] = payload;
        }
    }
    else if (apiName == "deleteGroup") {
        s782b4996["groupId"] = "iFLYTEK_examples_groupId";
        
        QJsonObject deleteGroupRes;
        deleteGroupRes["encoding"] = "utf8";
        deleteGroupRes["compress"] = "raw";
        deleteGroupRes["format"] = "json";
        s782b4996["deleteGroupRes"] = deleteGroupRes;
    }
    
    parameter["s782b4996"] = s782b4996;
    body["parameter"] = parameter;
    
    return body;
}

QString VoiceprintRequest::encodeAudioFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open audio file:" << filePath;
        return QString();
    }
    
    QByteArray audioData = file.readAll();
    file.close();
    
    // 如果是WAV文件，跳过WAV头部，只提取PCM数据
    if (audioData.size() > 44 && audioData.startsWith("RIFF")) {
        // 标准WAV头部是44字节，从第45字节开始是PCM数据
        QByteArray pcmData = audioData.mid(44);
        qDebug() << QString::fromUtf8("提取PCM数据，大小:") << pcmData.size() << QString::fromUtf8("字节");
        return pcmData.toBase64();
    } else {
        // 如果不是RAW格式，直接编码整个文件
        qDebug() << QString::fromUtf8("编码音频文件，大小:") << audioData.size() << QString::fromUtf8("字节");
        return audioData.toBase64();
    }
}

void VoiceprintRequest::parseUrl(const QString &url, QString &host, QString &path)
{
    QUrl qurl(url);
    host = qurl.host();
    path = qurl.path();
    if (path.isEmpty()) {
        path = "/";
    }
}

QString VoiceprintRequest::formatHttpDate(const QDateTime &dateTime)
{
    // 格式化为RFC1123 HTTP日期格式：Thu, 12 Dec 2019 01:57:27 GMT（24小时制）
    QLocale locale(QLocale::English);
    QString formattedDate = locale.toString(dateTime, "ddd, dd MMM yyyy HH:mm:ss") + " GMT";
    
    // 验证日期格式是否正确
    if (!formattedDate.contains("GMT")) {
        qWarning() << QString::fromUtf8("警告: 日期格式可能不正确:") << formattedDate;
    }
    
    return formattedDate;
}

void VoiceprintRequest::sendRequest(const QJsonObject &body)
{
    // 使用URL参数认证方式（参考Python官方示例）
    QString authUrl = assembleAuthUrl(m_baseUrl, "POST");
    
    qDebug() << QString::fromUtf8("发送声纹识别请求...");
    
    // 为了更好地显示中文，我们创建一个格式化的JSON输出（但不显示音频数据）
    QJsonObject logBody = body;
    
    // 检查并移除音频数据以避免在日志中输出大量编码数据
    if (logBody.contains("payload")) {
        QJsonObject payload = logBody["payload"].toObject();
        if (payload.contains("resource")) {
            QJsonObject resource = payload["resource"].toObject();
            if (resource.contains("audio")) {
                QString audioData = resource["audio"].toString();
                resource["audio"] = QString::fromUtf8("<%1字节的Base64音频数据已隐藏>").arg(audioData.length());
                payload["resource"] = resource;
                logBody["payload"] = payload;
            }
        }
    }
    
    // 只有在debug模式下才输出完整的请求体
    #ifdef QT_DEBUG
    QJsonDocument doc(logBody);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    qDebug() << QString::fromUtf8("请求体:") << jsonString;
    #endif
    
    QNetworkRequest request;
    request.setUrl(QUrl(authUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 设置更多的HTTP头部
    request.setRawHeader("User-Agent", "KunPeng/1.0");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Connection", "keep-alive");
    
    // 设置请求超时
    request.setTransferTimeout(30000); // 30秒
    
    // 使用原始的body生成请求数据
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    
    // 取消之前的请求
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    m_currentReply = m_networkManager->post(request, data);
    
    if (!m_currentReply) {
        qDebug() << QString::fromUtf8("错误：无法创建网络请求");
        emit requestError(QString::fromUtf8("无法创建网络请求"));
        return;
    }
    
    // 连接信号
    connect(m_currentReply, &QNetworkReply::finished, this, &VoiceprintRequest::onNetworkReplyFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, [this](QNetworkReply::NetworkError error) {
        qDebug() << QString::fromUtf8("网络错误:") << m_currentReply->errorString();
    });
    connect(m_currentReply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &errors) {
        qDebug() << QString::fromUtf8("SSL错误，忽略验证");
        m_currentReply->ignoreSslErrors();
    });
}

void VoiceprintRequest::onNetworkReplyFinished()
{
    qDebug() << QString::fromUtf8("onNetworkReplyFinished被调用");
    
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << QString::fromUtf8("reply对象为空");
        return;
    }
    
    // 确保这是当前的请求
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << QString::fromUtf8("声纹识别响应状态码:") << statusCode;
    
    // 为响应数据创建更好的中文显示
    QString responseString = QString::fromUtf8(responseData);
    qDebug() << QString::fromUtf8("声纹识别响应数据:") << responseString;
    
    // 输出所有响应头信息
    qDebug() << QString::fromUtf8("=== 响应头信息 ===");
    QList<QByteArray> headerList = reply->rawHeaderList();
    for (const QByteArray &header : headerList) {
        qDebug() << QString::fromUtf8(header) << ":" << QString::fromUtf8(reply->rawHeader(header));
    }
    qDebug() << QString::fromUtf8("==================");
    
    // 首先尝试解析JSON响应，无论HTTP状态码如何
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &error);
    
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject jsonResponse = doc.object();
        
        // 检查业务层面的错误
        if (jsonResponse.contains("header")) {
            QJsonObject header = jsonResponse["header"].toObject();
            int code = header["code"].toInt();
            QString message = header["message"].toString();
            
            if (code != 0) {
                // 特殊处理音频数据无效的错误
                if (code == 23003 && message.contains("the audio data is invalid")) {
                    qDebug() << QString::fromUtf8("音频数据无效错误，可能原因:");
                    qDebug() << QString::fromUtf8("1. 音频格式不正确");
                    qDebug() << QString::fromUtf8("2. 音频时长不够（需要至少2-3秒）");
                    qDebug() << QString::fromUtf8("3. 组中没有声纹数据可供比较");
                    qDebug() << QString::fromUtf8("4. 音频质量太差");
                    emit requestError(QString::fromUtf8("音频数据无效: %1").arg(message));
                    reply->deleteLater();
                    if (reply == m_currentReply) {
                        m_currentReply = nullptr;
                    }
                    return;
                }
                
                // 业务错误，但特殊处理"组为空"的情况
                if (message.contains("this groupId is empty") || message.contains("组为空")) {
                    qDebug() << QString::fromUtf8("检测到组为空，创建空结果");
                    // 组为空是正常情况，创建一个空的查询结果
                    QJsonObject emptyResult;
                    QJsonObject emptyHeader;
                    emptyHeader["code"] = 0;
                    emptyHeader["message"] = "success";
                    emptyResult["header"] = emptyHeader;
                    
                    QJsonObject emptyPayload;
                    QJsonObject emptyQueryRes;
                    emptyQueryRes["text"] = "{\"data\":[]}"; // 空的声纹列表
                    emptyPayload["queryFeatureListRes"] = emptyQueryRes;
                    emptyResult["payload"] = emptyPayload;
                    
                    qDebug() << QString::fromUtf8("发送空的查询结果");
                    emit requestFinished(emptyResult);
                    reply->deleteLater();
                    if (reply == m_currentReply) {
                        m_currentReply = nullptr;
                    }
                    return;
                }
                
                // 其他业务错误
                qDebug() << QString::fromUtf8("业务错误: [%1] %2").arg(code).arg(message);
                emit requestError(QString::fromUtf8("业务错误: [%1] %2").arg(code).arg(message));
                reply->deleteLater();
                if (reply == m_currentReply) {
                    m_currentReply = nullptr;
                }
                return;
            }
        }
        
        // 成功响应
        qDebug() << QString::fromUtf8("声纹识别请求成功，发送结果");
        emit requestFinished(jsonResponse);
        reply->deleteLater();
        if (reply == m_currentReply) {
            m_currentReply = nullptr;
        }
        return;
    }
    
    // 如果JSON解析失败，再检查网络错误
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString::fromUtf8("网络错误: %1").arg(reply->errorString());
        qDebug() << QString::fromUtf8("声纹识别网络错误:") << errorMsg;
        
        // 针对401错误的特殊处理
        if (statusCode == 401) {
            qDebug() << QString::fromUtf8("401未授权错误，可能的原因:");
            qDebug() << QString::fromUtf8("1. API密钥或密钥不正确");
            qDebug() << QString::fromUtf8("2. 签名计算错误");
            qDebug() << QString::fromUtf8("3. 时间戳过期");
            qDebug() << QString::fromUtf8("4. 请求头格式不正确");
        }
        
        emit requestError(errorMsg);
    } else {
        // JSON解析失败但没有网络错误
        QString parseError = QString::fromUtf8("JSON解析失败: ") + error.errorString();
        qDebug() << QString::fromUtf8("声纹识别JSON解析错误:") << parseError;
        emit requestError(parseError);
    }
    
    // 清理
    reply->deleteLater();
    if (reply == m_currentReply) {
        m_currentReply = nullptr;
    }
}

QByteArray VoiceprintRequest::convertPCMToMP3(const QByteArray &pcmData)
{
    // 简单的实现：由于没有LAME库，我们使用一种简化的方法
    // 创建一个最小的MP3头部，然后附加PCM数据
    // 这不是真正的MP3格式，但可能被API接受
    
    if (pcmData.isEmpty()) {
        return QByteArray();
    }
    
    // 创建WAV格式的数据，然后转换为base64
    // 这是一个简化的方法，实际的MP3编码需要专门的库
    QByteArray wavData;
    QDataStream stream(&wavData, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // WAV文件头
    stream.writeRawData("RIFF", 4);
    quint32 fileSize = pcmData.size() + 36;
    stream << fileSize;
    stream.writeRawData("WAVE", 4);
    
    // fmt 子块
    stream.writeRawData("fmt ", 4);
    quint32 fmtSize = 16;
    stream << fmtSize;
    quint16 audioFormat = 1; // PCM
    stream << audioFormat;
    quint16 numChannels = 1; // 单声道
    stream << numChannels;
    quint32 sampleRate = 16000; // 16kHz
    stream << sampleRate;
    quint32 byteRate = sampleRate * numChannels * 2;
    stream << byteRate;
    quint16 blockAlign = numChannels * 2;
    stream << blockAlign;
    quint16 bitsPerSample = 16;
    stream << bitsPerSample;
    
    // data 子块
    stream.writeRawData("data", 4);
    quint32 dataSize = pcmData.size();
    stream << dataSize;
    
    // 添加音频数据
    wavData.append(pcmData);
    
    qDebug() << QString::fromUtf8("创建WAV格式数据，大小:") << wavData.size() << QString::fromUtf8("字节");
    
    // 返回WAV数据作为"MP3"数据
    // 注意：这不是真正的MP3格式，但可能被API接受
    return wavData;
}

QString VoiceprintRequest::encodeAudioData(const QByteArray &audioData)
{
    return QString(audioData.toBase64());
}

bool VoiceprintRequest::saveRawAudioAsWav(const QByteArray &rawData, const QString &filePath)
{
    if (rawData.isEmpty()) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << QString::fromUtf8("无法创建临时WAV文件:") << filePath;
        return false;
    }
    
    // 创建WAV文件头
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // WAV文件头结构
    stream.writeRawData("RIFF", 4);                    // ChunkID
    quint32 fileSize = rawData.size() + 36;
    stream << fileSize;                                 // ChunkSize
    stream.writeRawData("WAVE", 4);                    // Format
    
    // fmt子块
    stream.writeRawData("fmt ", 4);                    // Subchunk1ID
    quint32 subchunk1Size = 16;
    stream << subchunk1Size;                           // Subchunk1Size
    quint16 audioFormat = 1;                           // PCM
    stream << audioFormat;                             // AudioFormat
    quint16 numChannels = 1;                           // 单声道
    stream << numChannels;                             // NumChannels
    quint32 sampleRate = 16000;                        // 16kHz
    stream << sampleRate;                              // SampleRate
    quint32 byteRate = sampleRate * numChannels * 2;  // 16位=2字节
    stream << byteRate;                                // ByteRate
    quint16 blockAlign = numChannels * 2;
    stream << blockAlign;                              // BlockAlign
    quint16 bitsPerSample = 16;
    stream << bitsPerSample;                           // BitsPerSample
    
    // data子块
    stream.writeRawData("data", 4);                    // Subchunk2ID
    quint32 dataSize = rawData.size();
    stream << dataSize;                                // Subchunk2Size
    
    // 写入文件头和音频数据
    file.write(header);
    file.write(rawData);
    file.close();
    
    qDebug() << QString::fromUtf8("创建临时WAV文件:") << filePath << QString::fromUtf8("，大小:") << (header.size() + rawData.size()) << QString::fromUtf8("字节");
    
    return true;
}
