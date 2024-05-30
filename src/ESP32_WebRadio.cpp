#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include "AudioBoard.h"
#include "OneButton.h"
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header
#include "BluetoothA2DPSink.h"
// WiFi SSID and PWD definition...
#include "mysecret.h"
hd44780_I2Cexp lcd; // declare lcd object: auto locate & auto config expander chip
// LCD geometry
const int LCD_COLS = 20;
const int LCD_ROWS = 4;
unsigned long iUltimaAccensioneDisplay = 0;
unsigned int iTimeoutDisplay = 5000;
bool bLcdFatalError = false;
/*Bluetooth a2dp section*/
I2SStream i2s;
BluetoothA2DPSink *a2dp_sink;

#define DEBUGGAME

//Audiokit i2s pin definition
#define I2S_DOUT      26 //35
#define I2S_BCLK      27
#define I2S_LRC       25
Audio *audio;
//OneButton KEY_1(36), KEY_2(13), KEY_3(19), KEY_4(23), KEY_5(18), *KEY_6;//(5);
OneButton *KEY_1, *KEY_2, *KEY_3, *KEY_4, *KEY_5, *KEY_6;
enum MachineStates {
    STATE_INIT,
    STATE_WAITWIFICONNECTION,
    STATE_RADIO,
    STATE_INITA2DP,
    STATE_BLUETOOTSPEAKER
};
MachineStates currentState = STATE_INIT;
unsigned int iInitialVolume = 25;

void nextStation();
void prevStation();
void volumeDown();
void volumeUp();
void setTone();
void printOnLcd(int idx, const char *info = NULL);
void changeMode();
void avrc_metadata_callback(uint8_t data1, const uint8_t *data2);
void logSuSeriale(const __FlashStringHelper *frmt, ...);

