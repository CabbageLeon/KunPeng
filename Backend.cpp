#include "Backend.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>

Backend::Backend(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_leftLayout(nullptr)
    , m_rightLayout(nullptr)
    , m_list(nullptr)
    , m_btnRecord(nullptr)
    , m_btnQuery(nullptr)
    , m_btnAdd(nullptr)
    , m_btnTest(nullptr)
    , m_featureIdEdit(nullptr)
    , m_visitorNameEdit(nullptr)
    , m_btnRegister(nullptr)
    , m_status(nullptr)
    , m_testResult(nullptr)
    , m_visitorDisplay(nullptr)
    , m_btnClearVisitor(nullptr)
    , m_audioSource(nullptr)
    , m_recordTimer(nullptr)
    , m_testTimer(nullptr)
    , m_isRecording(false)
    , m_isTesting(false)
    , m_isProcessingRequest(false)
    , m_api(nullptr)
    , m_groupName("student")
{
    setupUI();
    setupAudio();
    setupApi();
    loadVisitorFromFile(); // 加载来访者信息
}

Backend::~Backend()
{
    if (m_isRecording) {
        stopRecording();
    }
    if (m_isTesting) {
        stopTesting();
    }
}

void Backend::setupUI()
{
    setWindowTitle(QString::fromUtf8("声纹管理"));
    m_mainLayout = new QHBoxLayout(this);
    
    // 左侧：列表
    m_leftLayout = new QVBoxLayout();
    QLabel *title = new QLabel(QString::fromUtf8("已有声纹列表"), this);
    title->setAlignment(Qt::AlignCenter);
    m_list = new QListWidget(this);
    m_leftLayout->addWidget(title);
    m_leftLayout->addWidget(m_list, 1);

    // 右侧：按钮+输入框+状态
    m_rightLayout = new QVBoxLayout();
    m_btnRecord = new QPushButton(QString::fromUtf8("录制"), this);
    m_btnQuery = new QPushButton(QString::fromUtf8("查询"), this);
    
    // 添加"添加声纹"按钮
    m_btnAdd = new QPushButton(QString::fromUtf8("添加声纹"), this);
    m_btnAdd->setEnabled(false); // 初始状态禁用，录制完成后启用
    
    // 添加"实时测试"按钮
    m_btnTest = new QPushButton(QString::fromUtf8("实时测试"), this);
    
    m_featureIdEdit = new QLineEdit(this);
    m_featureIdEdit->setPlaceholderText(QString::fromUtf8("请输入 FeatureID"));
    
    // 来访者登记区域
    m_visitorNameEdit = new QLineEdit(this);
    m_visitorNameEdit->setPlaceholderText(QString::fromUtf8("请输入来访者姓名"));
    m_btnRegister = new QPushButton(QString::fromUtf8("登记来访者"), this);
    
    // 来访者显示标签
    m_visitorDisplay = new QLabel(QString::fromUtf8("当前来访者: 无"), this);
    m_visitorDisplay->setAlignment(Qt::AlignCenter);
    m_visitorDisplay->setStyleSheet("QLabel { background-color: #e8f5e8; border: 1px solid #4CAF50; padding: 10px; margin: 5px; font-weight: bold; }");
    
    // 清除来访者按钮
    m_btnClearVisitor = new QPushButton(QString::fromUtf8("清除来访者"), this);
    m_btnClearVisitor->setEnabled(false); // 初始状态禁用
    
    m_status = new QLabel(QString::fromUtf8("就绪 - 组名: student"), this);
    m_status->setAlignment(Qt::AlignCenter);
    
    // 测试结果显示标签
    m_testResult = new QLabel(QString::fromUtf8("测试结果将在这里显示"), this);
    m_testResult->setAlignment(Qt::AlignCenter);
    m_testResult->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid #ccc; padding: 10px; margin: 5px; }");
    m_testResult->setWordWrap(true);
    m_testResult->setMinimumHeight(80);

    m_rightLayout->addWidget(m_btnRecord);
    m_rightLayout->addWidget(m_btnAdd);
    m_rightLayout->addWidget(m_featureIdEdit);
    m_rightLayout->addWidget(m_btnQuery);
    m_rightLayout->addWidget(m_btnTest);
    m_rightLayout->addWidget(m_testResult);
    
    // 添加来访者登记区域
    m_rightLayout->addWidget(m_visitorNameEdit);
    m_rightLayout->addWidget(m_btnRegister);
    m_rightLayout->addWidget(m_visitorDisplay);
    m_rightLayout->addWidget(m_btnClearVisitor);
    
    m_rightLayout->addStretch();
    m_rightLayout->addWidget(m_status);

    m_mainLayout->addLayout(m_leftLayout, 2);
    m_mainLayout->addLayout(m_rightLayout, 1);

    connect(m_btnRecord, &QPushButton::clicked, this, &Backend::onRecordClicked);
    connect(m_btnQuery, &QPushButton::clicked, this, &Backend::onQueryClicked);
    connect(m_btnAdd, &QPushButton::clicked, this, &Backend::onAddClicked);
    connect(m_btnTest, &QPushButton::clicked, this, &Backend::onTestClicked);
    connect(m_btnRegister, &QPushButton::clicked, this, &Backend::onRegisterClicked);
    connect(m_btnClearVisitor, &QPushButton::clicked, this, &Backend::onClearVisitorClicked);
}

void Backend::setupAudio()
{
    m_audioFormat.setSampleRate(16000);
    m_audioFormat.setChannelCount(1);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);
    m_audioDevice = QMediaDevices::defaultAudioInput();
    if (m_audioDevice.isNull()) {
        updateStatus(QString::fromUtf8("错误: 未找到录音设备"));
        if (m_btnRecord) m_btnRecord->setEnabled(false);
        return;
    }
    m_audioSource = new QAudioSource(m_audioDevice, m_audioFormat, this);
    m_audioSource->setBufferSize(16000 * 2); // 约1秒缓冲
    m_recordTimer = new QTimer(this);
    m_recordTimer->setSingleShot(true);
    connect(m_recordTimer, &QTimer::timeout, this, &Backend::onRecordingTimeout);
    
    // 测试定时器：每2秒进行一次识别
    m_testTimer = new QTimer(this);
    connect(m_testTimer, &QTimer::timeout, this, &Backend::onTestingTimeout);
}

