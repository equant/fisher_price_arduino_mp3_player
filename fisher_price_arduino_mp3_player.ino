/***********************************************************/
// Code to turn a 2017 fisher price "tape" player into an mp3 player.
// https://github.com/equant/fisher_price_arduino_mp3_player
//
// Hardware:
// Arduino Pro Mini
// MP3 Serial Board (YX5300)
//
// Previous Credits:
// Project's codebase originally started from github project
// "ArduinoSerialMP3Player" with the following credits in the code...
//
// Demo for the Serial MP3 Player Catalex (YX5300 chip)
// http://www.dx.com/p/uart-control-serial-mp3-music-player-module-for-arduino-avr-arm-pic-blue-silver-342439#.VfHyobPh5z0
//


#include <SoftwareSerial.h>

#define ARDUINO_RX 5  //should connect to TX of the Serial MP3 Player module
#define ARDUINO_TX 6  //connect to RX of the module

#define PIN_VOL_A9  2   // A9 of Fisher Price volume rotary encoder
#define PIN_VOL_A10 3   // A10 of Fisher Price volume rotary encoder
//#define PIN_PAUSE   4
#define PIN_PLAY    7
#define PIN_PREV    8
#define PIN_NEXT    9
#define PIN_REC    10
#define PIN_MIC    11   // Switch on microphone
#define PIN_TAPE_1 12   // Switch 1 inside tape housing
#define PIN_TAPE_2 13   // Switch 2 inside tape housing

#define MAX_VOLUME    30
#define VOL_STEP_SIZE  3

SoftwareSerial mp3(ARDUINO_RX, ARDUINO_TX);

static int8_t Send_buf[8] = {0}; // Buffer for Send commands.  // BETTER LOCALLY
static uint8_t ansbuf[10] = {0}; // Buffer for the answers.    // BETTER LOCALLY

String mp3Answer;           // Answer from the MP3.

String sanswer(void);
String sbyte2hex(uint8_t b);

int currentVolume = MAX_VOLUME;
int currentDir    = 1;
boolean isPaused  = false;
boolean isPlaying = false;

/************ Command byte **************************/
#define CMD_NEXT_SONG     0X01  // Play next song.
#define CMD_PREV_SONG     0X02  // Play previous song.
#define CMD_PLAY_W_INDEX  0X03
#define CMD_VOLUME_UP     0X04
#define CMD_VOLUME_DOWN   0X05
#define CMD_SET_VOLUME    0X06

#define CMD_SNG_CYCL_PLAY 0X08  // Single Cycle Play.
#define CMD_SEL_DEV       0X09
#define CMD_SLEEP_MODE    0X0A
#define CMD_WAKE_UP       0X0B
#define CMD_RESET         0X0C
#define CMD_PLAY          0X0D
#define CMD_PAUSE         0X0E
#define CMD_PLAY_FOLDER_FILE 0X0F

#define CMD_STOP_PLAY     0X16  // Stop playing continuously. 
#define CMD_FOLDER_CYCLE  0X17
#define CMD_SHUFFLE_PLAY  0x18 //
#define CMD_SET_SNGL_CYCL 0X19 // Set single cycle.

#define CMD_SET_DAC 0X1A
#define DAC_ON  0X00
#define DAC_OFF 0X01

#define CMD_PLAY_W_VOL    0X22
#define CMD_PLAYING_N     0x4C
#define CMD_QUERY_STATUS      0x42
#define CMD_QUERY_VOLUME      0x43
#define CMD_QUERY_FLDR_TRACKS 0x4e
#define CMD_QUERY_TOT_TRACKS  0x48
#define CMD_QUERY_FLDR_COUNT  0x4f

/************ Opitons **************************/
#define DEV_TF            0X02


/*********************************************************************/

