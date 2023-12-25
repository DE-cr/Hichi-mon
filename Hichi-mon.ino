/*
 * Hichi power monitor (https://github.com/DE-cr/Hichi-mon)
 * using esp32 with (optional) ssd1306 display
 * (file Hichi-mon.ino)
*/

//-- config:

#include "config.h"

#define HichiMonVersion "0.1.1"
#define HELLO "-- Welcome to Hichi-mon v" HichiMonVersion "! --"

#ifdef FULL_DAY_DISPLAY
#define BIN_WIDTH_S ( 24*60*60 / DATA_SIZE )  // 24 hours display
#else
#define BIN_WIDTH_S (    10*60 / DATA_SIZE )  // 10 minutes display
#endif

#define OLED_TYPE U8G2_SSD1306_128X64_NONAME_F_HW_I2C
#define OLED_ROTATION U8G2_R3
#define OLED_FONT u8g2_font_ncenR08_tf // u8g2_font_helvR12_te
#define TEMP_FMT "%.1f"

#define UPDATE_DROPBOX_TOKEN_AT ":17" // when hh:mm ends like this

#define OTA_UPDATE_PORT 8080

//-- libs:

#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Update.h>

//-- vars:

OLED_TYPE oled( OLED_ROTATION, U8X8_PIN_NONE );
WiFiServer server( 80 );

#include "bmp_header.h"

#define DATA_SIZE (24*5)  // 5 bins/h => bin width = 12 min
typedef struct {
  int sum;
  int cnt;
  int avg;
} t_log_data;
t_log_data log_data[ DATA_SIZE ];
int prev_pos = -1;

char time_now[12], time_prev[12];
char date_now[11], date_prev[11];
char date_h_now[14], date_h_prev[14];
#define MAX_LINE_LEN 14 // e.g.: "00:00:00 -123\n"
char recent_line[ MAX_LINE_LEN + 1 ];
#define MAX_LOG_SIZE ( MAX_LINE_LEN * 60 * 60 + 1 )
char complete_log[ MAX_LOG_SIZE ];
unsigned long log_size = 0;

#define DROPBOX_TOKEN_SERVER "api.dropbox.com"
#define DROPBOX_CONTENT_SERVER "content.dropboxapi.com"
#define CHUNK_SIZE (8*1024)
#define TOKEN_INTRO "{\"access_token\": \""
#define MAX_TOKEN_LENGTH 150 // the ones I've seen are 139 chars each
char dropbox_access_token[ MAX_TOKEN_LENGTH + 1 ];
bool access_token_ok = false;

WebServer update_server( OTA_UPDATE_PORT );
const char* update_server_index =
  "<form method='POST' action='/update' enctype='multipart/form-data'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>";

//-- code:

int power_update( int *power ) {
  WiFiClient client;
  if ( !client.connect( HICHI_ADDRESS, 80 ) ) return false;
  client.println( "GET /?m HTTP/1.1" );
  client.println( String( "Host: " ) + HICHI_ADDRESS );
  client.println( );
  // Serial.println( client.readStringUntil( '\n' ) );
  if ( !client.find( "Leistung {m}" ) ) return false;
  *power = client.parseInt( );
  return true;
}

int find_min( t_log_data* data ) {
  int min = 0;
  int i;
  for ( i = 0; i < DATA_SIZE - 1; ++i )
    if ( data[i].cnt ) {
      min = data[i].avg;
      break;
    }
  for ( ++i; i < DATA_SIZE  ; ++i )
    if ( data[i].cnt  &&  data[i].avg < min )
      min = data[i].avg;
  return min;
}

int find_max( t_log_data* data ) {
  int max = 0;
  int i;
  for ( i = 0; i < DATA_SIZE - 1; ++i )
    if ( data[i].cnt ) {
      max = data[i].avg;
      break;
    }
  for ( ++i; i < DATA_SIZE  ; ++i )
    if ( data[i].cnt  &&  data[i].avg > max )
      max = data[i].avg;
  return max;
}

void update_log_data( t_log_data* data, int val ) {
  data->avg = ( data->sum += val ) / ++ data->cnt;
}

void draw_log( t_log_data* data, int pos, int y_offset ) {
  int p, x1, x2, y1, y2, n = 0;
  int min = find_min( data );
  int max = find_max( data );
  if ( min == max ) return;  // constant value, don't know how to scale
  for ( int i = 0; i < DATA_SIZE - 1; ++i ) {
    p = ( pos + DATA_SIZE - i ) % DATA_SIZE;
    if ( data[ p ].cnt == 0 )
      continue;  // no data here, try next bin
    x1 = 127 - i;  // most recent is on the very right
    #define MAXY 63
    y1 = y_offset + MAXY - (long)( data[p].avg - min ) * MAXY / ( max - min );
    if ( n++ )  // only after more than one point found can we draw a line
      // oled.drawLine( x1, y1, x2, y2 );
      oled.drawLine( MAXY-y1, x1, MAXY-y2, x2 );
    x2 = x1;
    y2 = y1;
  }
}