void Backend::setupApi()
{
    m_api = new VoiceprintRequest(this);
    // TODO: 在此处设置你的AppId/ApiKey/ApiSecret
    m_api->setCredentials("581ffbe4", "c43e133e41862c3aa2495bae6c2268ef", "OTgxYWRlNDdiYzFmZTBhNDRhM2NlYTE1");
    connect(m_api, &VoiceprintRequest::requestFinished, this, &Backend::onApiFinished);
    connect(m_api, &VoiceprintRequest::requestError, this, &Backend::onApiError);
}

void Backend::updateStatus(const QString &text)
{
    if (m_status) m_status->setText(text);
    qDebug() << text;
}

void Backend::onRecordClicked()
{
    if (m_isRecording) {
        stopRecording();
    } else {
        startRecording();
    }
}

void Backend::onQueryClicked()
{
    if (!m_api) return;
    updateStatus(QString::fromUtf8("准备查询声纹列表 - 先确保组存在..."));
    // 先创建组，如果组已存在会返回相应的信息，然后再查询列表
    m_api->createGroup(m_groupName, QString::fromUtf8("学生特征库"), QString::fromUtf8("用于学生声纹识别的特征库"));
}

void Backend::onAddClicked()
{
    if (!m_api) return;
    
    QString featureId = m_featureIdEdit->text().trimmed();
    if (featureId.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("请输入声纹ID"));
        return;
    }
    
    if (m_audioBuffer.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("警告"), QString::fromUtf8("请先录制音频"));
        return;
    }
    
    // 保存音频为临时文件
    QString tempAudioPath = QApplication::applicationDirPath() + "/temp_voiceprint.wav";
    if (!saveAudioAsWav(m_audioBuffer, tempAudioPath)) {
        updateStatus(QString::fromUtf8("错误: 音频保存失败"));
        return;
    }
    
    updateStatus(QString::fromUtf8("正在添加声纹: %1").arg(featureId));
    m_api->createFeature(tempAudioPath, m_groupName, featureId, QString::fromUtf8("学生声纹-%1").arg(featureId));
    
    // 清空输入框和音频缓冲区
    m_featureIdEdit->clear();
    m_audioBuffer.clear();
    if (m_btnAdd) m_btnAdd->setEnabled(false);
}

