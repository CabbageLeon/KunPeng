#include "TtsTest.h"
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

TtsTest::TtsTest(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_textEdit(nullptr)
    , m_playButton(nullptr)
    , m_statusLabel(nullptr)
    , m_voiceGenerator(nullptr)
    , m_mediaPlayer(nullptr)
    , m_audioOutput(nullptr)
{
    setupUI();
    connectSignals();
    
    // 创建语音合成器实例
    m_voiceGenerator = new VoiceGenerator(this);
    
    // 创建音频播放器
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer = new QMediaPlayer(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // 连接媒体播放器信号
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &TtsTest::onMediaPlayerStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &TtsTest::onMediaPlayerError);
    
    // 设置API认证信息（使用与其他类相同的认证）
    m_voiceGenerator->setCredentials("581ffbe4", "c43e133e41862c3aa2495bae6c2268ef", "OTgxYWRlNDdiYzFmZTBhNDRhM2NlYTE1");
    
    // 设置音色（可选）
    m_voiceGenerator->setVoice("x4_yezi"); // 叶子音色
    
    // 连接TTS信号
    connect(m_voiceGenerator, &VoiceGenerator::synthesisStarted,
            this, &TtsTest::onTtsSynthesisStarted);
    connect(m_voiceGenerator, &VoiceGenerator::synthesisFinished,
            this, &TtsTest::onTtsSynthesisFinished);
    connect(m_voiceGenerator, &VoiceGenerator::synthesisError,
            this, &TtsTest::onTtsSynthesisError);
}

TtsTest::~TtsTest()
{
    // Qt会自动清理所有以this为parent的对象
}

void TtsTest::setupUI()
{
    setWindowTitle("语音合成测试");
    setMinimumSize(600, 150);
    
    // 设置窗口样式
    setStyleSheet("QWidget { background-color: rgb(240, 240, 240); }");
    
    // 创建主布局（垂直布局）
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 创建水平布局用于文本输入和按钮
    m_mainLayout = new QHBoxLayout();
    m_mainLayout->setSpacing(10);
    
    // 创建文本输入框
    m_textEdit = new QLineEdit(this);
    m_textEdit->setPlaceholderText("请输入要合成的文本...");
    m_textEdit->setText("你好，这是一个语音合成测试。");
    m_textEdit->setStyleSheet(
        "QLineEdit {"
        "    font-size: 14px;"
        "    padding: 10px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 8px;"
        "    background-color: white;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #3498db;"
        "}"
    );
    m_textEdit->setMinimumHeight(40);
    
    // 创建播放按钮
    m_playButton = new QPushButton("播放", this);
    m_playButton->setStyleSheet(
        "QPushButton {"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    color: white;"
        "    background-color: #27ae60;"
        "    padding: 10px 20px;"
        "    border: none;"
        "    border-radius: 8px;"
        "    min-width: 80px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2ecc71;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #229954;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #95a5a6;"
        "}"
    );
    m_playButton->setMinimumHeight(40);
    
    // 将组件添加到水平布局
    m_mainLayout->addWidget(m_textEdit, 1); // 文本框占据大部分空间
    m_mainLayout->addWidget(m_playButton, 0); // 按钮固定宽度
    
    // 创建状态标签
    m_statusLabel = new QLabel("准备就绪", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 12px;"
        "    color: #7f8c8d;"
        "    background-color: rgba(255, 255, 255, 150);"
        "    padding: 8px;"
        "    border-radius: 6px;"
        "    border: 1px solid #bdc3c7;"
        "}"
    );
    
    // 将布局添加到主布局
    mainLayout->addLayout(m_mainLayout);
    mainLayout->addWidget(m_statusLabel);
}

void TtsTest::connectSignals()
{
    // 连接播放按钮点击信号
    connect(m_playButton, &QPushButton::clicked,
            this, &TtsTest::onPlayButtonClicked);
    
    // 连接回车键触发播放
    connect(m_textEdit, &QLineEdit::returnPressed,
            this, &TtsTest::onPlayButtonClicked);
}

void TtsTest::onPlayButtonClicked()
{
    QString text = m_textEdit->text().trimmed();
    
    if (text.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入要合成的文本！");
        m_textEdit->setFocus();
        return;
    }
    
    if (m_voiceGenerator->isSynthesizing()) {
        qDebug() << "正在合成中，忽略重复请求";
        return;
    }
    
    qDebug() << "开始合成文本:" << text;
    
    // 禁用按钮防止重复点击
    m_playButton->setEnabled(false);
    m_playButton->setText("合成中...");
    
    // 开始语音合成
    m_voiceGenerator->synthesizeText(text);
}

void TtsTest::onTtsSynthesisStarted()
{
    qDebug() << "TTS合成开始";
    m_statusLabel->setText("正在合成语音...");
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 12px;"
        "    color: #2980b9;"
        "    background-color: rgba(52, 152, 219, 20);"
        "    padding: 8px;"
        "    border-radius: 6px;"
        "    border: 1px solid #3498db;"
        "}"
    );
}