void setup()
{
  Serial.begin(9600);
  mp3.begin(9600);

  pinMode(PIN_VOL_A9,INPUT_PULLUP); 
  pinMode(PIN_VOL_A10,INPUT_PULLUP); 
  //pinMode(PIN_PAUSE,INPUT_PULLUP); 
  pinMode(PIN_PLAY,INPUT_PULLUP); 
  pinMode(PIN_PREV,INPUT_PULLUP); 
  pinMode(PIN_NEXT,INPUT_PULLUP); 
  pinMode(PIN_REC,INPUT_PULLUP); 
  pinMode(PIN_MIC,INPUT_PULLUP); 
  pinMode(PIN_TAPE_1,INPUT_PULLUP); 
  pinMode(PIN_TAPE_2,INPUT_PULLUP); 

  delay(200);
  sendCommand(CMD_SEL_DEV, 0, DEV_TF);
  checkTape(false);
  delay(200);
  //sendCommand(CMD_RESET);
  //delay(150);
  delay(150);
  delay(150);
  sendCommand(CMD_SET_VOLUME, 0, currentVolume);
  //delay(150);
  //delay(150);
  //delay(150);
  //sendCommand(CMD_FOLDER_CYCLE, 1, 0);
  //delay(150);
  //sendCommand(CMD_NEXT_SONG);
}


void loop()
{
  char c = ' ';

  // If there a char on Serial call sendMP3Command to sendCommand
  if ( Serial.available() )
  {
    c = Serial.read();
    sendMP3Command(c);
  }

  // Check for the answer.
  if (mp3.available())
  {
    Serial.println(decodeMP3Answer());
  }

  checkButtons();
  checkVolume();
  checkTape(true);
  delay(100);
}

void checkButtons(){

  /* PLAY */

  int val = digitalRead(PIN_PLAY);
  if (val == LOW) {
      if (isPaused) {
        Serial.println("PRESSED PLAY");
        sendCommand(CMD_PLAY);
        isPaused  = false;
        isPlaying = true;
        delay(20);
      }
      else if (not isPlaying) {
        Serial.println("PRESSED PLAY");
        sendCommand(CMD_FOLDER_CYCLE, currentDir, 0);
        isPaused  = false;
        isPlaying = true;
        delay(20);
      }
  }
  else if (val == HIGH && isPlaying) {
      sendCommand(CMD_PAUSE);
      isPaused = true;
      delay(30);
  }

//  /* PAUSE */
//
//  val = digitalRead(PIN_PAUSE);
//  if (val == LOW) {
//    if (not isPaused) {
//      Serial.println("Should PAUSE");
//      sendCommand(CMD_PAUSE);
//      isPaused = true;
//      delay(30);
//    }
//  }


  val = digitalRead(PIN_NEXT);
  if (val == LOW) {
      Serial.println("PRESSED NEXT");
      sendCommand(CMD_NEXT_SONG);
      delay(50);
  }

  val = digitalRead(PIN_PREV);
  if (val == LOW) {
      Serial.println("PRESSED PREV");
      sendCommand(CMD_PREV_SONG);
      delay(50);
  }

}

void checkVolume() {
  int a9  = digitalRead(PIN_VOL_A9);
  int a10 = digitalRead(PIN_VOL_A10);
  int set_vol = 30;
  int step_size = MAX_VOLUME;

  // Keep in mind the pull-up means the pushbutton's logic is inverted. It goes
  // HIGH when it's open, and LOW when it's pressed.

  if (a9 == HIGH  && a10 == HIGH) { // Vol: 4
    set_vol = MAX_VOLUME;
  }
  else if (a9 == HIGH  && a10 == LOW) { // Vol: 3
    set_vol -= VOL_STEP_SIZE;
  }
  else if (a9 == LOW  && a10 == HIGH) { // Vol: 2
    set_vol -= (VOL_STEP_SIZE * 2);
  }
  else if (a9 == LOW  && a10 == LOW) { // Vol: 1
    set_vol -= (VOL_STEP_SIZE * 3);
  }

  if (currentVolume != set_vol) {
      Serial.println("Changing volume.");
      sendCommand(CMD_SET_VOLUME, 0, set_vol);
      currentVolume = set_vol;
  }

}

