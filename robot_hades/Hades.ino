#include "Arduino.h"
#include "Oscillator.h"
#include <Servo.h>
#include "NeoSWSerial.h"
/*
         ---------------
        |     O   O     |
        |---------------|
YR 2<== |               | <== YL 3
         ---------------
            ||     ||
            ||     ||
RR 0==^   -----   ------  v== RL 1
         |-----   ------|
*/

/* Hardware interface mapping*/
#define YL_PIN 10
#define YR_PIN 9
#define RL_PIN 12
#define RR_PIN 6

#define ECHO_PIN 4
#define TRIG_PIN 5

#define ST188_R_PIN A1
#define ST188_L_PIN A0

#define MY1690_PIN 8
#define HT6871_PIN 7
#define SOFTWARE_RXD A2 
#define SOFTWARE_TXD A3


#define CENTRE 90
#define N_SERVOS 4
#define INTERVALTIME 5

Oscillator servo[N_SERVOS];
int t = 150;
int iswalking = 1;
int lastMusicState = -1;  // -1 means nothing has been played yet

unsigned long final_time;
unsigned long interval_time;
int           oneTime;
int           iteration;
float         increment[N_SERVOS];
int           oldPosition[] = { CENTRE, CENTRE, CENTRE, CENTRE };

#define time_percent 0.2             // 0.2
#define delay_hip 10                // 10
#define delay_feet delay_hip / 2    // 5
// #define iswalking 1
//#define hip_angle 20                // 20
//#define foot_angle 

bool delays(unsigned long ms)
{
  delay(ms);
  return false;
}

NeoSWSerial mp3Serial(SOFTWARE_RXD, SOFTWARE_TXD);


// Switch Music
// Since the underlying program has been written, the only program you need to
// write is: MP3.playSong(1, 20);
//
// There are 2 parameters of playSong():
// 1. The first parameter selects the music file number (from 1 to 1000).
//    - Files must be preset on the TF card.
//    - For example: "0001.mp3" represents the first song,
//                   "0002.mp3" represents the second song, etc.
// 2. The second parameter sets the volume (range: 0 to 30).
//
// To play the second song at the highest volume (30), follow these steps:
// 1. Copy a music file into the TF card.
// 2. Rename it to "0002.mp3".
// 3. Use the command: MP3.playSong(2, 30);
//
// To change the music, change it in setup, at the bottom of the script, restart needed after uploading code
//
// All code below is how the music works, change at your own risk :)
////////////////////////////////////////////////////////////////////

/*
    MP3:Implementation of MP3 Driver
*/
class MY1690_16S
{
  public:
      int volume;

      String playStatus[5] = {"0", "1", "2", "3", "4"}; // STOP PLAYING PAUSE FF FR

