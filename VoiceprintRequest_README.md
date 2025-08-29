# 声纹识别 C++ 版本

这是将原Python版本的讯飞声纹识别API转换为C++版本的实现。

## 文件说明

- `VoiceprintRequest.h` - 声纹识别类头文件
- `VoiceprintRequest.cpp` - 声纹识别类实现文件  
- `VoiceprintExample.cpp` - 使用示例代码

## 功能特性

### 支持的API操作

1. **createGroup** - 创建声纹特征库
2. **createFeature** - 添加音频特征
3. **queryFeatureList** - 查询特征列表
4. **searchScoreFea** - 特征比对1:1（验证身份）
5. **searchFea** - 特征比对1:N（从多个声纹中找匹配）
6. **updateFeature** - 更新音频特征
7. **deleteFeature** - 删除指定特征
8. **deleteGroup** - 删除声纹特征库

### 技术要求

- Qt 6.x 框架
- 音频格式：16kHz采样率、16位、MP3格式
- 文件大小：Base64编码后不超过4MB
- 网络连接：需要访问讯飞开放平台API

## 使用方法

### 1. 设置API认证信息

```cpp
VoiceprintRequest *voiceprintRequest = new VoiceprintRequest();
voiceprintRequest->setCredentials("您的APPID", "您的APIKey", "您的APISecret");
```

### 2. 连接信号槽

```cpp
// 连接成功信号
connect(voiceprintRequest, &VoiceprintRequest::requestFinished, 
        [](const QJsonObject &result) {
    qDebug() << "请求成功:" << result;
});

// 连接错误信号
connect(voiceprintRequest, &VoiceprintRequest::requestError,
        [](const QString &error) {
    qDebug() << "请求失败:" << error;
});
```

### 3. 调用API接口

```cpp
// 创建声纹特征库
voiceprintRequest->createGroup("my_group_id", "我的特征库", "测试特征库");

// 添加音频特征
voiceprintRequest->createFeature("audio.mp3", "my_group_id", "feature_001", "特征1");

// 声纹比对1:1
voiceprintRequest->searchScoreFeature("test_audio.mp3", "my_group_id", "feature_001");
```

## 注意事项

1. **groupId顺序**：必须先调用`createGroup`创建特征库，然后才能使用`createFeature`添加特征，否则会报错23005。

2. **音频格式**：确保音频文件为16K采样率、16位深度的MP3格式。

3. **文件大小**：音频文件Base64编码后不能超过4MB。

4. **API凭证**：请在讯飞开放平台获取有效的APPID、APIKey和APISecret。

5. **网络环境**：确保能正常访问`https://api.xf-yun.com`。

## 编译配置

确保CMakeLists.txt中包含了必要的Qt组件：

```cmake
find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebSockets
    REQUIRED)

target_link_libraries(KunPeng
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Network
    Qt::WebSockets
)
```

## 错误处理

所有的网络错误和API错误都会通过`requestError`信号发出，请确保连接了该信号来处理错误情况。

## 异步操作

所有API调用都是异步的，结果通过信号返回。请不要在同一个对象上并发调用多个API，等待前一个请求完成后再发起下一个请求。
