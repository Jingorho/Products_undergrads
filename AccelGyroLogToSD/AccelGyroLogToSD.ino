/*
 * 2017.12.31 by Y.MIYANISHI
 * 6軸加速度ジャイロセンサMPU6050を使用して角度や動きをmicroSDカードに記録する.
 * raw値(ax,ay,az,gx,gy,gz)とオイラー角(3軸)とロールピッチヨー(3軸).
 * PWMのデジタル2番ピンにタクトスイッチを設置することで, 
 * タイムスタンプもどきをファイルに記録する.
 * 参考 : http://iot.keicode.com/arduino/arduino-camera-sd-card.php
 */

 
#include <SoftwareSerial.h>
#include <SD.h>
#include <SPI.h>
//#include <DateTime.h>
//#include <DateTimeStrings.h>

///////////////////////////////////////////////
// VARIABLES
///////////////////////////////////////////////
/** SDカード用 **/
const int chipSelect = 10;
int file_count = 1;
char fname[16];
//time_t prevtime;
unsigned long time;

/** タイムスタンプ用 **/
int inputPin = 3;
bool timeStampFlg = false;


/** 値格納用 **/
int NUM = 100;
int count = 0;
String value_raw = "0,0,0,0,0,0,0";
String value_ypr = ",0,0,0";
String value = value_raw + value_ypr;


/** MPU6050_DMP6ライブラリからパクった **/
// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
//#include "MPU6050.h"
#include "MPU6050_6Axis_MotionApps20.h"
//#include "MPU6050.h" // not necessary if using MotionApps include file
// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 mpu;
//MPU6050 mpu(0x69); // <-- use for AD0 high

int16_t ax, ay, az;
int16_t gx, gy, gz;
//int16_t value_ag[6];
//String value_ag[6];
//float value_[3];
//float value_ypr[3];

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
int INTERRUPT_PIN = 12;

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };


// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}



///////////////////////
//  SETUP
///////////////////////
void setup(){
    pinMode(inputPin, INPUT); //タイムスタンプ用
    

    /** MPU6050初期設定 **/
    Wire.begin();
    Wire.setClock(400000);
  
    // initialize serial communication
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)
    Serial.begin(115200);
    while (!Serial); // wait for Leonardo enumeration, others continue immediately
  
    // NOTE: 8MHz or slower host processors, like the Teensy @ 3.3v or Ardunio
    // Pro Mini running at 3.3v, cannot handle this baud rate reliably due to
    // the baud timing being too misaligned with processor ticks. You must use
    // 38400 or slower in these cases, or use some kind of external separate
    // crystal solution for the UART timer.
  
    // initialize device
    Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();
  
    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
  
//      // wait for ready
//      Serial.println(F("\nSend any character to begin DMP programming and demo: "));
//      while (Serial.available() && Serial.read()); // empty buffer
//      while (!Serial.available());                 // wait for data
//      while (Serial.available() && Serial.read()); // empty buffer again
  
    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();
  
    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip
    
    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);
  
        // enable Arduino interrupt detection
        Serial.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();
  
        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;
  
        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
        
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

    /** SDカード初期設定 **/
    Serial.print(F("Initializing SD card..."));
    if (!SD.begin(chipSelect)) {
      Serial.println(F("Card failed, or not present"));
    }else{
      Serial.println(F("OK."));
    }

  //  //時刻合わせ
  //  prevtime = DateTime.makeTime(_sec, _min, _hour, _day, _month, 2017);
  //  DateTime.sync(prevtime); //_year/_month/_day/_hour:_min:_secからの経過秒数

}//setup()


