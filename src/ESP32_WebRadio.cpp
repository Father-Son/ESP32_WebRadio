#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include "AudioBoard.h"
#include "OneButton.h"
#include <hd44780.h>                       // main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c expander i/o class header
#include "BluetoothA2DPSink.h"
#include "bledefinitions.h"
// WiFi SSID and PWD definition...

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
WiFiServer *server;
#define DEBUGGAME

//Audio task definitions
TaskHandle_t AudioTaskHandle;
enum uieCommand{ SET_VOLUME, GET_VOLUME, NEXT_STATION, PREV_STATION, CHANGE_MODE, SET_BASS, SEET_MID, SET_HIGH};

struct audioMessage{
    uieCommand  cmd;
    const char* txt1;
    const char* txt2;
    const char* txt3;
    uint8_t    value1;
    uint16_t    value2;
    uint8_t    ret;
} audioTxMessage, //Sent from loop to audiotask
  audioRxTaskMessage,//received from audiotask sent by loop
  audioTxTaskMessage, //Sent from audiotask to loop
  audioRxMessage; //received from loop sent by audiotask
enum enButtonMode{
    BTN_MODE_VOLUME,
    BTN_MODE_EQUALIZER
} btnMode;

QueueHandle_t LoopToAudioQueue = NULL;
QueueHandle_t AudioToLoopQueue = NULL;

//Audiokit i2s pin definition
#define I2S_DOUT      26 //35
#define I2S_BCLK      27
#define I2S_LRC       25
#define I2S_MCLK      0
Audio *audio;
//OneButton KEY_1(36), KEY_2(13), KEY_3(19), KEY_4(23), KEY_5(18), *KEY_6;//(5);
OneButton *KEY_1, *KEY_2, *KEY_3, *KEY_4, *KEY_5, *KEY_6;
enum MachineStates {
    STATE_INIT,
    STATE_WAITWIFICONNECTION,
    STATE_INITBLE,
    STATE_WIFICONF,
    STATE_RADIO,
    STATE_INITA2DP,
    STATE_BLUETOOTSPEAKER
};
MachineStates currentState = STATE_INIT;
unsigned int iInitialVolume = 50;

void nextStation();
void prevStation();
void volumeDown();
void volumeUp();
void setBtnMode();
void printOnLcd(int idx, const char *info = NULL);
void changeMode();
void avrc_metadata_callback(uint8_t data1, const uint8_t *data2);
void logSuSeriale(const __FlashStringHelper *frmt, ...);
void audioInit(const char * urlStation);
void audioTask(void *parameter);
int readFile(const char * path);
#define IDX_LAST_STATIONS 14
static int i_stationIdx = 0;
const char *stationsName[] = {
  PROGMEM("Virgin radio"),
  PROGMEM("Virgin rock 80"),
  PROGMEM("Virgin rock 90"),
  PROGMEM("Virgin classic rock"),
  PROGMEM("Virgin radio queen"),
  PROGMEM("Virgin radio AC-DC"),
  PROGMEM("Radio Deejay"),
  PROGMEM("Deejay 80"),
  PROGMEM("Deejay On The Road"),
  PROGMEM("Tropical Pizza"),
  PROGMEM("Mitology"), 
  PROGMEM("RTL 102.5"),
  PROGMEM("Radio 105"),
  PROGMEM("Subasio"),
  PROGMEM("Controradio"), 
};
/******************************************************/
/*Use this link to fin streams: https://streamurl.link*/
/******************************************************/
const char *stationUrls[] = {
  PROGMEM("http://icy.unitedradio.it/Virgin.mp3"),
  PROGMEM("http://icy.unitedradio.it/VirginRock80.mp3"),
  PROGMEM("http://icy.unitedradio.it/Virgin_03.mp3"),
  PROGMEM("http://icy.unitedradio.it/VirginRockClassics.mp3"),
  PROGMEM("http://icy.unitedradio.it/Virgin_05.mp3"),
  PROGMEM("http://icy.unitedradio.it/um1026.mp3"), 
  PROGMEM("http://streamcdnb1-4c4b867c89244861ac216426883d1ad0.msvdn.net/radiodeejay/radiodeejay/play1.m3u8"),
  PROGMEM("http://streamcdnf25-4c4b867c89244861ac216426883d1ad0.msvdn.net/webradio/deejay80/live.m3u8"),
  PROGMEM("http://streamcdnm5-4c4b867c89244861ac216426883d1ad0.msvdn.net/webradio/deejayontheroad/live.m3u8"),
  PROGMEM("http://streamcdnm12-4c4b867c89244861ac216426883d1ad0.msvdn.net/webradio/deejaytropicalpizza/live.m3u8"),
  PROGMEM("http://onair15.xdevel.com/proxy/radiocatsdogs2?mp=/;stream/;"), //Mytology
  //PROGMEM("http://streamcdnc2-dd782ed59e2a4e86aabf6fc508674b59.msvdn.net/live/S97044836/chunklist_b128000.m3u8"),
  PROGMEM("https://streamingv2.shoutcast.com/rtl-1025_48.aac"),
  PROGMEM("http://icecast.unitedradio.it/Radio105.mp3"),
  PROGMEM("http://icy.unitedradio.it/Subasio.mp3"),
  PROGMEM("http://streaming.controradio.it:8190/;?type=http&nocache=76494"), //Controradio 
};