      void playSong(unsigned char num, unsigned char vol)
      {
          setVolume(vol);
          setPlayMode(1);
          CMD_SongSelet[4] = num;
          checkCode(CMD_SongSelet);
          mp3Serial.write(CMD_SongSelet, 7);
          delay(50);
      }
      String getPlayStatus()
      {
          mp3Serial.write(CMD_getPlayStatus, 5);
          delay(50);
          return getStatus();
      }
      String getStatus()
      {
          String statusMp3 = "";
          while (mp3Serial.available())
          {
              statusMp3 += (char)mp3Serial.read();
          }
          return statusMp3;
      }
      void stopPlay()
      {
          setPlayMode(4);
          mp3Serial.write(CMD_MusicStop, 5);
          delay(50);
      }
      void setVolume(unsigned char vol)
      {
          CMD_VolumeSet[3] = vol;
          checkCode(CMD_VolumeSet);
          mp3Serial.write(CMD_VolumeSet, 6);
          delay(50);
      }
      void volumePlus()
      {
          mp3Serial.write(CMD_VolumePlus, 5);
          delay(50);
      }
      void volumeDown()
      {
          mp3Serial.write(CMD_VolumeDown, 5);
          delay(50);
      }
      void setPlayMode(unsigned char mode)
      {
          CMD_PlayMode[3] = mode;
          checkCode(CMD_PlayMode);
          mp3Serial.write(CMD_PlayMode, 6);
          delay(50);
      }
      void checkCode(unsigned char *vs)
      {
          int val = vs[1];
          int i;

          for (i = 2; i < vs[1]; i++)
          {
              val = val ^ vs[i];
          }
          vs[i] = val;
      }
      void ampMode(int p, bool m)
      {
          pinMode(p, OUTPUT);
          if (m)
          {
              digitalWrite(p, HIGH);
          }
          else
          {
              digitalWrite(p, LOW);
          }
      }
      void init()
      {
          ampMode(HT6871_PIN, HIGH);
          stopPlay();
          volume = 15;
      }
  private:
      byte CMD_MusicPlay[5] = {0x7E, 0x03, 0x11, 0x12, 0xEF};
      byte CMD_MusicStop[5] = {0x7E, 0x03, 0x1E, 0x1D, 0xEF};
      byte CMD_MusicNext[5] = {0x7E, 0x03, 0x13, 0x10, 0xEF};
      byte CMD_MusicPrev[5] = {0x7E, 0x03, 0x14, 0x17, 0xEF};
      byte CMD_VolumePlus[5] = {0x7E, 0x03, 0x15, 0x16, 0xEF};
      byte CMD_VolumeDown[5] = {0x7E, 0x03, 0x16, 0x15, 0xEF};
      byte CMD_VolumeSet[6] = {0x7E, 0x04, 0x31, 0x00, 0x00, 0xEF};
      byte CMD_PlayMode[6] = {0x7E, 0x04, 0x33, 0x00, 0x00, 0xEF};
      byte CMD_SongSelet[7] = {0x7E, 0x05, 0x41, 0x00, 0x00, 0x00, 0xEF};
      byte CMD_getPlayStatus[5] = {0x7E, 0x03, 0x20, 0x23, 0xEF};
} mp3;

/*
    Setting the 90-degree position of the steering gear to make the penguin stand on its feet
*/
void homes(int millis_t)
{
  servo[0].SetPosition(90);
  servo[1].SetPosition(90);
  servo[2].SetPosition(90);
  servo[3].SetPosition(90);
  delay(millis_t);
}

bool moveNServos(int time, int newPosition[])
{
  for (int i = 0; i < N_SERVOS; i++)
  {
    increment[i] = ((newPosition[i]) - oldPosition[i]) / (time / INTERVALTIME);
  }

  final_time = millis() + time;
  iteration = 1;

  while (millis() < final_time)
  {
    interval_time = millis() + INTERVALTIME;
    oneTime = 0;

    while (millis() < interval_time)
    {
      if (oneTime < 1)
      {
        for (int i = 0; i < N_SERVOS; i++)
        {
          servo[i].SetPosition(oldPosition[i] + (iteration * increment[i]));
        }

        iteration++;
        oneTime++;
      }
    }
  }
  for (int i = 0; i < N_SERVOS; i++)
  {
    oldPosition[i] = newPosition[i];
  }
  return false;
}
/*
  Walking control realization:
*/

  // Variables base
  const int neutral = 90;    // 90
  const int hipRotMax = 35;   //35  // Giro máximo de cadera (adelante o atrás)
  const int hipRotMin = -35;  // -40 // Giro mínimo de cadera (hacia atrás)
  const int footUp    = 15;   // 15 // Levantar pie
  const int footDown  = -15;  // -15 // Bajar pie
  const int hipSwingSmall = 25; // 25
  const int hipSwingBig   = 30; // 30
  const int hipReturn     = 20; // 20