#define IDX_LAST_STATIONS 9
static int i_stationIdx = -1;
const char *stationsName[] = {
  PROGMEM("Radio Deejay"),
  PROGMEM("Virgin Radio"),
  PROGMEM("Controradio"), //Controradio  
  PROGMEM("Deejay 80"),
  PROGMEM("Deejay On The Road"),
  PROGMEM("Tropical Pizza"),
  PROGMEM("Mitology"), //Mytology
  PROGMEM("RTL 102.5"),
  PROGMEM("Radio 105"),
  PROGMEM("Subasio")
};
/******************************************************/
/*Use this link to fin streams: https://streamurl.link*/
/******************************************************/
const char *stationUrls[] = {
  PROGMEM("http://streamcdnb1-4c4b867c89244861ac216426883d1ad0.msvdn.net/radiodeejay/radiodeejay/play1.m3u8"),
  PROGMEM("http://icy.unitedradio.it/Virgin.mp3"),
  //PROGMEM("https://s4.yesstreaming.net/proxy/contror1/stream"), //Controradio  
  PROGMEM("http://streaming.controradio.it:8190/"), //Controradio  
  //PROGMEM("http://s4.yesstreaming.net:7199/stream"), //Controradio  
  PROGMEM("http://streamcdnf25-4c4b867c89244861ac216426883d1ad0.msvdn.net/webradio/deejay80/live.m3u8"),
  PROGMEM("http://streamcdnm5-4c4b867c89244861ac216426883d1ad0.msvdn.net/webradio/deejayontheroad/live.m3u8"),
  PROGMEM("http://streamcdnm12-4c4b867c89244861ac216426883d1ad0.msvdn.net/webradio/deejaytropicalpizza/live.m3u8"),
  PROGMEM("http://onair15.xdevel.com/proxy/radiocatsdogs2?mp=/;stream/;"), //Mytology
  //PROGMEM("http://streamcdnc2-dd782ed59e2a4e86aabf6fc508674b59.msvdn.net/live/S97044836/chunklist_b128000.m3u8"),
  PROGMEM("https://streamingv2.shoutcast.com/rtl-1025_48.aac"),
  PROGMEM("http://icecast.unitedradio.it/Radio105.mp3"),
  PROGMEM("http://icy.unitedradio.it/Subasio.mp3")
};
void setup() {
#ifdef DEBUGGAME  
    Serial.begin(115200);
    while (!Serial) {
    ;
    }
#endif    
    LOGLEVEL_AUDIODRIVER = AudioDriverError;
    logSuSeriale(F("*****************************\n"));
    logSuSeriale(F("Total Falsh: %d\n"), ESP.getFlashChipSize());
    logSuSeriale(F("Total heap: %d\n"), ESP.getHeapSize());
    logSuSeriale(F("Free heap: %d\n"), ESP.getFreeHeap());
    logSuSeriale(F("Total PSRAM: %d\n"), ESP.getPsramSize());
    logSuSeriale(F("Free PSRAM: %d-%d\n"), ESP.getFreePsram(), UINT16_MAX);
    logSuSeriale(F("ESP32 Chip model:  %s Rev %d\n"), ESP.getChipModel(), ESP.getChipRevision());
    logSuSeriale(F("SdkVersion:        %s\n"), ESP.getSdkVersion());
    logSuSeriale(F("*****************************\n"));
    CodecConfig cfg;
    cfg.input_device = audio_driver::ADC_INPUT_NONE;
    cfg.output_device = audio_driver::DAC_OUTPUT_ALL;
    cfg.i2s.bits = audio_driver::BIT_LENGTH_16BITS;
    cfg.i2s.rate = audio_driver::RATE_44K;
    cfg.i2s.fmt = audio_driver::I2S_NORMAL;
    // get current pin value 
    auto i2c_opt = AudioKitEs8388V1.getPins().getI2CPins(PinFunction::CODEC);

    if (i2c_opt)
    {   //Setting Wire1 for internal use, Wire so is available on pin 21-22 for external use. Lcd in this case
        auto i2c = i2c_opt.value();
        i2c.p_wire = &Wire1;
        // update infrmation
        if (!AudioKitEs8388V1.getPins().setI2C(i2c))
        {
            Serial.println("Failed to set Wire1 for internal use!");
        }
    }
    
    AudioKitEs8388V1.begin(cfg); 

    
    /*auto pinne = pins.value();
    int i = pinne.pin;*/
    auto pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 1);
    KEY_1 = new OneButton(pins.value().pin);
    KEY_1->attachClick(prevStation);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 2);
    KEY_2 = new OneButton(pins.value().pin);    
    KEY_2->attachClick(nextStation);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 3);
    KEY_3 = new OneButton(pins.value().pin);    
    KEY_3->attachClick(volumeDown);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 4);
    KEY_4 = new OneButton(pins.value().pin);    
    KEY_4->attachClick(volumeUp);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 5);
    KEY_5 = new OneButton(pins.value().pin);    
    KEY_5->attachClick(setTone);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 6);
    KEY_6 = new OneButton(pins.value().pin);
    KEY_6->attachClick(changeMode);

    int status = lcd.begin(LCD_COLS, LCD_ROWS);
	if(status) // non zero status means it was unsuccesful
	{
        bLcdFatalError =true;
	} else
    {
        lcd.noBacklight(); //spengiamo lo lcd
        lcd.lineWrap();
    }
   // initOtaUpdateSubsistem();
    
//  *** local files ***
//  audio.connecttoFS(SD, "/test.wav");     // SD
//  audio.connecttoFS(SD_MMC, "/test.wav"); // SD_MMC
//  audio.connecttoFS(SPIFFS, "/test.wav"); // SPIFFS
}