SemaphoreHandle_t mutex_updating;
void setup() {
#ifdef DEBUGGAME  
    Serial.begin(115200);
    while (!Serial) {
    ;
    }
    mutex_updating = xSemaphoreCreateMutex();
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
    btnMode = BTN_MODE_VOLUME;
    CodecConfig cfg;
    cfg.input_device = audio_driver::ADC_INPUT_NONE;
    cfg.output_device = audio_driver::DAC_OUTPUT_ALL;
    cfg.i2s.bits = audio_driver::BIT_LENGTH_16BITS;
    cfg.i2s.rate = audio_driver::RATE_44K;
    cfg.i2s.fmt = audio_driver::I2S_NORMAL;
    cfg.sd_active = false;
    // get current pin value 
    auto i2c_opt = AudioKitEs8388V1.getPins().getI2CPins(PinFunction::CODEC);
    
    if (i2c_opt)
    {   //Setting Wire1 for internal use, Wire so is available on pin 21-22 for external use. Lcd in this case
        auto i2c = i2c_opt.value();
        i2c.p_wire = &Wire1;
        // update infrmation
        if (!AudioKitEs8388V1.getPins().setI2C(i2c))
        {
            logSuSeriale(F("Failed to set Wire1 for internal use!"));
        }
    }
 
/*        // define custom SPI pins
    SPI.begin(14, 2, 15, 13);

    // intialize SD
    if(!SD.begin(13)){   
        logSuSeriale(F("SD.Begin failed...\n"));
        //return;
    }
    readFile("/config.bin");
*/
    AudioKitEs8388V1.begin(cfg);    
    auto spi_opt = AudioKitEs8388V1.getPins().getSPIPins(PinFunction::SD);
    if (spi_opt)
    {
        auto spi = spi_opt.value();
        
        if (!SD.begin(spi.cs, *(spi.p_spi)))
            logSuSeriale(F("SD.Begin failed...%d\n"), spi.cs);
    }
    //readFile("/config.bin");
    /*auto pinne = pins.value();
    int i = pinne.pin;*/
    auto pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 1);
    KEY_1 = new OneButton(pins.value().pin);
    KEY_1->attachClick(prevStation);
    KEY_1->attachDoubleClick(nextStation);
   /* pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 2);
    KEY_2 = new OneButton(pins.value().pin);    
    KEY_2->attachClick(nextStation);*/
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 3);
    KEY_3 = new OneButton(pins.value().pin);    
    KEY_3->attachClick(volumeDown);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 4);
    KEY_4 = new OneButton(pins.value().pin);    
    KEY_4->attachClick(volumeUp);
    pins = AudioKitEs8388V1.getPins().getPin(PinFunction::KEY, 5);
    KEY_5 = new OneButton(pins.value().pin);    
    KEY_5->attachClick(setBtnMode);
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
// Variable to store the HTTP request
String header;
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