unsigned char* compile_bitmap( ) {
  static unsigned char buf[ BMP_SIZE ];
  unsigned char* pbuf = buf;
  for ( int i = 0; i < BMP_HEADER_SIZE; ++i )
    *pbuf++ = bmp_header[i];
  unsigned char* p = oled.getBufferPtr( );
  int tw = oled.getBufferTileWidth( );
  int th = oled.getBufferTileHeight( );
  for ( int y = 0; y < th * 8; ++y )
    for ( int x = 0; x < tw * 8; x += 8 ) {
      unsigned char v = 0;
      for ( int b = 0; b < 8; ++b ) {
        v <<= 1;
        if ( u8x8_capture_get_pixel_1( x + b, th * 8 - 1 - y, p, tw ) )
          v |= 1;
      }
      *pbuf++ = ~v;  // black on white looks better in BMP
    }
  return buf;
}

bool updateDropboxToken( ) {
#ifndef USE_DROPBOX
  return false;
#endif
  bool success = false;
  Serial.print( "UpdateDropboxToken:" );
  WiFiClientSecure client;
  client.setInsecure( );
  String request = "grant_type=refresh_token&refresh_token=";
  request += DROPBOX_REFRESH_TOKEN;
  if ( client.connect( DROPBOX_TOKEN_SERVER, 443 ) ) {
    client.print( "POST /1/oauth2/token HTTP/1.1\r\n"
                  "Host: " DROPBOX_TOKEN_SERVER "\r\n"
                  "Authorization: Basic dGh4azg0ZGp4ZG45b3gyOmg5NTRoaGM2MDN4YnpyYQ==\r\n"
                  "Accept-Encoding: identity\r\n"
                  "Content-Type: application/x-www-form-urlencoded\r\n"
                  "Content-Length: " );
    client.print( request.length( ) );
    client.print( "\r\n\r\n" );
    client.print( request );
    while ( client.connected( ) && ! client.available( ) ) delay( 10 );
    String response = client.connected() ? client.readStringUntil('\n') : "diconnected";
    Serial.println( response );
    success = response == "HTTP/1.1 200 OK\r";
    while ( client.connected( ) ) // skip rest of header
      if ( client.readStringUntil( '\n' ) == "\r" )
        break;
    String line = client.readStringUntil( '\n' );
    success = line.startsWith( TOKEN_INTRO );
    if ( success ) {
      const char* p_line = line.c_str( ) + strlen( TOKEN_INTRO );
      char* p_token = dropbox_access_token;
      for ( int n = 0;  *p_line && *p_line != '"' && n < MAX_TOKEN_LENGTH;  ++n )
        *p_token++ = *p_line++;
      *p_token = 0;
    } else {
      Serial.print( ':' );
      Serial.println( line );
    }
    // we don't bother extracting the "expires_in" value from line
    while ( client.available() ) client.read();
    client.stop( );
  } else Serial.println( "connectFailed" );
  return success;
}

bool readFromDropbox( const char* path, const char* basename, const char* extension,
                      byte* data, unsigned long &size, unsigned long max_size, unsigned retries ) {
#ifndef USE_DROPBOX
  return false;
#endif
  if ( !access_token_ok ) return false;
  bool success = false;
  for (;;) {
    String fn = String( path ) + basename + extension;
    Serial.print( String( "<Dropbox:" ) + fn + ':' );
    WiFiClientSecure client;
    client.setInsecure();
    if ( client.connect( DROPBOX_CONTENT_SERVER, 443 ) ) {
      client.print( "POST /2/files/download HTTP/1.1\r\n"
                    "Host: " DROPBOX_CONTENT_SERVER "\r\n"
                    "Accept-Encoding: identity\r\n"
                    "Authorization: Bearer " );
      client.print( dropbox_access_token );
      client.print( "\r\n"
                    "Dropbox-API-Arg: {\"path\":\"" );
      client.print( fn );
      client.print( "\"}\r\n\r\n" );
      while ( client.connected( ) && ! client.available( ) ) delay( 10 );
      String response = client.connected() ? client.readStringUntil('\n') : "diconnected";
      Serial.println( response );
      success = response == "HTTP/1.1 200 OK\r";
      unsigned long data_size = 0;
      while ( client.connected( ) ) { // process rest of header
        String line = client.readStringUntil( '\n' );
        sscanf( line.c_str( ), "Content-Length: %lu", &data_size );
        if ( line == "\r" ) break;
      }
      if ( success )
        for ( size = 0;  client.connected() && size < data_size && size < max_size;  ++size, ++data ) {
          // WAIT for client data, just to be safe:
          while ( client.connected( ) && ! client.available( ) ) delay( 10 );
          *data = client.read( );
        }
      client.stop( );
      Serial.print( size );
      Serial.print( '/' );
      Serial.println( data_size );
    } else Serial.println( "connectFailed" );
    if ( success || !(retries--) ) return success;
    delay( 1000 );
  }
}