void loop()
{
    
    switch (currentState)
    {
    case STATE_INIT:
        Serial.println("Mode init!");
        WiFi.enableSTA(true); //Needed to switch on WIFI?
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid_1, password_1);
        iInitialVolume = 25;
        AudioKitEs8388V1.setVolume(iInitialVolume);
        
        audio = new Audio;
        audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, 0);
        audio->setVolume(21);
        i_stationIdx = -1;
        currentState = STATE_WAITWIFICONNECTION;
        break;
    case STATE_WAITWIFICONNECTION:
        //Serial.println("Mode STATE_WAITWIFICONNECTION!");
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("Move to STATE_RADIO!");
            currentState = STATE_RADIO;
            nextStation();
        }
        yield();
        break;
    case STATE_RADIO:
            //Serial.println("Mode radio on!");
            audio->loop();
            if ((millis() - iUltimaAccensioneDisplay) > iTimeoutDisplay)
            {
                lcd.noBacklight();
                iUltimaAccensioneDisplay = millis();
            }
            break;
    case STATE_INITA2DP:
            a2dp_sink = new BluetoothA2DPSink(i2s);
            //a2dp_sink->set_pin_config(my_pin_config);
            a2dp_sink->set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE); //| ESP_AVRC_MD_ATTR_PLAYING_TIME);
            a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
            a2dp_sink->start("ESP32_Speaker");
            //AudioKitEs8388V1.setInputVolume(100);
            Serial.println("Mode bluetooth speaker");
            iInitialVolume = 100;
            AudioKitEs8388V1.setVolume(iInitialVolume);
            currentState = STATE_BLUETOOTSPEAKER;
        break;
    default:
        break;
    }
    KEY_1->tick();
    KEY_2->tick();
    KEY_3->tick();
    KEY_4->tick();
    KEY_5->tick();
    KEY_6->tick();
}
void setTone()
{
    //etTone (int8_t gainLowPass, int8_t gainBandPass, int8_t gainHighPass);
    //possible values ​​are -40 ... 6 [dB]. The first tests were successful.
    static int iToneStatus = 0;
    switch (iToneStatus)
    {
        case 0:
            // statements
            audio->setTone(0, -12, -24);
            Serial.printf("%d\n", iToneStatus);
            iToneStatus = 1;
            break;
        case 1:
            audio->setTone(0, 0, 0);
            Serial.printf("%d\n", iToneStatus);
            iToneStatus = 0;
            break;
        default:
             break;
    // statements
    }
    //Serial.printf("%d\n", iBass);
}
void volumeDown()
{
    if (AudioKitEs8388V1.getVolume() > 0)
        iInitialVolume -= 5;
    AudioKitEs8388V1.setVolume(iInitialVolume);
    printOnLcd(i_stationIdx);
}
void volumeUp()
{
    //audio.setTone(10, -10,-10);
    if (AudioKitEs8388V1.getVolume() < 100)
        iInitialVolume += 5;
    AudioKitEs8388V1.setVolume(iInitialVolume);
    printOnLcd(i_stationIdx);
}
void prevStation()
{
    switch (currentState)
    { 
        case STATE_RADIO:  
            i_stationIdx--;
            if (i_stationIdx < 0)
                i_stationIdx = IDX_LAST_STATIONS;
            Serial.printf("Station %d-%s\n", i_stationIdx, stationsName[i_stationIdx]);
            if(!audio->connecttospeech(stationsName[i_stationIdx], "it")) // Google TTS
            {
                Serial.println("Entro qui!");
                audio->connecttohost(stationUrls[i_stationIdx]);
            }
            printOnLcd(i_stationIdx);
            break;
        case STATE_BLUETOOTSPEAKER:
            if (a2dp_sink->get_audio_state() != ESP_A2D_AUDIO_STATE_STARTED)
                a2dp_sink->play();
            else
                a2dp_sink->stop();
            break;
        default:
            break;
    }
        
} 
void nextStation()
{
    switch (currentState)
    { 
        case STATE_RADIO:  
            if (i_stationIdx < IDX_LAST_STATIONS)
                i_stationIdx++;
            else
                i_stationIdx = 0;
            Serial.printf("Station %d-%s\n", i_stationIdx, stationsName[i_stationIdx]);
            if(!audio->connecttospeech(stationsName[i_stationIdx], "it")) // Google TTS
                audio->connecttohost(stationUrls[i_stationIdx]);
            printOnLcd(i_stationIdx);
            break;
        case STATE_BLUETOOTSPEAKER:
            break;
        default:
            break;
    }
} 

