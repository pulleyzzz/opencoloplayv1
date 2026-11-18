# opencoloplayv1

coloplay v1 硬件的开源固件开发包:板级支持包(BSP)+ 示例应用,供社区为 coloplay 硬件开发自定义固件。

| 项目 | 说明 |
| --- | --- |
| 硬件 | coloplay v1(ESP32-S3,16MB Flash,8MB 八线 PSRAM) |
| ESP-IDF | 6.1 或更新(在 6.2 上验证) |
| 许可证 | Apache-2.0 |

## 支持的外设

| 外设 | 器件 | 接口 |
| --- | --- | --- |
| 屏幕 | GC9A01 240×240 圆屏 | SPI + LVGL(esp_lvgl_adapter) |
| 音频 | ES8311 喇叭/麦克风 | I2S + esp_codec_dev,PA 静音控制 |
| 按键 | 6 键(含旋钮按压) | espressif/button |
| 旋钮 | EC11 编码器 | PCNT 4 倍频解码 |
| LED | SK6812 ×13(9 环灯 + 4 按键背光) | RMT(led_strip) |

## 仓库结构

```
components/opencoloplayv1-bsp/   BSP:全部驱动与引脚定义
components/led_strip/            espressif/led_strip 2.5.5 修复版(IDF 6.2 兼容)
examples/hello_coloplay/         示例:LVGL UI + 全部外设演示
docs/developer-guide.md          二次开发指南(中文)
```

## 快速开始

```bash
source $IDF_PATH/export.sh               # 激活 ESP-IDF 6.1+
cd examples/hello_coloplay
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

详细内容(各驱动 API 用法、烧录方式、圆屏布局、已知注意事项)见
[二次开发指南](docs/developer-guide.md)。
