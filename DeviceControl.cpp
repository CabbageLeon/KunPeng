#include "DeviceControl.h"
#include <QStandardPaths>
#include <QDir>
#include <QMediaDevices>
#include <QFileInfo>

DeviceControl::DeviceControl(QObject *parent)
    : QObject(parent)
    , m_camera(nullptr)
    , m_imageCapture(nullptr)
    , m_captureSession(nullptr)
    , m_saveDirectory("../pic/")
    , m_isInitialized(false)
    , m_delayTimer(new QTimer(this))
    , m_isCameraWarmedUp(false)
    , m_captureRetryCount(0)
{
    // 创建保存目录
    createSaveDirectory();
    
    // 设置延迟定时器
    m_delayTimer->setSingleShot(true);
    connect(m_delayTimer, &QTimer::timeout, this, &DeviceControl::onDelayedCapture);
    
    // 立即初始化摄像头以便预热
    initializeCamera();
}

DeviceControl::~DeviceControl()
{
    if (m_camera) {
        m_camera->stop();
    }
}

bool DeviceControl::initializeCamera()
{
    if (m_isInitialized) {
        return true;
    }
    
    try {
        // 获取可用的摄像头设备
        const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
        if (cameras.isEmpty()) {
            qDebug() << "没有找到可用的摄像头设备";
            emit photoCaptureFailed("没有找到可用的摄像头设备");
            return false;
        }
        
        // 选择后置摄像头（通常质量更好）
        QCameraDevice selectedCamera;
        for (const auto &camera : cameras) {
            if (camera.position() == QCameraDevice::BackFace) {
                selectedCamera = camera;
                break;
            }
        }
        
        // 如果没有后置摄像头，使用第一个可用摄像头
        if (selectedCamera.isNull()) {
            selectedCamera = cameras.first();
        }
        
        // 创建摄像头对象
        m_camera = new QCamera(selectedCamera, this);
        
        // 创建图像捕获对象
        m_imageCapture = new QImageCapture(this);
        
        // 配置图像格式和质量 - 修复枚举值
        m_imageCapture->setFileFormat(QImageCapture::JPEG);
        m_imageCapture->setQuality(QImageCapture::HighQuality); // 修正为 HighQuality
        
        // 创建媒体捕获会话
        m_captureSession = new QMediaCaptureSession(this);
        m_captureSession->setCamera(m_camera);
        m_captureSession->setImageCapture(m_imageCapture);
        
        // 配置摄像头设置
        configureCamera();
        
        // 连接信号
        connect(m_imageCapture, &QImageCapture::imageCaptured,
                this, &DeviceControl::onPhotoCaptured);
        connect(m_imageCapture, &QImageCapture::errorOccurred,
                this, &DeviceControl::onPhotoCaptureError);
        connect(m_imageCapture, &QImageCapture::readyForCaptureChanged,
                this, &DeviceControl::onImageCaptureReady);
        connect(m_camera, &QCamera::activeChanged,
                this, &DeviceControl::onCameraActiveChanged);
        connect(m_camera, &QCamera::errorOccurred,
                this, &DeviceControl::onCameraErrorOccurred);
        
        // 启动摄像头
        m_camera->start();
        
        qDebug() << "摄像头初始化成功，设备:" << selectedCamera.description();
        m_isInitialized = true;
        
        // 启动预热定时器，给摄像头更多时间来调整
        QTimer::singleShot(3000, this, &DeviceControl::warmUpCamera);
        
        return true;
        
    } catch (const std::exception &e) {
        qDebug() << "摄像头初始化失败:" << e.what();
        emit photoCaptureFailed(QString("摄像头初始化失败: %1").arg(e.what()));
        return false;
    }
}

void DeviceControl::configureCamera()
{
    if (!m_camera) return;
    
    // 设置曝光模式
    m_camera->setExposureMode(QCamera::ExposureAuto);
    
    // 设置白平衡模式
    m_camera->setWhiteBalanceMode(QCamera::WhiteBalanceAuto);
    
    // 设置对焦模式
    m_camera->setFocusMode(QCamera::FocusModeAuto);
    
    // 设置分辨率（选择最高支持的分辨率）
    // 注意：QCameraDevice::setPhotoResolution 在 Qt 6 中可能不存在
    // 我们可以尝试使用 QImageCapture::setResolution 来设置分辨率
    QSize maxResolution(0, 0);
    const auto formats = m_camera->cameraDevice().photoResolutions();
    for (const auto &resolution : formats) {
        if (resolution.width() * resolution.height() > maxResolution.width() * maxResolution.height()) {
            maxResolution = resolution;
        }
    }
    
    if (!maxResolution.isNull() && m_imageCapture) {
        // 使用 QImageCapture 设置分辨率
        m_imageCapture->setResolution(maxResolution);
        qDebug() << "设置图像捕获分辨率:" << maxResolution;
    }
}