void Backend::onDeleteClicked()
{
    QPushButton *deleteBtn = qobject_cast<QPushButton*>(sender());
    if (!deleteBtn || !m_api) return;
    
    QString featureId = deleteBtn->property("featureId").toString();
    if (featureId.isEmpty()) return;
    
    int ret = QMessageBox::question(this, 
        QString::fromUtf8("确认删除"), 
        QString::fromUtf8("确定要删除声纹 '%1' 吗？").arg(featureId),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        updateStatus(QString::fromUtf8("正在删除声纹: %1").arg(featureId));
        m_api->deleteFeature(m_groupName, featureId);
    }
}

void Backend::onTestClicked()
{
    if (m_isTesting) {
        stopTesting();
    } else {
        startTesting();
    }
}

void Backend::onRegisterClicked()
{
    if (!m_visitorNameEdit || !m_visitorDisplay) return;
    
    QString visitorName = m_visitorNameEdit->text().trimmed();
    if (visitorName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请输入来访者姓名"));
        return;
    }
    
    // 保存到文件
    saveVisitorToFile(visitorName);
    
    // 登记来访者
    QString displayText = QString::fromUtf8("当前来访者: %1").arg(visitorName);
    m_visitorDisplay->setText(displayText);
    
    // 清空输入框并启用清除按钮
    m_visitorNameEdit->clear();
    if (m_btnClearVisitor) {
        m_btnClearVisitor->setEnabled(true);
    }
    
    // 显示登记成功提示
    updateStatus(QString::fromUtf8("来访者 \"%1\" 登记成功").arg(visitorName));
    
    qDebug() << QString::fromUtf8("来访者登记成功:") << visitorName;
}

void Backend::onClearVisitorClicked()
{
    if (!m_visitorDisplay || !m_btnClearVisitor) return;
    
    // 清空文件
    clearVisitorFile();
    
    // 重置标签
    m_visitorDisplay->setText(QString::fromUtf8("当前来访者: 无"));
    
    // 禁用清除按钮
    m_btnClearVisitor->setEnabled(false);
    
    // 更新状态
    updateStatus(QString::fromUtf8("来访者信息已清除"));
    
    qDebug() << QString::fromUtf8("来访者信息已清除");
}

void Backend::startRecording()
{
    if (!m_audioSource) return;
    m_audioBuffer.clear();
    QIODevice *dev = m_audioSource->start();
    if (!dev) {
        updateStatus(QString::fromUtf8("启动录音失败"));
        return;
    }
    m_audioIo = dev;
    connect(m_audioIo, &QIODevice::readyRead, this, &Backend::onAudioReady);
    m_isRecording = true;
    if (m_btnRecord) m_btnRecord->setText(QString::fromUtf8("停止"));
    updateStatus(QString::fromUtf8("正在录音...(10秒)"));
    if (m_recordTimer) m_recordTimer->start(10000);
}

void Backend::stopRecording()
{
    if (!m_isRecording) return;
    m_isRecording = false;
    if (m_recordTimer && m_recordTimer->isActive()) m_recordTimer->stop();
    if (m_audioIo) {
        disconnect(m_audioIo, &QIODevice::readyRead, this, &Backend::onAudioReady);
    }
    if (m_audioSource) m_audioSource->stop();
    m_audioIo.clear();
    if (m_btnRecord) m_btnRecord->setText(QString::fromUtf8("录制"));
    
    // 启用"添加声纹"按钮（如果有音频数据）
    if (!m_audioBuffer.isEmpty() && m_btnAdd) {
        m_btnAdd->setEnabled(true);
    }
    
    updateStatus(QString::fromUtf8("录音完成: %1 字节，可以添加声纹").arg(m_audioBuffer.size()));
}

