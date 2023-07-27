/*
 * Hichi power monitor (https://github.com/DE-cr/Hichi-mon)
 * using esp32 with (optional) ssd1306 display
 * (file config.h)
 */

//- WiFi credentials:
#define MY_SSID "MyWiFi"
#define MY_PASSWORD "MyPassword"

#define FULL_DAY_DISPLAY  // remove if you prefer a more detailed view of the most recent ten minutes only

//- timezone info (keep the variable names used here!)
// see https://github.com/JChristensen/Timezone for info on what values to provide within the {}:
TimeChangeRule DST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule STT = {"CET ", Last, Sun, Oct, 3,  60};     // Central European Standard Time

//- how to reach the Hichi reader on your smart meter:
#define HICHI_ADDRESS "hichi"  // name or ip address for Hichi server

//- define the following to periodically transfer screenshot and csv log file to your dropbox account:
#define USE_DROPBOX
// your token should be 64 chars!: 1234567890123456789012345678901234567890123456789012345678901234
#define DROPBOX_REFRESH_TOKEN     "--- Use 'Get_Dropbox_token_for_Hichi-mon.html' to get yours! ---"
// where to put files in your dropbox:
#define DROPBOX_PATH "/Hichi-mon/log/"  // must start and end with '/'!
// when to write files to dropbox (hh:mm ending like this? -> write!; "" = every minute, "-" = never):
#define DROPBOX_BMP_WRITE_TIME_PATTERN "2"  // keeping bmp and csv writing separate (not required)
#define DROPBOX_CSV_WRITE_TIME_PATTERN "7"  // will also be forced when entering a new hour or on OTA update!
