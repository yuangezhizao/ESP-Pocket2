#include "lgfx.h"

#define LGFX_USE_V1

#include <LovyanGFX.hpp>

// ESP32でLovyanGFXを独自設定で利用する場合の設定例

/*
このファイルを複製し、新しい名前を付けて、環境に合わせて設定内容を変更してください。
作成したファイルをユーザープログラムからincludeすることで利用可能になります。

複製したファイルはライブラリのlgfx_userフォルダに置いて利用しても構いませんが、
その場合はライブラリのアップデート時に削除される可能性があるのでご注意ください。

安全に運用したい場合はバックアップを作成しておくか、ユーザープロジェクトのフォルダに置いてください。
//*/

/// 独自の設定を行うクラスを、LGFX_Deviceから派生して作成します。
class LGFX : public lgfx::LGFX_Device
{
    /*
     クラス名は"LGFX"から別の名前に変更しても構いません。
     AUTODETECTと併用する場合は"LGFX"は使用されているため、LGFX以外の名前に変更してください。
     また、複数枚のパネルを同時使用する場合もそれぞれに異なる名前を付けてください。
     ※ クラス名を変更する場合はコンストラクタの名前も併せて同じ名前に変更が必要です。

     名前の付け方は自由に決めて構いませんが、設定が増えた場合を想定し、
     例えばESP32 DevKit-CでSPI接続のILI9341の設定を行った場合、
      LGFX_DevKitC_SPI_ILI9341
     のような名前にし、ファイル名とクラス名を一致させておくことで、利用時に迷いにくくなります。
    //*/

    // 接続するパネルの型にあったインスタンスを用意します。
    lgfx::Panel_ST7789 _panel_instance;

    // パネルを接続するバスの種類にあったインスタンスを用意します。
    lgfx::Bus_SPI _bus_instance; // SPIバスのインスタンス

    // // バックライト制御が可能な場合はインスタンスを用意します。(必要なければ削除)
    // lgfx::Light_PWM _light_instance;