void checkTape(boolean doPlay) {
  int t1 = digitalRead(PIN_TAPE_1);
  int t2 = digitalRead(PIN_TAPE_2);
  int set_dir = 1;

  // When tape is out, both switches should be low

  if (t1 == LOW  && t2 == LOW) { // No tape.
    set_dir = 1;
  }
  else if (t1 == HIGH  && t2 == LOW) { // Side A
    set_dir = 2;
  }
  else if (t1 == LOW  && t2 == HIGH) { // Side B
    set_dir = 3;
  }
  else if (t1 == HIGH  && t2 == HIGH) { // Only happens if you reach in and press switches 
    set_dir = 4;
  }

  if (currentDir != set_dir && doPlay) {
      Serial.println("Changing folder!");
      currentDir = set_dir;
      sendCommand(CMD_FOLDER_CYCLE, currentDir, 0);    // folder 1
  }
  else if (not doPlay) {
      currentDir = set_dir;
  }

}

/********************************************************************************/
/*Function sendMP3Command: seek for a 'c' command and send it to MP3  */
/*Parameter: c. Code for the MP3 Command, 'h' for help.                                                                                                         */
/*Return:  void                                                                */

void sendMP3Command(char c) {
  
  switch (c) {
    case '?':
    case 'h':
      Serial.println("HELP  ");
      Serial.println(" p = Play");
      Serial.println(" P = Pause");
      Serial.println(" > = Next");
      Serial.println(" < = Previous");
      Serial.println(" s = Stop Play"); 
      Serial.println(" + = Volume UP");
      Serial.println(" - = Volume DOWN");
      Serial.println(" c = Query current file");
      Serial.println(" q = Query status");
      Serial.println(" v = Query volume");
      Serial.println(" x = Query folder count");
      Serial.println(" t = Query total file count");
      Serial.println(" f = Play folder 1.");
      Serial.println(" S = Sleep");
      Serial.println(" W = Wake up");
      Serial.println(" r = Reset");
      break;


    case 'p':
      Serial.println("Play ");
      sendCommand(CMD_PLAY);
      //sendCommand(CMD_SNG_CYCL_PLAY, 0, 1);
      break;

    case 'P':
      Serial.println("Pause");
      sendCommand(CMD_PAUSE);
      break;

    case '>':
      Serial.println("Next");
      sendCommand(CMD_NEXT_SONG);
      sendCommand(CMD_PLAYING_N); // ask for the number of file is playing
      break;

    case '<':
      Serial.println("Previous");
      sendCommand(CMD_PREV_SONG);
      sendCommand(CMD_PLAYING_N); // ask for the number of file is playing
      break;

    case 's':
      Serial.println("Stop Play");
      sendCommand(CMD_STOP_PLAY);
      break;


    case '+':
      Serial.println("Volume Up");
      sendCommand(CMD_VOLUME_UP);
      break;

    case '-':
      Serial.println("Volume Down");
      sendCommand(CMD_VOLUME_DOWN);
      break;

    case 'c':
      Serial.println("Query current file");
      sendCommand(CMD_PLAYING_N);
      break;

    case 'q':
      Serial.println("Query status");
      sendCommand(CMD_QUERY_STATUS);
      break;

    case 'v':
      Serial.println("Query volume");
      sendCommand(CMD_QUERY_VOLUME);
      break;

    case 'x':
      Serial.println("Query folder count");
      sendCommand(CMD_QUERY_FLDR_COUNT);
      break;

    case 't':
      Serial.println("Query total file count");
      sendCommand(CMD_QUERY_TOT_TRACKS);
      break;

    case 'f':
      Serial.println("Playing folder 1");
      //sendCommand(CMD_FOLDER_CYCLE, 1, 0);    // folder 1
      //sendCommand(CMD_FOLDER_CYCLE, 2, 0);    // folder 2
      break;

    case 'S':
      Serial.println("Sleep");
      sendCommand(CMD_SLEEP_MODE);
      break;

    case 'W':
      Serial.println("Wake up");
      sendCommand(CMD_WAKE_UP);
      break;

    case 'r':
      Serial.println("Reset");
      sendCommand(CMD_RESET);
      break;
  }
}