int readFile(const char * path) {
  logSuSeriale(F("Reading file\n"));
  memset(ssid, 0, 32);
  memset(pswd, 0, 63);
  File file = SD.open(path, "r");
  if (!file) {
    logSuSeriale(F("Failed to open file for reading"));
    return 1;
  }
  if (file.available())
  {
    strcpy(ssid, (char*)file.readStringUntil('\r').c_str());
  }
  file.readStringUntil('\n');
  //ssid[15]=0;
  if (file.available())
  {
    strcpy(pswd, (char*)file.readStringUntil('\r').c_str());
  }
  file.readStringUntil('\n');
  //pswd[16]=0;
  file.close();
  return 0;
}
int writeFile(const char * path) {
  logSuSeriale(F("Writing file\n"));

  File file = SD.open("/config.bin", "w");
  if (!file) {
    logSuSeriale(F("Failed to open file for writing\n"));
    return 1;
  }
  file.println(ssid);
  file.println(pswd);
  delay(2000); // Make sure the CREATE and LASTWRITE times are different
  file.close();
  return 0;
}

void loop()
{
    static uint8_t ui8ConnTentative = 0;
    switch (currentState)
    {
    case STATE_INIT:
        logSuSeriale(F("Mode init!"));
        WiFi.disconnect(true, true);
        WiFi.enableSTA(true); //Needed to switch on WIFI?
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower(WIFI_POWER_7dBm);
        readFile("/config.bin");
        logSuSeriale(F(ssid));
        logSuSeriale(F(pswd));
        WiFi.begin(ssid, pswd);
        AudioKitEs8388V1.setVolume(iInitialVolume);
        currentState = STATE_WAITWIFICONNECTION;
        break;
    case STATE_WAITWIFICONNECTION:
        //Serial.println("Mode STATE_WAITWIFICONNECTION!");
        if (WiFi.status() == WL_CONNECTED)
        {
            logSuSeriale(F("Move to STATE_RADIO!"));
            audioInit(stationUrls[i_stationIdx]);
            ui8ConnTentative = 0;
            currentState = STATE_RADIO;
        }
        else
        {
            ui8ConnTentative++;
            delay(100);
        }
        if (ui8ConnTentative > 100)
        {        
            logSuSeriale(F("Failed to connect move to ap mode\n"));
            ui8ConnTentative = 0; //reset tentative counter...
            currentState = STATE_INITBLE; //file to connect move to ap mode servng a page for inpunt credential....
        }
        break;
    case STATE_INITBLE:
    {
        WiFi.disconnect(true, true);
        BLEDevice::init("ESP32RADIO_WIFICONFIGURATOR");
        BLEServer *pServer = BLEDevice::createServer();
        BLEService *pService = pServer->createService(UUID_SERVICE_MYWIFI);
        pServer->setCallbacks(new MyServerCallbacks());
        BLECharacteristic *pCharacteristicSsid = pService->createCharacteristic(UUID_CHARACTERISTIC_MYWIFI_SSID, BLECharacteristic::PROPERTY_WRITE);
        BLECharacteristic *pCharacteristicPwd = pService->createCharacteristic(UUID_CHARACTERISTIC_MYWIFI_PASS, BLECharacteristic::PROPERTY_WRITE);
        pCharacteristicSsid->setCallbacks(new SSID_Callbacks());
        pCharacteristicPwd->setCallbacks(new PWD_Callbacks());
        //pCharacteristic->setValue("Hello World");
        pService->start();

        BLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
        memset(ssid, 0, 32);
        memset(pswd, 0, 63);
        currentState = STATE_WIFICONF;
        break;
    }
    case STATE_WIFICONF:
    {
       if (strlen(ssid) && strlen(pswd) && !deviceConnected)
       {
            logSuSeriale(F(ssid));
            logSuSeriale(F(pswd));
            BLEDevice::deinit(true);
           // writeFile("/config.bin");
            ESP.restart();
       }
       //currentState = STATE_INIT;        
       break;
    }
    case STATE_RADIO:
            //Serial.println("Mode radio on!");
            if ((millis() - iUltimaAccensioneDisplay) > iTimeoutDisplay)
            {
                lcd.noBacklight();
                iUltimaAccensioneDisplay = millis();
            }
            break;
    case STATE_INITA2DP:
    {
            a2dp_sink = new BluetoothA2DPSink(i2s);
            auto cfg = i2s.defaultConfig();    
            cfg.pin_bck = I2S_BCLK;
            cfg.pin_ws = I2S_LRC;
            cfg.pin_data = I2S_DOUT;
            cfg.pin_mck = I2S_MCLK;
            i2s.begin(cfg);
            //a2dp_sink->set_pin_config(my_pin_config);
            a2dp_sink->set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE); //| ESP_AVRC_MD_ATTR_PLAYING_TIME);
            a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
            a2dp_sink->start("ESP32_Speaker");
            //AudioKitEs8388V1.setInputVolume(100);
            logSuSeriale(F("Mode bluetooth speaker"));
            iInitialVolume = 70;
            AudioKitEs8388V1.setVolume(iInitialVolume);
            currentState = STATE_BLUETOOTSPEAKER;
        break;
    }
    default:
        break;
    }
    KEY_1->tick();
    //KEY_2->tick();
    KEY_3->tick();
    KEY_4->tick();
    KEY_5->tick();
    KEY_6->tick();
}