bool send2dropbox( const char* path, const char* basename, const char* extension,
                   const byte* data, unsigned long size, unsigned retries ) {
#ifndef USE_DROPBOX
  return false;
#endif
  if ( !access_token_ok ) return false;
  bool success = false;
  for (;;) {
    String fn = String( path ) + basename + extension;
    Serial.print( String( ">Dropbox:" ) + fn + ':' );
    WiFiClientSecure client;
    client.setInsecure();
    if ( client.connect( DROPBOX_CONTENT_SERVER, 443 ) ) {
      client.print( "POST /2/files/upload HTTP/1.1\r\n"
                    "Host: " DROPBOX_CONTENT_SERVER "\r\n"
                    "Authorization: Bearer " );
      client.print( dropbox_access_token );
      client.print( "\r\n"
                    "Dropbox-API-Arg: {\"path\":\"" );
      client.print( fn );
      client.print( "\",\"mode\":\"overwrite\"}\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Content-Length: " );
      client.print( size );
      client.print( "\r\n\r\n" );
      while ( size ) {
        unsigned n = size > CHUNK_SIZE ? CHUNK_SIZE : size;
        client.write( data, n );
        data += n;
        size -= n;
      }
      while ( client.connected( ) && ! client.available( ) ) delay( 10 );
      String response = client.connected() ? client.readStringUntil('\n') : "diconnected";
      Serial.println( response );
      success = response == "HTTP/1.1 200 OK\r";
      while ( client.available() ) client.read();
      client.stop( );
    } else Serial.println( "connectFailed" );
    if ( success || !(retries--) ) return success;
    delay( 1000 );
  }
}

void init_ota_update( ) {
  update_server.on( "/", HTTP_GET, []() {
    update_server.sendHeader( "Connection", "close" );
    update_server.send( 200, "text/html", update_server_index );
  });
  update_server.on( "/update", HTTP_POST, []() {
    update_server.sendHeader( "Connection", "close" );
    update_server.send( 200, "text/plain", Update.hasError( ) ? "Failed" : "Success" );
    if ( log_size )
      send2dropbox( DROPBOX_PATH, date_h_prev, ".csv", (byte*)complete_log, log_size, 2 );
    ESP.restart( );
  }, []() {
    HTTPUpload& upload = update_server.upload( );
    if ( upload.status == UPLOAD_FILE_START ) {
      Serial.println( "Updating ESP32 firmware..." );
      if ( ! Update.begin( UPDATE_SIZE_UNKNOWN ) ) // start with max available size
        Update.printError( Serial );
    } else if ( upload.status == UPLOAD_FILE_WRITE ) {
      if ( Update.write( upload.buf, upload.currentSize ) != upload.currentSize )
        Update.printError( Serial );
    } else if ( upload.status == UPLOAD_FILE_END ) {
      if ( Update.end( true ) ) // true to set the size to the current progress
        Serial.println( "Update success, rebooting..." );
      else
        Update.printError( Serial );
    }
  });
  update_server.begin( );
  Serial.print( "Update server started on port " );
  Serial.println( OTA_UPDATE_PORT );
}

void update_time( ) {
  time_t now;
  if ( time( &now ) < 50 * 365ul * 24 * 60 * 60 ) return;  // before ca. 2020?
  strftime( date_now, 11, "%F", localtime( &now ) );
  strftime( time_now, 12, "%T", localtime( &now ) );
}

void setup( ) {
  // setCpuFrequencyMhz( 80 );  // 240->80 MHz = approx. -20% power consumption
  Serial.begin( 115200 );
  Serial.println( HELLO );
  memset( log_data, 0, DATA_SIZE * sizeof( t_log_data ) );
  oled.begin( );
  oled.enableUTF8Print( );
  oled.setFont( OLED_FONT );
  oled.setDrawColor( 1 );
  WiFi.mode( WIFI_STA );
  WiFi.begin( MY_SSID, MY_PASSWORD );
  server.begin( );
  *date_h_prev = *date_prev = *time_prev = 0;
  *dropbox_access_token = 0;
  init_ota_update( );
  configTime( 0, 0, NTP_SERVER );
  setenv( "TZ", LOCAL_TIMEZONE, 1 );
  tzset( );
  Serial.println( "setup() done." );
}

