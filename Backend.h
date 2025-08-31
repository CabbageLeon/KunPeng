#ifndef BACKEND_H
#define BACKEND_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QPointer>
#include <QAudioSource>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "VoiceprintRequest.h"

class Backend : public QWidget
{
    Q_OBJECT

public:
    explicit Backend(QWidget *parent = nullptr);
    ~Backend();
    
    // 公共接口，供Home调用
    void performVoiceprintRecognition(const QByteArray &audioData);

signals:
    void voiceprintRecognitionResult(const QString &result);
    void voiceprintRecognitionError(const QString &error);

private slots:
    void onRecordClicked();
    void onQueryClicked();
    void onAddClicked();
    void onDeleteClicked();
    void onTestClicked();
    void onRegisterClicked();      // 登记来访者槽函数
    void onClearVisitorClicked();  // 清除来访者槽函数
    void onTestingTimeout();
    void onAudioReady();
    void onRecordingTimeout();
    void onApiFinished(const QJsonObject &result);
    void onApiError(const QString &error);

private:
    void setupUI();
    void setupAudio();
    void setupApi();
    void updateStatus(const QString &text);
    void startRecording();
    void stopRecording();
    void startTesting();
    void stopTesting();
    void populateListFromQueryPayload(const QJsonObject &payload);
    void displaySearchResult(const QJsonDocument &doc);
    bool saveAudioAsWav(const QByteArray &audioData, const QString &filePath);
    void cleanupOldTempFiles();
    bool hasAudioContent(const QByteArray& audioData);
    void loadVisitorFromFile();    // 从文件读取来访者信息
    void saveVisitorToFile(const QString &visitorName); // 保存来访者信息到文件
    void clearVisitorFile();       // 清空来访者文件

private:
    // UI
    QHBoxLayout *m_mainLayout;
    QVBoxLayout *m_leftLayout;
    QVBoxLayout *m_rightLayout;
    QListWidget *m_list;
    QPushButton *m_btnRecord;
    QPushButton *m_btnQuery;
    QPushButton *m_btnAdd;
    QPushButton *m_btnTest;
    QLineEdit *m_featureIdEdit;
    QLineEdit *m_visitorNameEdit;  // 来访者姓名输入框
    QPushButton *m_btnRegister;    // 登记按钮
    QLabel *m_status;
    QLabel *m_testResult;
    QLabel *m_visitorDisplay;      // 来访者显示标签
    QPushButton *m_btnClearVisitor; // 清除来访者按钮

    // Audio
    QAudioDevice m_audioDevice;
    QAudioFormat m_audioFormat;
    QAudioSource *m_audioSource;
    QPointer<QIODevice> m_audioIo;
    QTimer *m_recordTimer;
    QTimer *m_testTimer;
    QByteArray m_audioBuffer;
    QByteArray m_testBuffer;
    bool m_isRecording;
    bool m_isTesting;
    bool m_isProcessingRequest; // 添加请求状态标志

    // API
    VoiceprintRequest *m_api;
    const QString m_groupName;
};

#endif // BACKEND_H