void setBtnMode()
{
    if (btnMode == BTN_MODE_VOLUME)
        btnMode = BTN_MODE_EQUALIZER;
    else
        btnMode = BTN_MODE_VOLUME;
    return;
    static int iToneStatus = 0;
    switch (iToneStatus)
    {
        case 0:
            // statements
            audio->setTone(2, 0, -1);
            logSuSeriale(F("%d\n"), iToneStatus);
            iToneStatus = 1;
            break;
        case 1:
            audio->setTone(0, 0, 0);
            logSuSeriale(F("%d\n"), iToneStatus);
            iToneStatus = 0;
            break;
        default:
             break;
    // statements
    }
    //Serial.printf("%d\n", iBass);
}
void changeMode()
{
    if (currentState != STATE_BLUETOOTSPEAKER) //Not state BTSpeaker then move to it
    {
        logSuSeriale(F("Changing state\n"));
        currentState = STATE_INITA2DP;
        WiFi.disconnect(true, true);
        audioTxMessage.cmd = CHANGE_MODE;
        xQueueSend(LoopToAudioQueue, &audioTxMessage, portMAX_DELAY);
        delay(1000);
    }
    else //BT speaker mode so move to radio
    {
        currentState = STATE_INIT;        
        a2dp_sink->disconnect();
        a2dp_sink->end(true);
        delete a2dp_sink;
        ESP.restart();

    }
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
    if (btnMode == BTN_MODE_VOLUME)
    {
        if (AudioKitEs8388V1.getVolume() < 100)
            iInitialVolume += 5;
        AudioKitEs8388V1.setVolume(iInitialVolume);
    }
    else
    {
        audioTxMessage.cmd = SET_BASS;
        xQueueSend(LoopToAudioQueue, &audioTxMessage, portMAX_DELAY);
    }
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
            logSuSeriale(F("Station %d-%s\n"), i_stationIdx, stationsName[i_stationIdx]);
            audioTxMessage.cmd = PREV_STATION;
            audioTxMessage.txt1 = stationUrls[i_stationIdx];
            audioTxMessage.txt2 = stationsName[i_stationIdx];
            xQueueSend(LoopToAudioQueue, &audioTxMessage, portMAX_DELAY);
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
    logSuSeriale(F("NextStation"));
    switch (currentState)
    { 
        case STATE_RADIO:  
            if (i_stationIdx < IDX_LAST_STATIONS)
                i_stationIdx++;
            else
                i_stationIdx = 0;
            logSuSeriale(F("Station %d-%s\n"), i_stationIdx, stationsName[i_stationIdx]);
            audioTxMessage.cmd = NEXT_STATION;
            audioTxMessage.value1 = i_stationIdx;
            audioTxMessage.txt1 = stationUrls[i_stationIdx];
            audioTxMessage.txt2 = stationsName[i_stationIdx];
            xQueueSend(LoopToAudioQueue, &audioTxMessage, portMAX_DELAY);
            printOnLcd(i_stationIdx);
            break;
        case STATE_BLUETOOTSPEAKER:
            break;
        default:
            break;
    }
} 

