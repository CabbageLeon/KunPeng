#include "VoiceTest.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QDebug>
#include <QApplication>
#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include <QDir>

// 全局变量定义
uint32_t g_audioAddress = 0;         // 32位音频数据地址
uint32_t g_audioDataSize = 0;        // 音频数据大小

VoiceTest::VoiceTest(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_recordButton(nullptr)
    , m_playButton(nullptr)
    , m_stopButton(nullptr)
    , m_statusLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_audioAddress(0)
    , m_audioDataSize(0)
    , m_audioDataOffset(0)
    , m_readTimer(nullptr)
    , m_mediaPlayer(nullptr)
    , m_audioOutput(nullptr)
    , m_isRecording(false)
    , m_isPlaying(false)
{
    setupUI();
    
    // 初始化定时器
    m_readTimer = new QTimer(this);
    connect(m_readTimer, &QTimer::timeout, this, &VoiceTest::onAudioDataRead);
    m_readTimer->setInterval(50); // 50ms读取一次，适合10kHz采样率
    
    // 初始化媒体播放器
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // 检查是否有全局音频数据
    if (g_audioAddress != 0 && g_audioDataSize > 0) {
        setGlobalAudioData();
    }
    
    qDebug() << "VoiceTest初始化完成 (10kHz采样率)";
}

VoiceTest::~VoiceTest()
{
    if (m_isRecording) {
        onStopClicked();
    }
    
    // 清理临时文件
    if (!m_tempAudioFile.isEmpty() && QFile::exists(m_tempAudioFile)) {
        QFile::remove(m_tempAudioFile);
    }
}

void VoiceTest::setupUI()
{
    // 主布局
    m_mainLayout = new QVBoxLayout(this);
    
    // 信息标签
    m_infoLabel = new QLabel("音频测试工具", this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px;");
    
    // 状态标签
    m_statusLabel = new QLabel("状态：就绪", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 12px; color: blue; margin: 5px;");
    
    // 按钮布局
    m_buttonLayout = new QHBoxLayout();
    
    // 录制按钮
    m_recordButton = new QPushButton("开始录制", this);
    m_recordButton->setStyleSheet("QPushButton { padding: 10px; font-size: 14px; }");
    connect(m_recordButton, &QPushButton::clicked, this, &VoiceTest::onRecordClicked);
    
    // 播放按钮
    m_playButton = new QPushButton("播放录音", this);
    m_playButton->setStyleSheet("QPushButton { padding: 10px; font-size: 14px; }");
    m_playButton->setEnabled(false);
    connect(m_playButton, &QPushButton::clicked, this, &VoiceTest::onPlayClicked);
    
    // 停止按钮
    m_stopButton = new QPushButton("停止", this);
    m_stopButton->setStyleSheet("QPushButton { padding: 10px; font-size: 14px; }");
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &VoiceTest::onStopClicked);
    
    // 添加按钮到布局
    m_buttonLayout->addWidget(m_recordButton);
    m_buttonLayout->addWidget(m_playButton);
    m_buttonLayout->addWidget(m_stopButton);
    
    // 添加组件到主布局
    m_mainLayout->addWidget(m_infoLabel);
    m_mainLayout->addWidget(m_statusLabel);
    m_mainLayout->addLayout(m_buttonLayout);
    
    // 设置窗口属性
    setWindowTitle("音频测试工具");
    setFixedSize(400, 200);
}

void VoiceTest::setTestAudioData(uint32_t audioAddress, uint32_t dataSize)
{
    m_audioAddress = audioAddress;
    m_audioDataSize = dataSize;
    m_audioDataOffset = 0;
    
    qDebug() << "设置测试音频数据:";
    qDebug() << "  地址: 0x" << QString::number(audioAddress, 16).toUpper();
    qDebug() << "  大小:" << dataSize << "字节";
    
    if (dataSize > 0) {
        double duration = dataSize / (10000.0 * 2); // 10kHz, 16bit, 单声道
        qDebug() << "  预计时长:" << duration << "秒";
        
        m_infoLabel->setText(QString("音频数据: 0x%1, %2 字节 (约 %3 秒)")
                            .arg(QString::number(audioAddress, 16).toUpper())
                            .arg(dataSize)
                            .arg(duration, 0, 'f', 1));
    }
    
    // 如果有数据，启用录制按钮
    m_recordButton->setEnabled(audioAddress != 0 && dataSize > 0);
}

void VoiceTest::setGlobalAudioData()
{
    setTestAudioData(g_audioAddress, g_audioDataSize);
    qDebug() << "使用全局音频数据: 地址=0x" << QString::number(g_audioAddress, 16).toUpper() 
             << "大小=" << g_audioDataSize;
}

void VoiceTest::onRecordClicked()
{
    if (m_audioAddress == 0 || m_audioDataSize == 0) {
        m_statusLabel->setText("错误：没有可用的音频数据");
        return;
    }
    
    if (m_isRecording) {
        return; // 已在录制中
    }
    
    qDebug() << "开始从内存地址 0x" << QString::number(m_audioAddress, 16).toUpper() << " 读取音频数据";
    
    // 重置状态
    m_audioDataOffset = 0;
    m_recordedData.clear();
    m_isRecording = true;
    
    // 更新UI
    m_recordButton->setText("录制中...");
    m_recordButton->setEnabled(false);
    m_playButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("状态：正在从内存读取音频数据 (10kHz)");
    
    // 开始定时器
    m_readTimer->start();
}

void VoiceTest::onPlayClicked()
{
    if (m_recordedData.isEmpty()) {
        m_statusLabel->setText("错误：没有录制的数据可播放");
        return;
    }
    
    if (m_isPlaying) {
        return; // 已在播放中
    }
    
    qDebug() << "开始播放录制的音频，数据大小:" << m_recordedData.size() << "字节";
    
    // 保存音频到临时文件
    QString tempFile;
    if (!saveAudioToTempFile(m_recordedData, tempFile)) {
        m_statusLabel->setText("错误：无法保存临时音频文件");
        return;
    }
    
    m_tempAudioFile = tempFile;
    m_isPlaying = true;
    
    // 更新UI
    m_playButton->setText("播放中...");
    m_playButton->setEnabled(false);
    m_recordButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("状态：正在播放录音");
    
    // 播放音频
    m_mediaPlayer->setSource(QUrl::fromLocalFile(tempFile));
    m_mediaPlayer->play();
    
    // 连接播放完成信号
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            onStopClicked();
        }
    });
}