    // // タッチスクリーンの型にあったインスタンスを用意します。(必要なければ削除)
    // lgfx::Touch_GT911 _touch_instance;

public:
    // コンストラクタを作成し、ここで各種設定を行います。
    // クラス名を変更した場合はコンストラクタも同じ名前を指定してください。
    LGFX(void)
    {
        {                                      // バス制御の設定を行います。
            auto cfg = _bus_instance.config(); // バス設定用の構造体を取得します。

            // SPIバスの設定
            cfg.spi_host = SPI2_HOST; // 使用するSPIを選択  ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
            // ※ ESP-IDFバージョンアップに伴い、VSPI_HOST , HSPI_HOSTの記述は非推奨になるため、エラーが出る場合は代わりにSPI2_HOST , SPI3_HOSTを使用してください。
            cfg.spi_mode = 0;                  // SPI通信モードを設定 (0 ~ 3)
            cfg.freq_write = 40000000;         // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
            cfg.freq_read = 16000000;          // 受信時のSPIクロック
            cfg.spi_3wire = false;             // 受信をMOSIピンで行う場合はtrueを設定
            cfg.use_lock = true;               // トランザクションロックを使用する場合はtrueを設定
            cfg.dma_channel = SPI_DMA_CH_AUTO; // 使用するDMAチャンネルを設定 (0=DMA不使用 / 1=1ch / 2=ch / SPI_DMA_CH_AUTO=自動設定)
            // ※ ESP-IDFバージョンアップに伴い、DMAチャンネルはSPI_DMA_CH_AUTO(自動設定)が推奨になりました。1ch,2chの指定は非推奨になります。
            cfg.pin_sclk = 21;            // SPIのSCLKピン番号を設定
            cfg.pin_mosi = 20;            // SPIのMOSIピン番号を設定
            cfg.pin_miso = GPIO_NUM_NC;   // SPIのMISOピン番号を設定 (-1 = disable)
            cfg.pin_dc = 18; // SPIのD/Cピン番号を設定  (-1 = disable)
                                          // SDカードと共通のSPIバスを使う場合、MISOは省略せず必ず設定してください。

            _bus_instance.config(cfg);              // 設定値をバスに反映します。
            _panel_instance.setBus(&_bus_instance); // バスをパネルにセットします。
        }

        {                                        // 表示パネル制御の設定を行います。
            auto cfg = _panel_instance.config(); // 表示パネル設定用の構造体を取得します。

            cfg.pin_cs = 9;             // CSが接続されているピン番号   (-1 = disable)
            cfg.pin_rst = 19;           // RSTが接続されているピン番号  (-1 = disable)
            cfg.pin_busy = GPIO_NUM_NC; // BUSYが接続されているピン番号 (-1 = disable)

            // ※ 以下の設定値はパネル毎に一般的な初期値が設定されていますので、不明な項目はコメントアウトして試してみてください。

            cfg.panel_width = 240;    // 実際に表示可能な幅
            cfg.panel_height = 320;   // 実際に表示可能な高さ
            cfg.offset_x = 0;         // パネルのX方向オフセット量
            cfg.offset_y = 0;         // パネルのY方向オフセット量
            cfg.offset_rotation = 0;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
            cfg.dummy_read_pixel = 8; // ピクセル読出し前のダミーリードのビット数
            cfg.dummy_read_bits = 1;  // ピクセル以外のデータ読出し前のダミーリードのビット数
            cfg.readable = true;      // データ読出しが可能な場合 trueに設定
            cfg.invert = false;       // パネルの明暗が反転してしまう場合 trueに設定
            cfg.rgb_order = true;     // パネルの赤と青が入れ替わってしまう場合 trueに設定
            cfg.dlen_16bit = false;   // 16bitパラレルやSPIでデータ長を16bit単位で送信するパネルの場合 trueに設定
            cfg.bus_shared = true;    // SDカードとバスを共有している場合 trueに設定(drawJpgFile等でバス制御を行います)

            // 以下はST7735やILI9163のようにピクセル数が可変のドライバで表示がずれる場合にのみ設定してください。
            //    cfg.memory_width     =   240;  // ドライバICがサポートしている最大の幅
            //    cfg.memory_height    =   320;  // ドライバICがサポートしている最大の高さ

            _panel_instance.config(cfg);
        }

        //     //*
        //     {                                        // バックライト制御の設定を行います。（必要なければ削除）
        //         auto cfg = _light_instance.config(); // バックライト設定用の構造体を取得します。

        //         cfg.pin_bl = GPIO_NUM_46; // バックライトが接続されているピン番号
        //         cfg.invert = false;       // バックライトの輝度を反転させる場合 true
        //         cfg.freq = 44100;         // バックライトのPWM周波数
        //         cfg.pwm_channel = 7;      // 使用するPWMのチャンネル番号

        //         _light_instance.config(cfg);
        //         _panel_instance.setLight(&_light_instance); // バックライトをパネルにセットします。
        //     }
        //     //*/

        //     //*
        //     { // タッチスクリーン制御の設定を行います。（必要なければ削除）
        //         auto cfg = _touch_instance.config();

        //         cfg.x_min = 0;             // タッチスクリーンから得られる最小のX値(生の値)
        //         cfg.x_max = 239;           // タッチスクリーンから得られる最大のX値(生の値)
        //         cfg.y_min = 0;             // タッチスクリーンから得られる最小のY値(生の値)
        //         cfg.y_max = 319;           // タッチスクリーンから得られる最大のY値(生の値)
        //         cfg.pin_int = GPIO_NUM_45; // INTが接続されているピン番号
        //         cfg.bus_shared = true;     // 画面と共通のバスを使用している場合 trueを設定
        //         cfg.offset_rotation = 0;   // 表示とタッチの向きのが一致しない場合の調整 0~7の値で設定

        //         // I2C接続の場合
        //         cfg.i2c_port = 1;          // 使用するI2Cを選択 (0 or 1)
        //         cfg.i2c_addr = 0x5D;       // I2Cデバイスアドレス番号
        //         cfg.pin_sda = GPIO_NUM_47; // SDAが接続されているピン番号
        //         cfg.pin_scl = GPIO_NUM_48; // SCLが接続されているピン番号
        //         cfg.freq = 400000;         // I2Cクロックを設定

        //         _touch_instance.config(cfg);
        //         _panel_instance.setTouch(&_touch_instance); // タッチスクリーンをパネルにセットします。
        //     }
        //     //*/

        setPanel(&_panel_instance); // 使用するパネルをセットします。
    }
};

// Refer: https://web.archive.org/web/20250918152441/https://blog.csdn.net/weixin_41589183/article/details/137756695

uint32_t count = ~0;

static LGFX lcd;
static LGFX_Sprite sprite(&lcd);

void lgfx_test(void)
{
    lcd.init();

    lcd.setColorDepth(1);
    sprite.setColorDepth(1);
    lcd.setTextSize((std::max(lcd.width(), lcd.height()) + 255) >> 8);
    lcd.fillScreen(TFT_BLACK);

    int i = 0;
    while (1)
    {
        printf("Count: [%d]\n", i);
        i++;
        vTaskDelay(pdMS_TO_TICKS(1000));

        lcd.printf("%d ", i);
    }

    // while (1)
    // {
    //     lcd.startWrite();
    //     lcd.setRotation(++count & 7);
    //     lcd.setColorDepth((count & 8) ? 16 : 24);

    //     lcd.setTextColor(TFT_BLACK);
    //     lcd.drawNumber(lcd.getRotation(), 16, 0);

    //     lcd.setTextColor(0xFF0000U);
    //     lcd.drawString("R", 25, 16);
    //     lcd.setTextColor(0x00FF00U);
    //     lcd.drawString("G", 32, 16);
    //     lcd.setTextColor(0x0000FFU);
    //     lcd.drawString("B", 39, 16);
    //     lcd.setTextColor(0x0467FFU);
    //     lcd.drawString("JCZN", 25, 3);
    //     lcd.drawRect(30, 30, lcd.width() - 60, lcd.height() - 60, count * 7);
    //     lcd.drawFastHLine(0, 0, 10);

    //     lcd.endWrite();

    //     int32_t x, y;
    //     if (lcd.getTouch(&x, &y))
    //     {
    //         lcd.fillRect(x - 2, y - 2, 5, 5, count * 7);
    //     }

    //     vTaskDelay(pdMS_TO_TICKS(10));
    // }
}