void Backend::startTesting()
{
    if (!m_audioSource || m_isRecording) return;
    
    m_testBuffer.clear();
    QIODevice *dev = m_audioSource->start();
    if (!dev) {
        updateStatus(QString::fromUtf8("启动实时测试失败"));
        return;
    }
    
    m_audioIo = dev;
    connect(m_audioIo, &QIODevice::readyRead, this, [this]() {
        if (!m_isTesting || !m_audioIo) return;
        QByteArray chunk = m_audioIo->readAll();
        if (!chunk.isEmpty()) {
            m_testBuffer.append(chunk);
            // 保持最近5秒的音频数据，这样我们总能提取出3秒
            const int maxBufferSize = 16000 * 2 * 5; // 5秒 * 16kHz * 2字节
            if (m_testBuffer.size() > maxBufferSize) {
                m_testBuffer = m_testBuffer.right(maxBufferSize);
            }
        }
    });
    
    m_isTesting = true;
    if (m_btnTest) m_btnTest->setText(QString::fromUtf8("停止测试"));
    if (m_testResult) m_testResult->setText(QString::fromUtf8("正在监听..."));
    updateStatus(QString::fromUtf8("实时测试中..."));
    
    // 启动定时器，每2秒进行一次识别
    if (m_testTimer) m_testTimer->start(2000);
}

void Backend::stopTesting()
{
    if (!m_isTesting) return;
    m_isTesting = false;
    
    if (m_testTimer && m_testTimer->isActive()) m_testTimer->stop();
    if (m_audioIo) {
        disconnect(m_audioIo, nullptr, this, nullptr);
    }
    if (m_audioSource) m_audioSource->stop();
    m_audioIo.clear();
    
    if (m_btnTest) m_btnTest->setText(QString::fromUtf8("实时测试"));
    if (m_testResult) m_testResult->setText(QString::fromUtf8("测试已停止"));
    updateStatus(QString::fromUtf8("实时测试已停止"));
    
    // 清理所有临时文件
    QDir appDir(QApplication::applicationDirPath());
    QStringList filters;
    filters << "temp_realtime_*.wav";
    
    QFileInfoList files = appDir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fileInfo : files) {
        QFile::remove(fileInfo.absoluteFilePath());
        qDebug() << QString::fromUtf8("清理临时文件:") << fileInfo.fileName();
    }
}

void Backend::onTestingTimeout()
{
    // 如果正在处理请求，跳过本次识别
    if (m_isProcessingRequest) {
        qDebug() << QString::fromUtf8("正在处理上一个请求，跳过本次识别");
        return;
    }
    
    // 声纹识别需要至少3秒的高质量音频数据
    const int requiredSize = 16000 * 2 * 3; // 3秒的16kHz 16bit单声道数据
    
    if (!m_isTesting || !m_api || m_testBuffer.size() < requiredSize) {
        if (m_testBuffer.size() > 0) {
            qDebug() << QString::fromUtf8("音频数据不足，当前:") << m_testBuffer.size() 
                     << QString::fromUtf8("字节，需要至少:") << requiredSize << QString::fromUtf8("字节");
        }
        return;
    }
    
    // 设置请求状态
    m_isProcessingRequest = true;
    
    // 取最后3秒的音频数据，确保是连续的
    QByteArray audioForTest = m_testBuffer.right(requiredSize);
    
    // 检查音频数据是否有声音（简单的音量检测）
    if (!hasAudioContent(audioForTest)) {
        qDebug() << QString::fromUtf8("音频数据太安静，跳过识别");
        m_isProcessingRequest = false;
        return;
    }
    
    qDebug() << QString::fromUtf8("准备保存3秒音频数据，大小:") << audioForTest.size() << QString::fromUtf8("字节");
    
    // 使用时间戳创建唯一的临时文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString tempAudioPath = QApplication::applicationDirPath() + QString("/temp_realtime_%1.wav").arg(timestamp);
    
    if (!saveAudioAsWav(audioForTest, tempAudioPath)) {
        qDebug() << QString::fromUtf8("保存临时音频文件失败");
        m_isProcessingRequest = false; // 重置状态
        return;
    }
    
    qDebug() << QString::fromUtf8("音频文件已保存:") << tempAudioPath;
    qDebug() << QString::fromUtf8("开始声纹识别...");
    
    // 使用文件进行识别
    m_api->searchFeature(tempAudioPath, m_groupName, 3);
    
    // 延迟删除之前的临时文件（保留最近的几个文件）
    cleanupOldTempFiles();
}

void Backend::onAudioReady()
{
    if (!m_isRecording || !m_audioIo) return;
    QByteArray chunk = m_audioIo->readAll();
    if (!chunk.isEmpty()) m_audioBuffer.append(chunk);
}

