#ifndef HOME_H
#define HOME_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include "VoiceRecognition.h"

class Home : public QWidget
{
    Q_OBJECT

public:
    explicit Home(QWidget *parent = nullptr);
    ~Home();

private slots:
    void onStartRealtimeRecognition();
    void onStopRecognition();
    void onRecognitionResult(const QString &result);
    void onRecognitionError(const QString &error);
    void onRecognitionFinished();

private:
    void setupUI();
    void connectSignals();

private:
    // UI组件
    QVBoxLayout *m_mainLayout;
    
    // 控制按钮
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    
    // 结果显示区域
    QTextEdit *m_resultEdit;
    QLabel *m_statusLabel;
    
    // 语音识别对象
    VoiceRecognition *m_voiceRecognition;
};

#endif // HOME_H