void TtsTest::onTtsSynthesisFinished(const QString &audioFilePath)
{
    qDebug() << "TTS合成完成，音频文件:" << audioFilePath;
    
    // 恢复按钮状态
    m_playButton->setEnabled(true);
    m_playButton->setText("播放");
    
    // 更新状态
    QFileInfo fileInfo(audioFilePath);
    m_statusLabel->setText(QString("合成完成，正在播放：%1")
                          .arg(fileInfo.fileName()));
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 12px;"
        "    color: #27ae60;"
        "    background-color: rgba(39, 174, 96, 20);"
        "    padding: 8px;"
        "    border-radius: 6px;"
        "    border: 1px solid #2ecc71;"
        "}"
    );
    
    // 直接播放音频
    QUrl audioUrl = QUrl::fromLocalFile(audioFilePath);
    m_mediaPlayer->setSource(audioUrl);
    m_mediaPlayer->play();
    
    qDebug() << "开始播放音频文件:" << audioUrl.toString();
}

void TtsTest::onTtsSynthesisError(const QString &error)
{
    qDebug() << "TTS合成错误:" << error;
    
    // 恢复按钮状态
    m_playButton->setEnabled(true);
    m_playButton->setText("播放");
    
    // 更新状态
    m_statusLabel->setText(QString("合成失败：%1").arg(error));
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 12px;"
        "    color: #e74c3c;"
        "    background-color: rgba(231, 76, 60, 20);"
        "    padding: 8px;"
        "    border-radius: 6px;"
        "    border: 1px solid #c0392b;"
        "}"
    );
    
    // 显示错误消息
    QMessageBox::critical(this, "错误", QString("语音合成失败：\n\n%1").arg(error));
}

void TtsTest::onMediaPlayerStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
    case QMediaPlayer::PlayingState:
        qDebug() << "音频开始播放";
        m_statusLabel->setText("正在播放音频...");
        m_statusLabel->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"
            "    color: #8e44ad;"
            "    background-color: rgba(142, 68, 173, 20);"
            "    padding: 8px;"
            "    border-radius: 6px;"
            "    border: 1px solid #9b59b6;"
            "}"
        );
        break;
    case QMediaPlayer::PausedState:
        qDebug() << "音频暂停";
        break;
    case QMediaPlayer::StoppedState:
        qDebug() << "音频播放完成";
        m_statusLabel->setText("播放完成");
        m_statusLabel->setStyleSheet(
            "QLabel {"
            "    font-size: 12px;"
            "    color: #27ae60;"
            "    background-color: rgba(39, 174, 96, 20);"
            "    padding: 8px;"
            "    border-radius: 6px;"
            "    border: 1px solid #2ecc71;"
            "}"
        );
        break;
    }
}

void TtsTest::onMediaPlayerError(QMediaPlayer::Error error)
{
    QString errorString;
    switch (error) {
    case QMediaPlayer::ResourceError:
        errorString = "资源错误：无法找到或打开音频文件";
        break;
    case QMediaPlayer::FormatError:
        errorString = "格式错误：不支持的音频格式";
        break;
    case QMediaPlayer::NetworkError:
        errorString = "网络错误";
        break;
    case QMediaPlayer::AccessDeniedError:
        errorString = "访问被拒绝：没有权限播放音频文件";
        break;
    default:
        errorString = "未知播放错误";
        break;
    }
    
    qDebug() << "音频播放错误:" << errorString;
    
    m_statusLabel->setText(QString("播放失败：%1").arg(errorString));
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 12px;"
        "    color: #e74c3c;"
        "    background-color: rgba(231, 76, 60, 20);"
        "    padding: 8px;"
        "    border-radius: 6px;"
        "    border: 1px solid #c0392b;"
        "}"
    );
    
    QMessageBox::warning(this, "播放错误", QString("音频播放失败：\n\n%1").arg(errorString));
}