void Backend::onRecordingTimeout()
{
    stopRecording();
}

void Backend::populateListFromQueryPayload(const QJsonObject &payload)
{
    if (!m_list) return;
    m_list->clear();
    if (!payload.contains("queryFeatureListRes")) return;
    
    QJsonObject res = payload.value("queryFeatureListRes").toObject();
    const QString text = res.value("text").toString();
    if (text.isEmpty()) return;
    
    // 解码Base64编码的JSON数据
    QByteArray decodedData = QByteArray::fromBase64(text.toUtf8());
    QJsonDocument doc = QJsonDocument::fromJson(decodedData);
    
    // 如果解码失败，尝试直接解析原始文本
    if (doc.isNull()) {
        doc = QJsonDocument::fromJson(text.toUtf8());
    }
    
    if (doc.isNull()) return;
    
    // 检查是否是数组格式 [{"featureId":...}] 还是对象格式 {"data":[...]}
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("data") && obj.value("data").isArray()) {
            arr = obj.value("data").toArray();
        }
    }
    
    for (const QJsonValue &v : arr) {
        const QJsonObject item = v.toObject();
        const QString fid = item.value("featureId").toString();
        const QString finfo = item.value("featureInfo").toString();
        
        // 创建自定义的列表项
        QListWidgetItem *listItem = new QListWidgetItem(m_list);
        QWidget *itemWidget = new QWidget();
        QHBoxLayout *itemLayout = new QHBoxLayout(itemWidget);
        
        // 声纹信息标签
        QLabel *infoLabel = new QLabel(QString("%1 - %2").arg(fid, finfo));
        infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        
        // 删除按钮
        QPushButton *deleteBtn = new QPushButton(QString::fromUtf8("删除"));
        deleteBtn->setMaximumWidth(60);
        deleteBtn->setProperty("featureId", fid);  // 存储featureId用于删除
        connect(deleteBtn, &QPushButton::clicked, this, &Backend::onDeleteClicked);
        
        itemLayout->addWidget(infoLabel);
        itemLayout->addWidget(deleteBtn);
        itemLayout->setContentsMargins(5, 2, 5, 2);
        
        listItem->setSizeHint(itemWidget->sizeHint());
        m_list->setItemWidget(listItem, itemWidget);
    }
}

void Backend::onApiFinished(const QJsonObject &result)
{
    // 重置请求状态
    m_isProcessingRequest = false;
    
    qDebug() << QString::fromUtf8("onApiFinished被调用，API响应:") << QJsonDocument(result).toJson(QJsonDocument::Compact);
    
    if (!result.contains("payload")) {
        updateStatus(QString::fromUtf8("操作成功"));
        return;
    }
    
    const QJsonObject payload = result.value("payload").toObject();
    
    if (payload.contains("createGroupRes")) {
        // 创建组完成，现在查询声纹列表
        updateStatus(QString::fromUtf8("组创建成功，正在查询声纹列表..."));
        if (m_api) {
            m_api->queryFeatureList(m_groupName);
        }
    } else if (payload.contains("createFeatureRes")) {
        // 添加声纹完成，刷新列表
        updateStatus(QString::fromUtf8("声纹添加成功！正在刷新列表..."));
        if (m_api) {
            m_api->queryFeatureList(m_groupName);
        }
    } else if (payload.contains("deleteFeatureRes")) {
        // 删除声纹完成，刷新列表
        updateStatus(QString::fromUtf8("声纹删除成功！正在刷新列表..."));
        if (m_api) {
            m_api->queryFeatureList(m_groupName);
        }
    } else if (payload.contains("queryFeatureListRes")) {
        populateListFromQueryPayload(payload);
        updateStatus(QString::fromUtf8("查询完成"));
    } else if (payload.contains("searchFeaRes") || payload.contains("searchScoreFeaRes")) {
        // 处理声纹识别结果
        QJsonObject searchRes;
        if (payload.contains("searchFeaRes")) {
            searchRes = payload.value("searchFeaRes").toObject();
        } else {
            searchRes = payload.value("searchScoreFeaRes").toObject();
        }
        
        const QString text = searchRes.value("text").toString();
        if (!text.isEmpty()) {
            // 解码Base64编码的识别结果
            QByteArray decodedData = QByteArray::fromBase64(text.toUtf8());
            QJsonDocument doc = QJsonDocument::fromJson(decodedData);
            
            if (doc.isNull()) {
                doc = QJsonDocument::fromJson(text.toUtf8());
            }
            
            if (!doc.isNull()) {
                displaySearchResult(doc);
            }
        }
    } else {
        updateStatus(QString::fromUtf8("操作完成"));
    }
}

