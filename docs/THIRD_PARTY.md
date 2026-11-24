# Third-Party Notices

Desk Buddy (摸鱼搭子)
Copyright 2026 Wulu

Licensed under the Apache License, Version 2.0. See [LICENSE](../LICENSE).

Desk Buddy (摸鱼搭子) incorporates or depends on the following open-source components. Original copyright notices and licenses are preserved.

1. **ESP-IDF** (Espressif Systems)
   - License: Apache License 2.0
   - https://github.com/espressif/esp-idf

2. **ESP-Brookesia** (Espressif Systems)
   - Components: `espressif/brookesia_system_core`, `espressif/brookesia_hal_interface`, `espressif/brookesia_service_device`
   - License: Apache License 2.0 (see each managed component's `license.txt`)
   - https://github.com/espressif/esp-brookesia

3. **ESP LCD & LVGL Adapter Components** (Espressif Systems)
   - Components: `espressif/esp_lcd_co5300`, `espressif/esp_lvgl_adapter`
   - License: Apache License 2.0
   - https://github.com/espressif/esp-iot-solution

4. **Waveshare ESP32-S3-Touch-AMOLED-2.16 BSP & QMI8658 Driver**
   - Components: `waveshare/esp32_s3_touch_amoled_2_16`, `waveshare/qmi8658`
   - License: Apache License 2.0
   - https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16

5. **LVGL** (LVGL Kft)
   - License: MIT License
   - https://github.com/lvgl/lvgl

6. **Noto Sans CJK SC 2.004** (Google / Adobe)
   - License: [SIL Open Font License 1.1](NotoSansCJK-OFL.txt)
   - Subsetted into C bitmap fonts (`main/ui/assets/db_font_*.c`) via `lv_font_conv`.

7. **Original Desk Buddy Application Code, Icons, and Copywriting**
   - All application logic, SDF vector artwork (`tools/gen_icons.py`, `tools/gen_ui_icons.py`, `tools/gen_woodfish_art.py`), food lists, Book of Answers text, Daily Fortune pools, slacking mottos, and kinship rule tables in `components/desk_buddy_core/` and `main/` are original creations for the Desk Buddy project. They are licensed under Apache License 2.0.

8. **LittleFS for ESP-IDF** (Brian Pugh and LittleFS contributors)
   - Licenses: MIT for the ESP-IDF port and BSD 3-Clause for upstream LittleFS.
   - https://github.com/joltwallet/esp_littlefs