bool walk(int steps, int T, int dir)
{
  int move1[]  = { neutral, neutral + hipRotMax, neutral + footUp, neutral + footUp };
  int move2[]  = { neutral + hipSwingSmall, neutral + hipSwingBig, neutral + footUp, neutral + footUp };
  int move3[]  = { neutral + hipReturn, neutral + hipReturn, neutral + footDown, neutral + footDown };
  int move4[]  = { neutral - hipRotMax, neutral, neutral + footDown, neutral + footDown };
  int move5[]  = { neutral + hipRotMin, neutral - hipSwingBig, neutral + footDown, neutral + footDown };
  int move6[]  = { neutral - hipReturn, neutral - hipReturn, neutral + footUp, neutral + footUp };

  int move21[] = { neutral, neutral + hipRotMax, neutral + footDown, neutral + footDown };
  int move22[] = { neutral + hipSwingSmall, neutral + hipSwingBig, neutral + footDown, neutral + footDown };
  int move23[] = { neutral + hipReturn, neutral + hipReturn, neutral + footUp, neutral + footUp };
  int move24[] = { neutral - hipRotMax, neutral, neutral + footUp, neutral + footUp };
  int move25[] = { neutral + hipRotMin, neutral - hipSwingBig, neutral + footUp, neutral + footUp };
  int move26[] = { neutral - hipReturn, neutral - hipReturn, neutral + footDown, neutral + footDown };

  if (dir == 1)  //Walking forward
  {
    for (int i = 0; i < steps; i++)
    {
      if (moveNServos(T * time_percent, move1) || delays(t / delay_hip) \
      || moveNServos(T * time_percent, move2) || delays(t / delay_hip) \
      || moveNServos(T * time_percent, move3) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move4) || delays(t / 2) \
      || moveNServos(T * time_percent, move5) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move6) || delays(t / delay_feet))
        return true;
    }
  }
  else  //Walking backward
  {
    for (int i = 0; i < steps; i++)
    {
      if (moveNServos(T * time_percent, move21) || delays(t / delay_hip) \
      || moveNServos(T * time_percent, move22) || delays(t / delay_hip) \
      || moveNServos(T * time_percent, move23) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move24) || delays(t / 2) \
      || moveNServos(T * time_percent, move25) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move26))
        return true;
    }
  }
  return false;
}


bool turn(int steps, int T, int dir)
{
  // Variables base
  const int neutral = 90;

  // Offsets
  const int hipRotMax = 40; // 55
  const int hipRotMin = -20; // 20
  const int footUp = 20;    // 20
  const int footDown = -20; // 20

  // Movimiento hacia un lado
  int move1[] = { neutral - hipRotMax, neutral - hipRotMin, neutral + footUp, neutral + footUp };
  int move2[] = { neutral - hipRotMin, neutral - hipRotMin, neutral + footUp, neutral + footDown };
  int move3[] = { neutral + footUp, neutral + hipRotMax, neutral + footUp, neutral + footDown };
  int move4[] = { neutral + footUp, neutral + footUp, neutral + footDown, neutral + footUp };
  int move5[] = { neutral - hipRotMax, neutral - hipRotMin, neutral + footDown, neutral + footUp };
  int move6[] = { neutral - hipRotMin, neutral - hipRotMin, neutral + footUp, neutral + footDown };
  int move7[] = { neutral + footUp, neutral + hipRotMax, neutral + footUp, neutral + footDown };
  int move8[] = { neutral + footUp, neutral + footUp, neutral + footDown, neutral + footUp };

  // Movimiento hacia el otro lado
  int move21[] = { neutral + footUp, neutral + hipRotMax, neutral + footUp, neutral + footUp };
  int move22[] = { neutral + footUp, neutral + footUp, neutral + footUp, neutral + footDown };
  int move23[] = { neutral - hipRotMax, neutral - hipRotMin, neutral + footUp, neutral + footDown };
  int move24[] = { neutral - hipRotMin, neutral - hipRotMin, neutral + footDown, neutral + footDown };
  int move25[] = { neutral + footUp, neutral + hipRotMax, neutral + footDown, neutral + footUp };
  int move26[] = { neutral + footUp, neutral + footUp, neutral + footUp, neutral + footDown };
  int move27[] = { neutral - hipRotMax, neutral - hipRotMin, neutral + footUp, neutral + footDown };
  int move28[] = { neutral - hipRotMin, neutral - hipRotMin, neutral + footDown, neutral + footDown };

  // Aquí iría el resto del código para ejecutar los movimientos como en tu función original
  if (dir == 1)
  {
    for (int i = 0; i < steps; i++)
    {
      if (moveNServos(T * time_percent, move1) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move2) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move3) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move4) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move5) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move6) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move7) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move8) || delays(t / delay_feet))
        return true;
    }
  }
  else
  {
    for (int i = 0; i < steps; i++)
    {
      if (moveNServos(T * time_percent, move21) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move22) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move23) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move24) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move25) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move26) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move27) || delays(t / delay_feet) \
      || moveNServos(T * time_percent, move28) || delays(t / delay_feet))
        return true;
    }
  }
  return false;
}
/*Ultrasonic ranging*/
int getDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  return (int)pulseIn(ECHO_PIN, HIGH) / 58;
}
/*
Obstacle avoidance mode:
*/
// void obstacleMode()
// {
//   double distance_value = getDistance();
//   // int    iswalking = 0;