void printOnLcd(int idx, const char* info)
{
    if (!bLcdFatalError)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("Staz.:%02d", idx + 1);
        lcd.setCursor(0, 1);
        if (!strstr(audio->getCodecname(), "unkn"))
        {
            lcd.printf("%s-%s", stationsName[idx], audio->getCodecname());
        }
        else
            lcd.printf("%s", stationsName[idx]);
        lcd.setCursor(10, 0);
        lcd.printf("Vol.: %d%%", AudioKitEs8388V1.getVolume());
        iUltimaAccensioneDisplay = millis();
        lcd.backlight();
        if (info)
        {
            String sInfo = info;
            String sAppo = "";
            int iIndx = 0;
            sInfo.trim();
            //sInfo.replace('~', ' ');
            lcd.setCursor(0, 2);
            iIndx = sInfo.indexOf('~');
            if (iIndx > 0)
            {
                sAppo = sInfo.substring(0, iIndx);
                lcd.printf("%s", sAppo.c_str());
                lcd.setCursor(0, 3);
                sAppo = sInfo.substring(iIndx+1, iIndx + 20);
                iIndx = sAppo.indexOf('~');
                lcd.printf("%s", sAppo.substring(0, iIndx).c_str());
                return;
            }
            iIndx = sInfo.indexOf('-');
            if (iIndx > 0)
            {
                sAppo = sInfo.substring(0, iIndx);
                lcd.printf("%s", sAppo.c_str());
                lcd.setCursor(0, 3);
                sAppo = sInfo.substring(iIndx+1, iIndx + 20);
                sAppo.trim();
                lcd.printf("%s", sAppo.substring(0, 19).c_str());
                return;
            }else
                lcd.printf("%s", sInfo.substring(0, 39).c_str());
        }
    }
} 
void audio_info(const char*info)
{
    logSuSeriale(F("audio_info %s-%s\n"), info, audio->getCodecname());
}

void audio_showstreamtitle(const char* info)
{
    if (strlen(info))
        Serial.printf("showstreamtitle %s-%s\n", info, audio->getCodecname());
    printOnLcd(i_stationIdx, info);
}
void audio_icydescription(const char* info)
{
    //Serial.printf("icydescription %s\n", info);
}
void audio_commercial(const char* info)
{
    //Serial.printf("commercial %s\n", info);
}

void audio_eof_speech(const char*info)
{
    Serial.println("End of speech!");
    audio->connecttohost(stationUrls[i_stationIdx]);
}

void changeMode()
{
    if (currentState != STATE_BLUETOOTSPEAKER) //Not state BTSpeaker then move to it
    {
        currentState = STATE_INITA2DP;
        WiFi.disconnect(true, true);
        delete audio;
        delay(1000);
    }
    else //BT speaker mode so move to radio
    {
        currentState = STATE_INIT;
        a2dp_sink->disconnect();
        a2dp_sink->end(true);
        delete a2dp_sink;
        delay(1000);
    }
}
void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  logSuSeriale(F("AVRC metadata rsp: attribute id 0x%x, %s\n"), data1, data2);
}

void logSuSeriale(const __FlashStringHelper *frmt, ...) {
#ifdef DEBUGGAME
  va_list args;
  va_start(args, frmt);
  static uint const MSG_BUF_SIZE = 256;
  char msg_buf[MSG_BUF_SIZE] = {0};
  vsnprintf_P(msg_buf, MSG_BUF_SIZE, (const char *)frmt, args);
  Serial.print(msg_buf);
  va_end(args);
#endif
}