void Backend::onApiError(const QString &error)
{
    // 重置请求状态
    m_isProcessingRequest = false;
    
    qDebug() << QString::fromUtf8("onApiError被调用，API错误:") << error;
    
    // 如果是组已存在的错误，尝试查询声纹列表
    if (error.contains("group already exists") || error.contains("组已存在")) {
        updateStatus(QString::fromUtf8("组已存在，正在查询声纹列表..."));
        if (m_api) {
            m_api->queryFeatureList(m_groupName);
        }
        return;
    }
    
    QMessageBox::warning(this, QString::fromUtf8("错误"), error);
    updateStatus(QString::fromUtf8("错误: ") + error);
    
    // 发射错误信号供Home界面使用
    emit voiceprintRecognitionError(error);
}

bool Backend::saveAudioAsWav(const QByteArray &audioData, const QString &filePath)
{
    if (audioData.isEmpty()) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    // 创建WAV文件头
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // WAV文件头结构
    stream.writeRawData("RIFF", 4);                    // ChunkID
    quint32 fileSize = audioData.size() + 36;
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
    quint32 dataSize = audioData.size();
    stream << dataSize;                                // Subchunk2Size
    
    // 写入文件头和音频数据
    file.write(header);
    file.write(audioData);
    file.close();
    
    return true;
}

void Backend::displaySearchResult(const QJsonDocument &doc)
{
    if (!m_testResult) {
        qDebug() << QString::fromUtf8("警告: m_testResult为空！");
        return;
    }
    
    qDebug() << QString::fromUtf8("displaySearchResult被调用，数据:") << doc.toJson(QJsonDocument::Compact);
    
    QString resultText = QString::fromUtf8("识别结果: ");
    
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        if (arr.isEmpty()) {
            resultText += QString::fromUtf8("未识别");
        } else {
            const QJsonObject item = arr[0].toObject();
            const QString featureId = item.value("featureId").toString();
            const double score = item.value("score").toDouble();
            resultText += QString::fromUtf8("%1 (%2)").arg(featureId).arg(score, 0, 'f', 2);
        }
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("scoreList") && obj.value("scoreList").isArray()) {
            // 处理 {"scoreList": []} 格式 - 这是iFlytek API的实际返回格式
            QJsonArray scoreList = obj.value("scoreList").toArray();
            if (scoreList.isEmpty()) {
                resultText += QString::fromUtf8("未识别");
            } else {
                const QJsonObject item = scoreList[0].toObject();
                const QString featureId = item.value("featureId").toString();
                const double score = item.value("score").toDouble();
                resultText += QString::fromUtf8("%1 (%2)").arg(featureId).arg(score, 0, 'f', 2);
            }
        } else if (obj.contains("data") && obj.value("data").isArray()) {
            // 处理 {"data": []} 格式
            displaySearchResult(QJsonDocument(obj.value("data").toArray()));
            return;
        } else {
            // 处理单个结果
            const QString featureId = obj.value("featureId").toString();
            const double score = obj.value("score").toDouble();
            
            if (featureId.isEmpty()) {
                resultText += QString::fromUtf8("未识别");
            } else {
                resultText += QString::fromUtf8("%1 (%2)").arg(featureId).arg(score, 0, 'f', 2);
            }
        }
    } else {
        resultText += QString::fromUtf8("格式错误");
    }
    
    qDebug() << QString::fromUtf8("设置标签文本:") << resultText;
    m_testResult->setText(resultText);
    m_testResult->update(); // 强制刷新标签
    qDebug() << QString::fromUtf8("标签文本设置完成，当前文本:") << m_testResult->text();
    
    // 发射信号供Home界面使用
    emit voiceprintRecognitionResult(resultText);
}