void VoiceTest::onStopClicked()
{
    qDebug() << "停止操作";
    
    // 停止录制
    if (m_isRecording) {
        m_readTimer->stop();
        m_isRecording = false;
        qDebug() << "录制完成，总共读取:" << m_recordedData.size() << "字节";
    }
    
    // 停止播放
    if (m_isPlaying) {
        m_mediaPlayer->stop();
        m_isPlaying = false;
    }
    
    // 更新UI
    m_recordButton->setText("开始录制");
    m_recordButton->setEnabled(m_audioAddress != 0 && m_audioDataSize > 0);
    m_playButton->setText("播放录音");
    m_playButton->setEnabled(!m_recordedData.isEmpty());
    m_stopButton->setEnabled(false);
    m_statusLabel->setText("状态：就绪");
}

void VoiceTest::onAudioDataRead()
{
    if (!m_isRecording || m_audioAddress == 0 || m_audioDataOffset >= static_cast<int>(m_audioDataSize)) {
        // 数据读取完毕，自动停止
        if (m_audioDataOffset >= static_cast<int>(m_audioDataSize)) {
            qDebug() << "音频数据读取完毕";
            onStopClicked();
        }
        return;
    }
    
    // 每次读取2000字节 (约100ms的10kHz音频数据)
    const int chunkSize = 2000; // 10kHz * 2字节/样本 * 0.1秒 = 2000字节
    int remainingSize = static_cast<int>(m_audioDataSize) - m_audioDataOffset;
    int readSize = (remainingSize < chunkSize) ? remainingSize : chunkSize;
    
    if (readSize > 0) {
        // 从内存地址读取数据
        const char* dataPtr = reinterpret_cast<const char*>(m_audioAddress + m_audioDataOffset);
        QByteArray chunk(dataPtr, readSize);
        m_recordedData.append(chunk);
        m_audioDataOffset += readSize;
        
        // 更新状态
        double progress = (double)m_audioDataOffset / m_audioDataSize * 100;
        m_statusLabel->setText(QString("录制进度: %1% (%2/%3 字节)")
                              .arg(progress, 0, 'f', 1)
                              .arg(m_audioDataOffset)
                              .arg(m_audioDataSize));
        
        // qDebug() << "读取音频数据:" << readSize << "字节，偏移:" << m_audioDataOffset;
    }
}

void VoiceTest::updateStatus()
{
    // 定期更新状态信息
    if (m_isRecording) {
        double progress = (double)m_audioDataOffset / m_audioDataSize * 100;
        m_statusLabel->setText(QString("录制进度: %1%").arg(progress, 0, 'f', 1));
    }
}

bool VoiceTest::saveAudioToTempFile(const QByteArray& audioData, QString& filePath)
{
    if (audioData.isEmpty()) {
        return false;
    }
    
    // 生成临时文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    filePath = QApplication::applicationDirPath() + QString("/voice_test_%1.wav").arg(timestamp);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法创建临时音频文件:" << filePath;
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
    quint32 sampleRate = 10000;                        // 10kHz
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
    
    qDebug() << "保存临时音频文件:" << filePath << "大小:" << (header.size() + audioData.size()) << "字节";
    return true;
}

#include "VoiceTest.moc"