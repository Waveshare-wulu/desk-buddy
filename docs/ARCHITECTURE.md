# Desk Buddy 软件架构说明

## 1. 分层架构

Desk Buddy 将硬件无关的核心逻辑、硬件公共服务、统一 UI 主题与独立应用严格解耦：

1. **核心算法层 (`components/desk_buddy_core/`)**
   - 纯标准 C++17，零 ESP-IDF / LVGL 依赖，可在主机或目标端直接执行自动化测试：
     - `RandomEngine`：基于拒绝采样的无偏均匀分布随机数，内置菜品库、答案之书、抛硬币、骰子与安全区间随机。
     - `FortuneEngine`：基于 SplitMix32 确定性日期哈希的每日运势引擎，显式区分有效日期与未校时状态。
     - `Game2048Engine`：标准 4×4 2048 引擎，单次移动单格最多合并一次，支持残局校验加载。
     - `RelativeEngine`：结构化中文亲属关系规则表 + 配偶往返/父母关系归约引擎，避免错误拼接。
     - `ShakeDetector`：高通重力分离 + 双峰滞回阈值 + 冷却窗口算法，过滤拿放微扰。
     - `ButtonLogic`：实体按键消抖与短按/长按状态机。
     - `MalletMotion`：木鱼槌头敲击、抬起与回弹角度状态机。
     - `PomodoroTimer` & `computeOffWorkCountdown`：基于单调时间戳的番茄钟与下班倒计时计算。

2. **硬件公共服务层 (`main/services/`)**
   - CO5300 优先在整屏大小的 PSRAM 缓冲中绘制刷新区域，LVGL 用两个软件绘制单元分担双核。区域画完后，像素轮流复制到两块 30 行内部缓冲，以 40 MHz QSPI DMA 发送，复制下一段与发送上一段重叠，最后一段完成时通知 LVGL。40 MHz 与目标板 BSP 一致，满足 CO5300 手册第 6.4.1 节规定的写时钟周期。`display_service` 初始化面板并通过同一面板 IO 设置亮度。PSRAM 缓冲不可用时，两块内部缓冲改作 LVGL 双缓冲。面板区域按偶数坐标和尺寸对齐，触控沿用目标板的坐标变换。[乐鑫 ESP32-S3 SPI 显示内存说明](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#what-are-some-good-solutions-if-the-internal-ram-of-esp32-s3-driving-an-spi-screen-is-insufficient-to-allocate-space-for-the-entire-screen-buffer)
   - 当前微雪 2.16 原理图的 `LCD_TE` 仅引到测试点。动画优先限制刷新区域和预渲染内容，面板同步方案需要主控能采集 TE 信号。[微雪原理图](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.16/ESP32-S3-Touch-AMOLED-2.16-Schematic.pdf) · [乐鑫屏幕撕裂说明](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/lcd_screen_tearing.html)
   - 所有 I2C 外设统一复用 BSP 已初始化的 `bsp_i2c_get_handle()`（SDA GPIO15 / SCL GPIO14），避免总线重复初始化冲突：
     - `StorageService`：NVS 命名空间 `desk_buddy`（设置结构带 `schema_ver=2` 版本头并兼容 V1 迁移，另保存 Wi-Fi 配置、按键引导状态与 2048 进度）。电子木鱼采用内存累加 + 脏标记 + 15 秒周期刷盘 + `pause()/close()` 强刷策略，保护 Flash 寿命。
     - `ClockService`：直接驱动板载 `PCF85063ATL`（`0x51`），默认固定 `CST-8`（UTC+8）时区；开机校验 OS 标志位与年份范围，未校准时诚实返回 `valid=false`。
     - `WifiTimeService`：按需启停 Wi-Fi（校时完成后关闭无线电），提供开放式 SoftAP（`DeskBuddy-XXXX`，空闲 5 分钟自动超时）与强制门户网页，支持手机直接同步时间或连接 2.4 GHz Wi-Fi 后通过 SNTP（`ntp.aliyun.com`、`cn.pool.ntp.org`、`time.apple.com`）校时。
     - `PowerService`：直接读取 `AXP2101`（`0x34`）状态、电量与电源键短/长按事件；基于 `lv_display_get_inactive_time()` 实现 AMOLED 自动降亮、熄屏及防误触唤醒遮罩（应用定时器与动画不会重置用户输入空闲计时）。
     - `ButtonService`：独立任务每 20 ms 轮询 `GPIO0`（BOOT）与 `GPIO18`（KEY），每 100 ms 轮询 AXP2101 电源键，经事件队列由 LVGL 定时器安全分发。
     - `MotionService`：驱动 `QMI8658`（`0x6B`），仅在支持摇一摇的活跃 App 注册监听时才以 50 Hz 轮询，触发后置位原子标志，由 50 ms LVGL 定时器调用 `pollOnLvglThread()` 在 LVGL 线程分发回调，确保 2048、亲戚计算器等应用绝不误触发。
     - `SoundService`：使用 `bsp_audio_codec_speaker_init()`（ES8311 + NS4150B PA，PA_CTRL=GPIO46）在独立低优先级任务中实时合成短促 PCM 音效，无扬声器或静音时平滑降级为空操作。

3. **桌面与应用层 (`main/ui/` + `main/apps/`)**
   - `brookesia_system_core` `0.8.7` 安装七个 `IApp` 原生应用并驱动生命周期。桌面点击通过队列提交给主任务，应用生命周期回调取得 LVGL 锁后创建或销毁界面。
   - `Desktop` 使用奶油色 `#FAF6EE` 背景和带圆角安全边距的状态栏，以 LVGL `TileView` 承载三页桌面。横向手势在松手时一次切换，设置页只显示设置入口。
   - `DeskBuddyApp` 为每个应用管理独立的内容容器，在暂停和关闭时解绑摇晃监听。顶部返回键和从应用底部中央起始的上滑手势共用应用关闭流程。
   - 随机实验室在进入硬币页时预渲染两张 RGB565 硬币图。抛掷期间只更新硬币图像及其水平缩放，在最窄的一帧切换正反面，结束后再更新统计。离开页面时释放图像缓冲。
   - 木鱼应用在按下时计数并触发槌头回弹。槌头使用预渲染的透明图像，浮字在固定位置淡出。负一屏木鱼卡仍在松手后判定点击，以区分敲击和横滑切页。
   - 2048 在棋盘容器上判定四向滑动，阈值 28 像素。桌面分页仅处理从桌面 `TileView` 内开始的触摸。桌面应用图标与下方名称共用整格触控区，较矮的按钮沿纵向扩大触控区以保留同排按钮之间的分隔。
   - System Core 通过 Device Service 和 LittleFS HAL 获得设备与文件系统能力。设置、功德和 2048 进度仍使用原有 NVS 命名空间。

## 2. 生成的资源

`main/ui/assets/` 中的图标、木鱼图和中文字库由 `tools/` 下的脚本生成，普通构建直接使用仓库内的结果。修改界面文案或图形后重新生成：

```sh
python tools/gen_icons.py
python tools/gen_ui_icons.py
python tools/gen_woodfish_art.py
python tools/gen_fonts.py --regular path/to/NotoSansCJKsc-Regular.otf --bold path/to/NotoSansCJKsc-Bold.otf
```

字库生成需要 Node.js 和 `npx`，字体使用 [Noto Sans CJK 2.004](https://github.com/notofonts/noto-cjk/releases/tag/Sans2.004) 简体中文 Regular 和 Bold 的静态 OTF，生成覆盖源码中中文字符的 4 bpp 位图子集。

