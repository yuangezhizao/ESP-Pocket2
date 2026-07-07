#include "lgfx.h"
#include "lgfx_hp28008.hpp"

// Refer: https://web.archive.org/web/20250918152441/https://blog.csdn.net/weixin_41589183/article/details/137756695

static LGFX_HP28008 lcd;
static LGFX_Sprite sprite(&lcd);

void lgfx_test(void)
{
    uint32_t count = ~0;   // 原为文件级全局符号，收编进函数内消除符号污染
    lcd.init();
    lcd.setColorDepth(1);
    sprite.setColorDepth(1);
    lcd.setTextSize((std::max(lcd.width(), lcd.height()) + 255) >> 8);
    lcd.fillScreen(TFT_BLACK);

    while (1)
    {
        lcd.startWrite();
        lcd.setRotation(++count & 7);
        lcd.setColorDepth((count & 8) ? 16 : 24);

        lcd.setTextColor(TFT_BLACK);
        lcd.drawNumber(lcd.getRotation(), 16, 0);

        lcd.setTextColor(0xFF0000U);
        lcd.drawString("R", 25, 16);
        lcd.setTextColor(0x00FF00U);
        lcd.drawString("G", 32, 16);
        lcd.setTextColor(0x0000FFU);
        lcd.drawString("B", 39, 16);
        lcd.setTextColor(0x0467FFU);
        lcd.drawString("JCZN", 25, 3);
        lcd.drawRect(30, 30, lcd.width() - 60, lcd.height() - 60, count * 7);
        lcd.drawFastHLine(0, 0, 10);

        lcd.endWrite();

        int32_t x, y;
        if (lcd.getTouch(&x, &y))
        {
            lcd.fillRect(x - 2, y - 2, 5, 5, count * 7);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