//Begin audio thread section

void CreateQueues(){
    LoopToAudioQueue = xQueueCreate(2, sizeof(struct audioMessage));
    AudioToLoopQueue = xQueueCreate(2, sizeof(struct audioMessage));
}
void audioInit(const char * urlStation)
{
    CreateQueues();
    audio = new Audio;
    audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
    //audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio->setVolumeSteps(35);
    audio->setVolume(35); // due to call to upper call 0...35!
//    values can be between -40 ... +6 (dB)

    audio->setTone(6, 0, -3);
    if (!audio->connecttohost(urlStation))
    {
        ESP.restart();
    }
    // aac
    logSuSeriale(F("init %s\n"),urlStation);
    xTaskCreatePinnedToCore(
        audioTask,          /* Function to implement the task */
        "audioplay",        /* Name of the task */
        7500,               /* Stack size in words */
        NULL,               /* Task input parameter */
        2,                  /* Priority of the task */
        &AudioTaskHandle,   /* Task handle. */
        0                   /* Core where the task should run */
    );
}

void audioTask(void *parameter)
{
  while (true)
  {
    if(xQueueReceive(LoopToAudioQueue, &audioRxTaskMessage, 1) == pdPASS)
    {
      if (audioRxTaskMessage.cmd == NEXT_STATION || audioRxTaskMessage.cmd == PREV_STATION)
      {
        audio->connecttospeech(audioRxTaskMessage.txt2, "it");
      }
      if (audioRxTaskMessage.cmd == CHANGE_MODE)
      {
        audio->stopSong();
        break;
      }
      if (audioRxTaskMessage.cmd == SET_BASS)
      {
        audio->setTone(0, 0, -3);
      }
    }
    audio->loop();
    vTaskDelay(7); //Necessario??
  }
  logSuSeriale(F("Deleting audio task....\n"));
  vTaskDelete( NULL );
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
   //logSuSeriale(F("audio_info %s-%s\n"), info, audio->getCodecname());
}

void audio_showstreamtitle(const char* info)
{
    if (strlen(info))
        logSuSeriale(F("showstreamtitle %s-%s\n"), info, audio->getCodecname());
    //printOnLcd(i_stationIdx, info);
}
void audio_icydescription(const char* info)
{
    logSuSeriale(F("icydescription %s\n"), info);
}
void audio_commercial(const char* info)
{
    //Serial.printf("commercial %s\n", info);
}

void audio_eof_speech(const char*info)
{
    //Serial.println("End of speech!");
    logSuSeriale(F("End of speech %s-%s\n"), audioRxTaskMessage.txt2, audioRxTaskMessage.txt1);
    if(!audio->connecttohost(audioRxTaskMessage.txt1))
        ESP.restart();
}


void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  //logSuSeriale(F("AVRC metadata rsp: attribute id 0x%x, %s\n"), data1, data2);
}

void logSuSeriale(const __FlashStringHelper *frmt, ...) {
#ifdef DEBUGGAME
  xSemaphoreTake(mutex_updating, portMAX_DELAY);
  va_list args;
  va_start(args, frmt);
  static uint const MSG_BUF_SIZE = 256;
  char msg_buf[MSG_BUF_SIZE] = {0};
  vsnprintf_P(msg_buf, MSG_BUF_SIZE, (const char *)frmt, args);
  Serial.print(msg_buf);
  va_end(args);
  xSemaphoreGive(mutex_updating);
#endif
}