void Backend::cleanupOldTempFiles()
{
    // 清理5秒前的临时音频文件，保留最近的文件避免并发问题
    QDir appDir(QApplication::applicationDirPath());
    QStringList filters;
    filters << "temp_realtime_*.wav";
    
    QFileInfoList files = appDir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    // 保留最新的3个文件，删除其他的
    for (int i = 3; i < files.size(); ++i) {
        const QFileInfo &fileInfo = files[i];
        // 只删除5秒前的文件
        if (fileInfo.lastModified().secsTo(QDateTime::currentDateTime()) > 5) {
            QFile::remove(fileInfo.absoluteFilePath());
            qDebug() << QString::fromUtf8("清理旧临时文件:") << fileInfo.fileName();
        }
    }
}

bool Backend::hasAudioContent(const QByteArray& audioData)
{
    // 简单的音量检测：检查是否有足够的音频内容
    const qint16* samples = reinterpret_cast<const qint16*>(audioData.constData());
    int sampleCount = audioData.size() / sizeof(qint16);
    
    long long totalEnergy = 0;
    int significantSamples = 0;
    
    for (int i = 0; i < sampleCount; ++i) {
        qint16 sample = samples[i];
        int amplitude = qAbs(sample);
        totalEnergy += amplitude;
        
        // 认为绝对值大于1000的样本是有意义的音频内容
        if (amplitude > 1000) {
            significantSamples++;
        }
    }
    
    double averageEnergy = static_cast<double>(totalEnergy) / sampleCount;
    double significantRatio = static_cast<double>(significantSamples) / sampleCount;
    
    // 调整阈值：从日志看到平均能量96.58、有效样本0.40%的音频能成功识别
    bool hasContent = (averageEnergy > 50.0 && significantRatio > 0.001);
    
    qDebug() << QString::fromUtf8("音频质量检测 - 平均能量:") << averageEnergy 
             << QString::fromUtf8("有效样本比例:") << (significantRatio * 100) << "%"
             << QString::fromUtf8("质量判定:") << (hasContent ? "通过" : "不通过");
    
    return hasContent;
}

void Backend::loadVisitorFromFile()
{
    QString filePath = QApplication::applicationDirPath() + "/../material/visitor.txt";
    QFile file(filePath);
    
    if (!file.exists()) {
        qDebug() << QString::fromUtf8("来访者文件不存在:") << filePath;
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << QString::fromUtf8("无法打开来访者文件:") << filePath;
        return;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString visitorName = in.readLine().trimmed();
    file.close();
    
    if (!visitorName.isEmpty() && m_visitorDisplay && m_btnClearVisitor) {
        m_visitorDisplay->setText(QString::fromUtf8("当前来访者: %1").arg(visitorName));
        m_btnClearVisitor->setEnabled(true);
        qDebug() << QString::fromUtf8("从文件加载来访者:") << visitorName;
    }
}

void Backend::saveVisitorToFile(const QString &visitorName)
{
    QString dirPath = QApplication::applicationDirPath() + "/../material";
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
        qDebug() << QString::fromUtf8("创建目录:") << dirPath;
    }
    
    QString filePath = dirPath + "/visitor.txt";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << QString::fromUtf8("无法创建来访者文件:") << filePath;
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << visitorName << Qt::endl;
    file.close();
    
    qDebug() << QString::fromUtf8("来访者信息已保存到文件:") << visitorName;
}

void Backend::clearVisitorFile()
{
    QString filePath = QApplication::applicationDirPath() + "/../material/visitor.txt";
    QFile file(filePath);
    
    if (file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.resize(0); // 清空文件内容
            file.close();
            qDebug() << QString::fromUtf8("来访者文件已清空");
        } else {
            qDebug() << QString::fromUtf8("无法清空来访者文件:") << filePath;
        }
    }
}

void Backend::performVoiceprintRecognition(const QByteArray &audioData)
{
    if (!m_api) {
        emit voiceprintRecognitionError("声纹识别API未初始化");
        return;
    }
    
    if (audioData.isEmpty()) {
        emit voiceprintRecognitionError("音频数据为空");
        return;
    }
    
    // 检查音频质量
    if (!hasAudioContent(audioData)) {
        emit voiceprintRecognitionError("音频质量不够，跳过识别");
        return;
    }
    
    // 使用Backend默认的组名进行声纹识别
    m_api->searchFeatureWithData(audioData, m_groupName, 1);
}