void loop( ) {
  unsigned long ms = millis( );
  int pos = ms              // overflow after ca. 54 d ignored!
            / 1000          // => s
            / BIN_WIDTH_S   // => bin
            % DATA_SIZE;    // overflow => wrap around
  if ( pos != prev_pos ) {  // entering new log bin? => reset contents
    Serial.println( String( "pos=" ) + pos );
    memset( &log_data[pos], 0, sizeof( t_log_data ) );
    prev_pos = pos;
  }
  update_time( );
  if ( *date_now != '2' ) return;
  if ( !*time_now ) return;
  if ( !strcmp( time_now, time_prev ) ) return;
  static int power = 0;
  power_update( &power );  // (try to) read current power consumption from HICHI_URL
  update_log_data( &log_data[pos], power );
  if ( !access_token_ok ||
       !strncmp( time_now + 5 - strlen( UPDATE_DROPBOX_TOKEN_AT ),
                 UPDATE_DROPBOX_TOKEN_AT,
                 strlen( UPDATE_DROPBOX_TOKEN_AT ) ) )
    // Dropbox access tokens usually expire after 4h, but this is easier for us to handle
    access_token_ok = updateDropboxToken( );
  strcpy( date_h_now, date_now );
  strcat( date_h_now, "_" );
  strncat( date_h_now, time_now, 2 );
  bool new_hm = strncmp( time_now, time_prev, 5 );
  bool new_day_h = strcmp( date_h_now, date_h_prev );
  if ( log_size && new_hm ) {
    if ( ! strncmp( time_now + 5 - strlen( DROPBOX_BMP_WRITE_TIME_PATTERN ),
                    DROPBOX_BMP_WRITE_TIME_PATTERN,
                    strlen( DROPBOX_BMP_WRITE_TIME_PATTERN ) ) )
      send2dropbox( DROPBOX_PATH, "hichi-mon", ".bmp", compile_bitmap( ), BMP_SIZE, 0 );
    if ( new_hm &&
         ( new_day_h ||
           ! strncmp( time_now + 3, "00", 2 ) ||  // new hour?
           ! strncmp( time_now + 5 - strlen( DROPBOX_CSV_WRITE_TIME_PATTERN ),
                      DROPBOX_CSV_WRITE_TIME_PATTERN,
                      strlen( DROPBOX_CSV_WRITE_TIME_PATTERN ) ) ) )
      send2dropbox( DROPBOX_PATH, date_h_prev, ".csv", (byte*)complete_log, log_size, new_day_h ? 2 : 0 );
  } // log_size && new_hm
  if ( new_day_h ) {
    Serial.println( date_h_now );
    log_size = *complete_log = 0; // clear log for new day (even if the last send2dropbox failed!)
    // any data to restore from dropbox?:
    readFromDropbox( DROPBOX_PATH, date_h_now, ".csv", (byte*)complete_log, log_size, MAX_LOG_SIZE, 0 );
  }
  sprintf( recent_line, "%s %d\n", time_now, power );
  Serial.print( recent_line );
  int n = strlen( recent_line );
  if ( log_size + n > MAX_LOG_SIZE )
    Serial.println( "log_full" );
  else {
    strcat( complete_log + log_size, recent_line );
    log_size += n;
  }
  Serial.print( "log_size=" );
  Serial.println( log_size );
  strcpy( date_h_prev, date_h_now );
  strcpy( date_prev, date_now );
  strcpy( time_prev, time_now );
  oled.clearBuffer( );
  draw_log( log_data, pos, 0 );
  oled.setCursor( 0, 8 );
  char text[19];
  sprintf( text, "%d %d %d", find_min(log_data), power, find_max(log_data) );
  oled.print( text );
  oled.sendBuffer( );
  //- HTTP client request?:
  // (takes about 35 ms on my system)
  WiFiClient client = server.available( );
  if ( client ) {
    unsigned long my_ms = millis( );
    unsigned char buf[ 999 ];  // to hold a single line from client
    unsigned char* pbuf = buf;
    // don't wait forever:
    while ( client.connected( ) && millis( ) - my_ms < 100 )
      if ( client.available( ) ) {
        if ( ( *pbuf  = client.read( ) ) == '\n' )
          break;  // we're not interested in what comes after the first \n
        Serial.write( *pbuf );
        *++pbuf = '\0';
        if ( ! strcmp( (char*)buf, "GET / " ) ) {
          // ready to go, forget about the rest
          client.println( "HTTP/1.1 200 OK" );
          client.println( "Content-Type: image/bmp" );
          client.println( "Connection: close" );
          client.println( );
          client.write( compile_bitmap( ), BMP_SIZE );
          break;
        }
      }
    client.stop( );
    Serial.println( );
  }
  //-
  update_server.handleClient( );
}
