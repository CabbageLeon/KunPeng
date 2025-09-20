#ifndef VOICETEST_H
#define VOICETEST_H

#include <QWidget>
#include <QTimer>
#include <QByteArray>
#include <cstdint>

// 全局变量声明
extern uint32_t g_audioAddress;      // 32位音频数据地址
extern uint32_t g_audioDataSize;     // 音频数据大小

// 前向声明
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QMediaPlayer;
class QAudioOutput;

class VoiceTest : public QWidget
{
    Q_OBJECT

public:
    explicit VoiceTest(QWidget *parent = nullptr);
    ~VoiceTest();

    // 设置测试用的音频内存数据
    void setTestAudioData(uint32_t audioAddress, uint32_t dataSize);
    
    // 使用全局变量设置音频数据
    void setGlobalAudioData();

private slots:
    void onRecordClicked();
    void onPlayClicked();
    void onStopClicked();
    void onAudioDataRead();
    void updateStatus();

private:
    void setupUI();
    bool saveAudioToTempFile(const QByteArray& audioData, QString& filePath);

private:
    // UI组件
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_recordButton;
    QPushButton* m_playButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;
    QLabel* m_infoLabel;

    // 音频数据源
    uint32_t m_audioAddress;         // 32位音频数据地址
    uint32_t m_audioDataSize;        // 音频数据大小
    uint32_t m_audioDataOffset;      // 当前读取偏移量
    
    // 定时器和缓冲区
    QTimer* m_readTimer;
    QByteArray m_recordedData;
    
    // 播放相关
    QMediaPlayer* m_mediaPlayer;
    QAudioOutput* m_audioOutput;
    QString m_tempAudioFile;
    
    // 状态标志
    bool m_isRecording;
    bool m_isPlaying;
};

#endif // VOICETEST_H