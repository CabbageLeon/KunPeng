#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include <QObject>
#include <QCamera>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QTimer>
#include <QDir>
#include <QDateTime>
#include <QDebug>

class DeviceControl : public QObject
{
    Q_OBJECT

public:
    explicit DeviceControl(QObject *parent = nullptr);
    ~DeviceControl();

    // 初始化摄像头
    bool initializeCamera();

    // 拍照功能
    void capturePhoto(const QString &visitorName = "访客");

    // 检查相机状态
    bool isCameraReady() const;

    // 预热摄像头
    void warmUpCamera();

    // 设置保存目录
    void setSaveDirectory(const QString &path);

signals:
    // 拍照完成信号
    void photoSaved(const QString &filePath);

    // 拍照失败信号
    void photoCaptureFailed(const QString &error);

    // 摄像头状态变化信号
    void cameraStateChanged(bool ready);

private slots:
    // 拍照完成处理
    void onPhotoCaptured(int id, const QImage &image);

    // 拍照错误处理
    void onPhotoCaptureError(int id, QImageCapture::Error error, const QString &errorString);

    // 相机状态变化处理
    void onCameraActiveChanged(bool active);

    // 相机错误处理
    void onCameraErrorOccurred(QCamera::Error error, const QString &errorString);

    // 延迟拍照槽函数
    void onDelayedCapture();

    // 图像捕获器就绪处理
    void onImageCaptureReady(bool ready);

private:
    // 创建保存目录
    bool createSaveDirectory();

    // 生成文件名
    QString generateFileName(const QString &visitorName);

    // 配置摄像头设置
    void configureCamera();

private:
    QCamera *m_camera;                    // 摄像头对象
    QImageCapture *m_imageCapture;        // 图像捕获对象
    QMediaCaptureSession *m_captureSession; // 媒体捕获会话

    QString m_saveDirectory;              // 保存目录
    QString m_pendingVisitorName;         // 待处理的访客名称

    bool m_isInitialized;                 // 是否已初始化
    QTimer *m_delayTimer;                 // 延迟拍照定时器
    bool m_isCameraWarmedUp;              // 摄像头是否已预热
    int m_captureRetryCount;              // 捕获重试计数器
};

#endif // DEVICECONTROL_H