void DeviceControl::capturePhoto(const QString &visitorName)
{
    if (!m_isInitialized) {
        if (!initializeCamera()) {
            return;
        }
    }
    
    if (!isCameraReady()) {
        qDebug() << "摄像头未准备就绪，无法拍照";
        emit photoCaptureFailed("摄像头未准备就绪");
        return;
    }
    
    // 保存访客名称，用于文件命名
    m_pendingVisitorName = visitorName;
    m_captureRetryCount = 0;
    
    qDebug() << "开始为访客拍照:" << visitorName << "摄像头预热状态:" << m_isCameraWarmedUp;
    
    if (!m_isCameraWarmedUp) {
        // 如果摄像头还没有预热完成，延迟拍照
        qDebug() << "摄像头正在预热，延迟3秒后拍照";
        m_delayTimer->start(3000);
    } else {
        // 直接拍照
        onDelayedCapture();
    }
}

bool DeviceControl::isCameraReady() const
{
    return m_camera && m_camera->isActive() && 
           m_imageCapture && m_imageCapture->isReadyForCapture();
}

void DeviceControl::onPhotoCaptured(int id, const QImage &image)
{
    Q_UNUSED(id)
    
    qDebug() << "拍照回调被调用，图像大小:" << image.size() << "是否为空:" << image.isNull();
    
    if (image.isNull()) {
        qDebug() << "捕获的图像为空";
        
        // 重试机制
        if (m_captureRetryCount < 3) {
            m_captureRetryCount++;
            qDebug() << "第" << m_captureRetryCount << "次重试拍照";
            QTimer::singleShot(1000, this, [this]() {
                if (m_imageCapture && m_imageCapture->isReadyForCapture()) {
                    m_imageCapture->capture();
                }
            });
            return;
        }
        
        emit photoCaptureFailed("捕获的图像为空");
        return;
    }
    
    // 生成文件名
    QString fileName = generateFileName(m_pendingVisitorName);
    QString fullPath = QDir::cleanPath(m_saveDirectory + fileName);
    
    qDebug() << "准备保存图像 - 目录:" << m_saveDirectory << "文件名:" << fileName;
    qDebug() << "完整路径:" << fullPath;
    qDebug() << "图像信息 - 大小:" << image.size() << "格式:" << image.format() << "字节数:" << image.sizeInBytes();
    
    // 确保目录存在
    QDir dir(m_saveDirectory);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "无法创建保存目录:" << m_saveDirectory;
            emit photoCaptureFailed(QString("无法创建保存目录: %1").arg(m_saveDirectory));
            return;
        }
        qDebug() << "创建保存目录成功:" << m_saveDirectory;
    }
    
    // 转换图像格式确保兼容性
    QImage convertedImage = image;
    if (image.format() != QImage::Format_RGB888 && image.format() != QImage::Format_RGB32) {
        convertedImage = image.convertToFormat(QImage::Format_RGB888);
        qDebug() << "图像格式已转换为 RGB888";
    }
    
    // 保存图像，使用高质量JPEG格式
    bool saveResult = convertedImage.save(fullPath, "JPEG", 95);
    
    if (saveResult) {
        // 验证文件是否确实保存成功
        QFileInfo fileInfo(fullPath);
        if (fileInfo.exists() && fileInfo.size() > 0) {
            qDebug() << "访客照片保存成功:" << fullPath << "文件大小:" << fileInfo.size() << "字节";
            emit photoSaved(fullPath);
        } else {
            qDebug() << "照片保存后验证失败 - 文件不存在或大小为0";
            emit photoCaptureFailed("照片保存后验证失败");
        }
    } else {
        qDebug() << "访客照片保存失败:" << fullPath;
        qDebug() << "可能的原因：权限不足、磁盘空间不足或路径无效";
        emit photoCaptureFailed(QString("照片保存失败: %1").arg(fullPath));
    }
    
    // 清空待处理的访客名称
    m_pendingVisitorName.clear();
    m_captureRetryCount = 0;
}

