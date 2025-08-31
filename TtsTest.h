#ifndef TTSTEST_H
#define TTSTEST_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "VoiceGenerator.h"

class TtsTest : public QWidget
{
    Q_OBJECT

public:
    explicit TtsTest(QWidget *parent = nullptr);
    ~TtsTest();

private slots:
    void onPlayButtonClicked();
    void onTtsSynthesisStarted();
    void onTtsSynthesisFinished(const QString &audioFilePath);
    void onTtsSynthesisError(const QString &error);
    void onMediaPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaPlayerError(QMediaPlayer::Error error);

private:
    void setupUI();
    void connectSignals();

private:
    // UI组件
    QHBoxLayout *m_mainLayout;
    QLineEdit *m_textEdit;
    QPushButton *m_playButton;
    QLabel *m_statusLabel;
    
    // TTS组件
    VoiceGenerator *m_voiceGenerator;
    
    // 音频播放组件
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
};

#endif // TTSTEST_H