///////////////////////
// LOOP
///////////////////////
void loop(){
    count++; 
    /******** 時刻(ただのミリ秒だけど)計算 ********/
    // DateTime.available();
    time = millis();//プログラム開始からのミリ秒計算

    
    /******** タイムスタンプ判断処理 ********/
    timeStampFlg = false;
    int status;
    status = digitalRead(inputPin) ; //スイッチの状態を読む
    if(status == HIGH){
        Serial.println("############# PUSHED SWITCH FLAG #############");
        timeStampFlg = true;
    }else{
       timeStampFlg = false;
    }

    /******** MPU6050の処理 ********/
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    
    
    // if programming failed, don't try to do anything
    if (!dmpReady){
      return;
    }

//    // wait for MPU interrupt or extra packet(s) available
//    while (!mpuInterrupt && fifoCount < packetSize) {
//        // other program behavior stuff here
//        // if you are really paranoid you can frequently test in between other
//        // stuff to see if mpuInterrupt is true, and if so, "break;" from the
//        // while() loop to immediately process the MPU data
//    }

    // reset interrupt flag and get INT_STATUS byte
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();

    // get current FIFO count
    fifoCount = mpu.getFIFOCount();

    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));
        
    // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus & 0x02) {
      
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

        // read a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);
        
        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;

//      // display quaternion values in easy matrix form: w x y z
        mpu.dmpGetQuaternion(&q, fifoBuffer); //Quaternionは省略

        // display Euler angles in degrees
//        mpu.dmpGetQuaternion(&q, fifoBuffer);
//        mpu.dmpGetEuler(euler, &q); //オイラー角
        
//        // display Euler angles in degrees
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity); //ロールピッチヨー角


        //Serialにもprintしておく
        Serial.print("r: ");
        Serial.print(ax); Serial.print(",");
        Serial.print(ay); Serial.print(",");
        Serial.print(az); Serial.print(",");
        Serial.print(gx); Serial.print(",");
        Serial.print(gy); Serial.print(",");
        Serial.println(gz);
        


        //なんとなくデータNULL判断はこれにしてる. なんでもいいか
        if(ax == NULL || ay == NULL || az == NULL || gx == NULL || gy == NULL || gz == NULL){
          Serial.println("Data is null");
        }else{
          //rawの6値はここでvalue_rawに格納
          //int16_tは16進数intなので10進数Stringに変換
//          value_raw = String(time)
//              + "," + String(ax,DEC) + "," + String(ay,DEC) + "," + String(az,DEC)
//              + "," + String(gx,DEC) + "," + String(gy,DEC) + "," + String(gz,DEC);
           value_raw = String(ax,DEC) + "," + String(ay,DEC) + "," + String(az,DEC)
              + "," + String(gx,DEC) + "," + String(gy,DEC) + "," + String(gz,DEC);
  
          //角度3軸をここでvalue_yprに格納
          value_ypr = "," + String(ypr[0] * 180/M_PI)
                    + "," + String(ypr[1] * 180/M_PI)
                    + "," + String(ypr[2] * 180/M_PI) + "\n";


          //タイムスタンプ押してるフレームだったら末尾に区切りつける
          if(timeStampFlg){ value_ypr += "----\n"; }

//          value = value + value_raw + value_ypr;
          
          Serial.print("2: "); Serial.print(value_raw); Serial.print(value_ypr);
          Serial.print("b: "); Serial.print(value);
          
        }//!NULL
        


        if(count > NUM){
            /******** SDカードを開いて書き込み ********/
            SetFileName();
            //ファイルを開く. 同名のファイルがある場合は上書き
            File file = SD.open( fname, FILE_WRITE | O_TRUNC );
            
            if(file){
                Serial.println("Output to file !!");
                Serial.println(value);
                //SDカード内のファイルに書き込み
//                file.println(value_raw[NUM-1] + value_ypr[NUM-1]);
//                file.close();
            }else{
                  Serial.println("File Opening Error");
            }//if file

            count = 0;

        }//if(count>NUM)
    }//if FIFO over
    
    delay(50);
  
}//loop()



////////////////////////
// SET FILE NAME
////////////////////////
void SetFileName(){
//  String dateForFileName = String(DateTime.Year) + 
//                           String(DateTime.Month) +
//                           String(DateTime.Day) +
//                           String(DateTime.Hour) +
//                           String(DateTime.Minute) + ".csv";
  
  sprintf(fname, "log.csv", file_count++);
  if( file_count > 9999 ){
    file_count = 1; // Overwrite
  }
}