void DeviceControl::onPhotoCaptureError(int id, QImageCapture::Error error, const QString &errorString)
{
    Q_UNUSED(id)
    Q_UNUSED(error)
    
    qDebug() << "拍照失败:" << errorString;
    emit photoCaptureFailed(QString("拍照失败: %1").arg(errorString));
    
    // 清空待处理的访客名称
    m_pendingVisitorName.clear();
    m_captureRetryCount = 0;
}

void DeviceControl::onCameraActiveChanged(bool active)
{
    qDebug() << "摄像头状态变化:" << (active ? "激活" : "未激活");
    emit cameraStateChanged(active);
}

void DeviceControl::onCameraErrorOccurred(QCamera::Error error, const QString &errorString)
{
    Q_UNUSED(error)
    
    qDebug() << "摄像头错误:" << errorString;
    emit photoCaptureFailed(QString("摄像头错误: %1").arg(errorString));
}

void DeviceControl::onImageCaptureReady(bool ready)
{
    qDebug() << "图像捕获器就绪状态:" << ready;
}

bool DeviceControl::createSaveDirectory()
{
    QDir dir;
    if (!dir.exists(m_saveDirectory)) {
        // 确保使用绝对路径进行创建
        QDir currentDir;
        QString absolutePath = currentDir.absoluteFilePath(m_saveDirectory);
        qDebug() << "尝试创建目录，相对路径:" << m_saveDirectory << ", 绝对路径:" << absolutePath;
        
        if (dir.mkpath(m_saveDirectory)) {
            qDebug() << "创建保存目录成功:" << m_saveDirectory;
            return true;
        } else {
            qDebug() << "创建保存目录失败:" << m_saveDirectory;
            return false;
        }
    }
    
    qDebug() << "保存目录已存在:" << m_saveDirectory;
    return true;
}

QString DeviceControl::generateFileName(const QString &visitorName)
{
    // 获取当前时间
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeString = currentTime.toString("yyyyMMdd_hhmmss_zzz");
    
    // 清理访客名称，移除特殊字符 - 使用简单的字符替换
    QString cleanVisitorName = visitorName;
    // 替换常见的特殊字符
    cleanVisitorName.replace("\\", "_");
    cleanVisitorName.replace("/", "_");
    cleanVisitorName.replace(":", "_");
    cleanVisitorName.replace("*", "_");
    cleanVisitorName.replace("?", "_");
    cleanVisitorName.replace("\"", "_");
    cleanVisitorName.replace("<", "_");
    cleanVisitorName.replace(">", "_");
    cleanVisitorName.replace("|", "_");
    
    // 生成文件名: 访客名_时间.jpg
    QString fileName = QString("%1_%2.jpg").arg(cleanVisitorName).arg(timeString);
    
    qDebug() << "生成文件名:" << fileName;
    return fileName;
}

void DeviceControl::warmUpCamera()
{
    if (m_camera && m_camera->isActive()) {
        m_isCameraWarmedUp = true;
        qDebug() << "摄像头预热完成，可以正常拍照";
    } else {
        qDebug() << "摄像头预热失败，摄像头未激活";
        // 尝试重新初始化
        if (!m_isInitialized) {
            QTimer::singleShot(2000, this, &DeviceControl::initializeCamera);
        }
    }
}

void DeviceControl::onDelayedCapture()
{
    if (!isCameraReady()) {
        qDebug() << "延迟拍照时摄像头仍未准备就绪";
        
        // 重试机制
        if (m_captureRetryCount < 3) {
            m_captureRetryCount++;
            qDebug() << "第" << m_captureRetryCount << "次重试延迟拍照";
            m_delayTimer->start(2000);
            return;
        }
        
        emit photoCaptureFailed("摄像头延迟初始化失败");
        m_pendingVisitorName.clear();
        m_captureRetryCount = 0;
        return;
    }
    
    qDebug() << "执行延迟拍照，访客:" << m_pendingVisitorName;
    
    // 再次检查图像捕获的状态
    if (!m_imageCapture->isReadyForCapture()) {
        qDebug() << "图像捕获器未准备就绪，再次尝试";
        // 再延迟1秒
        m_delayTimer->start(1000);
        return;
    }
    
    // 执行拍照
    m_imageCapture->capture();
}

void DeviceControl::setSaveDirectory(const QString &path)
{
    if (path.isEmpty()) return;
    
    m_saveDirectory = path;
    if (!m_saveDirectory.endsWith("/")) {
        m_saveDirectory += "/";
    }
    
    createSaveDirectory();
}