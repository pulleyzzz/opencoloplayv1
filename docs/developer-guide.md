# opencoloplayv1 二次开发指南

面向想在 coloplay v1 硬件上开发自定义固件的开发者。BSP(板级支持包)已经封装了全部外设驱动,你只需要专注应用逻辑。

## 目录

- [环境搭建](#环境搭建)
- [从示例新建自己的应用](#从示例新建自己的应用)
- [硬件资源与引脚](#硬件资源与引脚)
- [驱动 API 用法](#驱动-api-用法)
  - [屏幕(GC9A01 + LVGL)](#屏幕gc9a01--lvgl)
  - [音频(ES8311 喇叭/麦克风)](#音频es8311-喇叭麦克风)
  - [按键(6 键)](#按键6-键)
  - [旋钮(EC11 编码器)](#旋钮ec11-编码器)
  - [LED 灯环(SK6812×14)](#led-灯环sk6812x13)
- [圆屏 UI 布局指南](#圆屏-ui-布局指南)
- [Kconfig 配置项](#kconfig-配置项)
- [烧录方式](#烧录方式)
- [已知注意事项](#已知注意事项)

## 环境搭建

- **ESP-IDF 6.1 或更新**(在 v6.2.0 master 上验证过)
- 若使用 EIM 安装的 IDF(本仓库开发者的情况),用 EIM 激活脚本,**不要用 `export.sh`**(它找的 python venv 可能不存在):

```bash
. ~/.espressif/tools/activate_idf_master.sh    # 或你的 EIM 对应激活脚本
idf.py --version
```

标准安装的用户直接 `source $IDF_PATH/export.sh` 即可。

## 从示例新建自己的应用

最快的路径是复制示例:

```bash
cp -r examples/hello_coloplay examples/my_app
cd examples/my_app
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

示例通过**相对路径**依赖仓库内的 BSP(`main/idf_component.yml`):

```yaml
dependencies:
  idf:
    version: ">=6.1"
  opencoloplayv1-bsp:
    path: ../../../components/opencoloplayv1-bsp
```

应用入口只包含 `main/main.c`,一个 `app_main()`。示例里已经演示了全部驱动的初始化和联动,可以按需删改。

所有头文件通过一个伞头文件引入:

```c
#include "bsp/opencoloplayv1.h"   // 引入全部 BSP API + 引脚宏
```

## 硬件资源与引脚

| 外设 | 器件/总线 | 引脚 |
| --- | --- | --- |
| 屏幕 | GC9A01 240×240 圆屏 RGB565,SPI2 | MOSI=13 CLK=14 CS=12 DC=11 RST=45 |
| 背光 | LEDC PWM,高有效 | GPIO18 |
| 音频 codec | ES8311,I2C0 @0x18 | SDA=42 SCL=41 |
| 音频数据 | I2S0 主机,16k/16bit/mono-left | MCLK=40 BCLK=39 WS=47 DOUT=21 DIN=38 |
| PA 使能 | 高=出声,低=静音 | GPIO2 |
| 按键 ×6 | 低有效,内部上拉, | K1=6 K2=1 K3=4 K4=5 EKEY=46 P=15 |
| 旋钮 | EC11,PCNT 4 倍频 | A=9 B=10 |
| LED 灯环 | SK6812×13(9个环灯+4个按键灯),RMT 驱动 | DATA=48 |
| LED 电源轨 | 高=开 | GPIO8 |

SoC:ESP32-S3,16MB Flash,8MB 八线 PSRAM。

## 驱动 API 用法

### 屏幕(GC9A01 + LVGL)

最简单的方式 — 一行启动 LVGL(自动建 panel、双缓冲、刷新任务):

```c
lv_display_t *disp = bsp_display_start();   // NULL 表示失败
```

之后正常使用 LVGL。**跨线程操作 LVGL 前必须拿锁**:

```c
if (bsp_display_lock(100)) {     // 超时 100ms
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello coloplay");
    bsp_display_unlock();
}
```

其他接口:

```c
bsp_display_brightness_set(70);              // 背光 0..100
bsp_display_set_rotation(BSP_DISPLAY_ROTATE_90);  // 运行时旋转
bsp_display_off();                           // 息屏(Display Off + Sleep In,暂停 LVGL)
bsp_display_on();                            // 亮屏并恢复 LVGL
esp_lcd_panel_handle_t p = bsp_display_get_panel();  // 需要 esp_lcd 原生句柄时
```

如果不用 LVGL,`bsp_display_new(NULL, &panel)` 只初始化面板,自己用 `esp_lcd_panel_draw_bitmap()` 刷屏。

### 音频(ES8311 喇叭/麦克风)

speaker/mic 是两个独立的 `esp_codec_dev` 设备,共享同一条 I2S0:

```c
esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
```

**正确的使用姿势(重要,踩过坑)**:

```c
// 1. 播放前:open + 解除静音
esp_codec_dev_sample_info_t fs = {
    .sample_rate = 16000, .channel = 1, .bits_per_sample = 16,
};
esp_codec_dev_open(spk, &fs);               // open 是必须的,write 前提
esp_codec_dev_set_out_vol(spk, 70);
esp_codec_dev_set_out_mute(spk, false);
esp_codec_dev_write(spk, buf, len);          // 阻塞写
esp_codec_dev_set_out_mute(spk, true);       // 播完静音,但不要 close!

// 2. 录音:open mic → 读 → close mic(spk 保持 open 撑住芯片供电)
esp_codec_dev_open(mic, &fs);
esp_codec_dev_set_in_gain(mic, 30.0);        // 30dB 合适,42dB 说话削顶
esp_codec_dev_read(mic, buf, len);           // 阻塞读
esp_codec_dev_close(mic);
```

为什么不能把 speaker 也 close?见[已知注意事项](#已知注意事项)第 1 条。

自定义采样率/声道:`bsp_audio_init(&cfg)` 传自定义 `i2s_std_config_t`(须在第一次调用 codec 构造之前)。

### 按键(6 键)

k1左上角, k2右上角, k3右下角, k4左下角. 基于 espressif/button(iot_button),拿到的是标准 `button_handle_t`:

```c
button_handle_t btns[BSP_BUTTON_NUM];
int cnt = 0;
bsp_iot_button_create(btns, &cnt, BSP_BUTTON_NUM);

// 索引即 bsp_button_t:BSP_BUTTON_1..4 / BSP_BUTTON_KNOB(旋钮按压)/ BSP_BUTTON_PWR
iot_button_register_cb(btns[BSP_BUTTON_KNOB], BUTTON_PRESS_DOWN, NULL, my_cb, NULL);
iot_button_register_cb(btns[BSP_BUTTON_1], BUTTON_LONG_PRESS_UP, NULL, my_long_cb, NULL);
```

iot_button 的全部事件(单击/双击/长按/连击计数)都可用。

### 旋钮(EC11 编码器)

```c
bsp_knob_init();
int32_t count = 0;
bsp_knob_get_count(&count);   // 顺时针为正;1 个物理棘格 = 4 个 count
bsp_knob_clear_count();       // 清零
```

计数在 ±300(`BSP_KNOB_COUNT_HIGH/LOW_LIMIT`)之间回绕,持续旋转的场景要自己做 delta 累加(参考 hello_coloplay 的 `knob_timer_cb`,含余数保持的写法)。

### LED 灯环(SK6812×14)

```c
bsp_led_init();                                // 自动使能 LED 电源轨(GPIO8)
bsp_led_set_pixel(0, 255, 0, 0);               // 红色,写缓冲
bsp_led_set_all(0, 0, 64);                     // 整环低亮蓝色
bsp_led_refresh();                             // 一次性刷出
bsp_led_clear();                               // 全灭
bsp_led_set_power(false);                      // 彻底断 LED 电源轨
```

如果颜色不对(红绿蓝错位),是 SK6812 通道顺序问题,在 BSP `src/led.c` 里调 `led_strip_set_pixel` 的参数映射即可。

## 圆屏 UI 布局指南

240×240 的屏,可见区域是**内切圆**(圆心 120,120,半径 120)。方形布局的四个角都不可见!文字在垂直偏移 y 处的可用宽度:

| 偏移 y(距圆心) | 可用宽度 |
| --- | --- |
| ±40 | 227px |
| ±60 | 208px |
| ±80 | 179px |
| ±97 | 129px |
| ±104 | 119px |
| ±110 | 66px |

公式:`width = 2 * sqrt(120² − y²)`。放控件前先量一下文字实际宽度(`lv_txt_get_size()`),超出就换行、缩字号或上移。hello_coloplay 的 `create_ui()` 注释里有现成的计算示例。

## Kconfig 配置项

`idf.py menuconfig` → "opencoloplayv1 BSP":

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `BSP_GC9A01_SPI_CLK_HZ` | 40000000 | 屏幕 SPI 时钟。产品固件跑 80MHz,40MHz 稳妥;改 80MHz 需真机验证 |
| `BSP_GC9A01_BACKLIGHT_DEFAULT_DUTY` | 70 | 初始化时的背光占空比(%) |

## 烧录方式

### 方式一:USB-Serial-JTAG(默认)

插上 USB,esptool 会自动复位进 bootloader:

```bash
idf.py -p /dev/tty.usbmodemXXXX flash monitor
```

### 方式二:组合键手动进入烧录模式

当 USB 口被应用占用(例如固件自己接管了 USB CDC 通信)、esptool 无法自动复位时:

1. 设备断电(拔掉 USB)
2. **按住旋钮(按下)+ 背面 power 键**
3. 保持按住,重新上电(插回 USB)
4. 进入烧录模式后松开按键,执行:

```bash
idf.py -p /dev/tty.usbmodemXXXX --before no_reset flash
```

> 若报 `Failed to connect`,去掉 `--before no_reset` 重试,或重新按组合键进模式。

## 已知注意事项

1. **不要让 ES8311 refcount 归零**:最后一个 codec 设备 `esp_codec_dev_close()` 会执行整芯片断电序列。反复断电/重开后 ADC 模拟路径会静音失效(寄存器都写了但收不到信号;产品固件的 `check_i2cs_state` 自愈循环就是同一问题的兜底)。**speaker 常开、空闲 DAC mute,mic 按需开合**是验证过的安全姿势。
2. **不要手动 `i2s_channel_enable`**:BSP 的 `bsp_audio_init` 故意不使能 TX/RX,esp_codec_dev 在 open/close 时自管通道。强制使能 RX 会导致 mic 常开并串到喇叭。
3. **mic 增益**:30dB 起步;42dB 正常说话就满幅削顶。
4. **改 `idf_component.yml` 后 `idf.py reconfigure` 可能不重新求解**(沿用旧 `dependencies.lock`):先 `rm dependencies.lock` 再 reconfigure。
5. **led_strip 本地 fork**:`components/led_strip` 是 2.5.5 的本地修复版(上游 SPI 后端缺 include,在 IDF 6.2 编不过)。上游发修复版后可换回 registry 依赖。
6. 屏幕反色(0x21)等 vendor 初始化序列来自 coloplay 产品固件,**不要动** `src/display.c` 里的 `s_vendor_init` 表。

## 常用命令速查

```bash
. ~/.espressif/tools/activate_idf_master.sh       # 激活 IDF(EIM 安装)
idf.py build                                      # 编译
idf.py -p /dev/tty.usbmodemXXXX flash monitor     # 烧录 + 串口
idf.py menuconfig                                 # 配置(Kconfig 在 "opencoloplayv1 BSP" 菜单)
idf.py fullclean                                  # 彻底清理
```

---

有问题或想贡献驱动?欢迎在仓库提 Issue / Pull Request。