/********************************************************************************/
/*Function decodeMP3Answer: Decode MP3 answer.                                  */
/*Parameter:-void                                                               */
/*Return: The                                                  */

String decodeMP3Answer() {
  String decodedMP3Answer = "[A] ";

  decodedMP3Answer += sanswer();

  switch (ansbuf[3]) {
    case 0x3A:
      decodedMP3Answer += " -> Memory card inserted.";
      break;

    case 0x3D:
      decodedMP3Answer += " -> Completed play num " + String(ansbuf[6], DEC);
      break;

    case 0x40:
      decodedMP3Answer += " -> Error";
      break;

    case 0x41:
      decodedMP3Answer += " -> Data recived correctly. ";
      break;

    case 0x42:
      decodedMP3Answer += " -> Status playing: " + String(ansbuf[6], DEC);
      break;

    case 0x48:
      decodedMP3Answer += " -> File count: " + String(ansbuf[6], DEC);
      break;

    case 0x4C:
      decodedMP3Answer += " -> Playing: " + String(ansbuf[6], DEC);
      break;

    case 0x4E:
      decodedMP3Answer += " -> Folder file count: " + String(ansbuf[6], DEC);
      break;

    case 0x4F:
      decodedMP3Answer += " -> Folder count: " + String(ansbuf[6], DEC);
      break;
  }

  return decodedMP3Answer;
}






/********************************************************************************/
/*Function: Send command to the MP3                                             */
/*Parameter: byte command                                                       */
/*Parameter: byte dat1 parameter for the command                                */
/*Parameter: byte dat2 parameter for the command                                */

void sendCommand(byte command){
  sendCommand(command, 0, 0);
}

void sendCommand(byte command, byte dat1, byte dat2){
  delay(20);
  Send_buf[0] = 0x7E;    //
  Send_buf[1] = 0xFF;    //
  Send_buf[2] = 0x06;    // Len
  Send_buf[3] = command; //
  Send_buf[4] = 0x01;    // 0x00 NO, 0x01 feedback
  Send_buf[5] = dat1;    // datah
  Send_buf[6] = dat2;    // datal
  Send_buf[7] = 0xEF;    //
  Serial.print("Sending: ");
  for (uint8_t i = 0; i < 8; i++)
  {
    mp3.write(Send_buf[i]) ;
    Serial.print(sbyte2hex(Send_buf[i]));
  }
  Serial.println();
}



/********************************************************************************/
/*Function: sbyte2hex. Returns a byte data in HEX format.                       */
/*Parameter:- uint8_t b. Byte to convert to HEX.                                */
/*Return: String                                                                */


String sbyte2hex(uint8_t b)
{
  String shex;

  shex = "0X";

  if (b < 16) shex += "0";
  shex += String(b, HEX);
  shex += " ";
  return shex;
}


/********************************************************************************/
/*Function: shex2int. Returns a int from an HEX string.                         */
/*Parameter: s. char *s to convert to HEX.                                      */
/*Parameter: n. char *s' length.                                                */
/*Return: int                                                                   */

int shex2int(char *s, int n){
  int r = 0;
  for (int i=0; i<n; i++){
     if(s[i]>='0' && s[i]<='9'){
      r *= 16; 
      r +=s[i]-'0';
     }else if(s[i]>='A' && s[i]<='F'){
      r *= 16;
      r += (s[i] - 'A') + 10;
     }
  }
  return r;
}


/********************************************************************************/
/*Function: sanswer. Returns a String answer from mp3 UART module.          */
/*Parameter:- uint8_t b. void.                                                  */
/*Return: String. If the answer is well formated answer.                        */

String sanswer(void)
{
  uint8_t i = 0;
  String mp3answer = "";

  // Get only 10 Bytes
  while (mp3.available() && (i < 10))
  {
    uint8_t b = mp3.read();
    ansbuf[i] = b;
    i++;

    mp3answer += sbyte2hex(b);
  }

  // if the answer format is correct.
  if ((ansbuf[0] == 0x7E) && (ansbuf[9] == 0xEF))
  {
    return "?!?: " + mp3answer;
  }

  return "???: " + mp3answer;
}