//   if (distance_value <= 20)
//   {
//     iswalking = 0;
//     // mp3Serial.begin(9600);
//     // mp3.init();
//     // mp3.setPlayMode(1);
//     // mp3.playSong(9, 20);
//     int st188Val_L = analogRead(ST188_L_PIN);
//     int st188Val_R = analogRead(ST188_R_PIN);

//     if (st188Val_L <= st188Val_R)
//       turn(1, t * 3, 1);
//     else
//       turn(1, t * 3, -1);
//   }
//   else
//   {
//     // mp3Serial.begin(9600);
//     // mp3.init();
//     // mp3.setPlayMode(1);
//     // mp3.playSong(1, 20);
//     iswalking = 1;
//     walk(1, t * 3, 1); 
//   }
// }
double getAverageDistance(int numSamples) {
  long sum = 0;
  for (int i = 0; i < numSamples; i++) {
    sum += getDistance();
    delay(10);  // Espera corta entre lecturas
  }
  return (double)sum / numSamples;
}

int getMaxDistance(int numSamples) {
  int maxValue = 0;
  for (int i = 0; i < numSamples; i++) {
    int d = getDistance();
    if (d > maxValue) {
      maxValue = d;
    }
    delay(10);  // Small pause between readings
  }
  return maxValue;
}

void obstacleMode()
{
  //double distance_value = getDistance();
  // double distance_value = getAverageDistance(7);  // Promedia 5 lecturas
  double distance_value;

  if (millis() < 3000) {
    distance_value = 50.0;  // Force value for first 5 seconds
  } else {
    distance_value = getMaxDistance(3);
  }


  if (distance_value <= 20) {
    if (iswalking != 0) {
      mp3.stopPlay();
      mp3.playSong(10, 30);  // Obstacle music
      iswalking = 0;
    }

    int st188Val_L = analogRead(ST188_L_PIN);
    int st188Val_R = analogRead(ST188_R_PIN);

    if (st188Val_L <= st188Val_R)
      turn(1, t * 3, 1);
    else
      turn(1, t * 3, -1);
  }
  else {
    if (iswalking != 1) {
      mp3.stopPlay();
      mp3.playSong(9, 10);  // Walking music
      iswalking = 1;
    }

    walk(1, t * 3, 1);
  }
}



void setup()
{
  Serial.begin(9600);
  delay(1000);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);


  mp3Serial.begin(9600);
  mp3.init();
  // mp3.setPlayMode(1);
  // mp3.playSong(1, 30); // play song 0001, set volume 20
  // mp3.playSong(9, 10);
  // if (iswalking == 1)
  //   // mp3.stopPlay();
  //   mp3.playSong(9, 10);
  // if (iswalking == 0)
  //   // mp3.stopPlay();
  //   mp3.playSong(10, 10);
  
  servo[0].attach(RR_PIN);
  servo[1].attach(RL_PIN);
  servo[2].attach(YR_PIN);
  servo[3].attach(YL_PIN);
  homes(100);
}

void loop()
{
  obstacleMode();
}

