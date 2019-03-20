/* SCR Morion ver.01  by Yukako MIYANISHI
 * - This is for an experiment to remove motion 
 *   artifacts from skin conductance response (SCR).
 *   
 *   DISPLAY the protocol of experiment, and
 *   LOG the accelerometer and gyrometer sensor data 
 *   from serial monitor. This is corresponds to
 *   Arduino sketch on UnlimitedHand ().
 * - UPDATE LOG:
 *   2017.12.23
 *
 */


///////////////////////////////////////////////
// GLOBAL VARIABLES
///////////////////////////////////////////////
// シリアルライブラリを取り入れる
import processing.serial.*;
// myPortというインスタンスを用意
Serial myPort;
int NUM = 5;
String[] buffer = new String[NUM];
int count = 0;

/**** 音の処理 ****/
import ddf.minim.*;
Minim minim;  //Minim型変数であるminimの宣言
AudioPlayer player;  //サウンドデータ格納用の変数

/**** 表示させる文字など ****/
String message = "";
int[] accelGyro = new int[6];

boolean startFlg; //'S'でスタート

boolean restFlg = true;
boolean actionFlg = true;
int actionCount = 0;
int actionTime = 0;

boolean onceActionCountFlg = true;
boolean onceBeepFlg = true;

int keepSec = 0;

/**** 文字表示のインターバル設定 ****/
int strOffTime = 300;     //文字を消す(白画面)間隔

/**** ファイル出力 ****/
PrintWriter output;                    //ファイルI/O用(PrintWriter型オブジェクト)
int millisForTime = 0;
String[] actionCode = {
  "standup", "sitdown", "walk", "walk_wait", 
  "read", "read_wait", "write", "write_wait", "post_rest"
  };

///////////////////////////////////////////////
// SET UP
///////////////////////////////////////////////
void setup(){ 
  /**** 描画部分 ****/
  size(1000, 500);
  
  for(int i=0; i<6; i++){
    accelGyro[i] = 0;
  }
  
  myPort = new Serial(this, "/dev/tty.usbserial-A104X0SZ", 115200);
  
  
  /**** 音の設定 ****/
  minim = new Minim(this);  //初期化
  player = minim.loadFile("beep.mp3");  //groove.mp3をロードする
  
  /**** インターバル用の設定 ****/
  //10[回/sec] = 10[Hz]
  frameRate(NUM); //1秒間に何フレームdraw()を実行するか サンプリング周期に合わせて調節
  
  /**** ファイル出力用の設定 ****/
  String dateTime = nf(month(),2) + nf(day(),2) + nf(hour(),2) + nf(minute(),2); //ファイル名として日付を取得
  String pathName = "/Users/3824/Desktop/Data_raw/171223_pre_basic/"; //ファイルを出力したいパスを入力
  String fileName = pathName + dateTime + ".csv";    //パスを元にファイル名作成
  
  output = createWriter(fileName);               //ファイル名(.csv)でファイルを開く
  output.println("actionCode,time,accelX,accelY,accelZ,gyroX,gyroY,gyroZ");  //ヘッダが必要であれば
  
}//end of setup




///////////////////////////////////////////////
// DRAW
///////////////////////////////////////////////
void draw(){
  background(255); //背景を白に
  
  /**** キーによる処理 ****/
  if(keyPressed==true){
    if(key=='s'){ //'s'でスタート
      println("START");
      startFlg = true;
      actionTime = millis();
    }else if(key=='q'){ //'q'で終了
      output.flush();
      output.close();
      exit();
    }
  }
  
  
  
  /**** startした時の処理 ****/
  if(startFlg){
    
    //開始または終了時の安静処理
    if(restFlg){
        cleanText();
        
        //このループに入った瞬間(actionTime)から文字オフ時間経ったら
        if(millis() > actionTime + strOffTime){
            //player.play();  //再生
            //player.rewind();  //再生が終わったら巻き戻しておく
            //メッセージ表示
            message = "目を閉じて\n安静にしてください";
            writeText(message);
            
            //安静を60sec続けたら
            if(millis() > actionTime + 60 * 1000){
              
                //アクション終了後なら
                if(actionCount > 23){
                  //最後1回だけ鳴らす
                  player.play();  //再生
                  player.rewind();  //再生が終わったら巻き戻しておく
                  
                  //アプリケーションを終了
                  output.flush();
                  output.close();
                  exit();
                
                //アクション終了前(アプリケーション開始時)まら
                }else{
                  //開始時刻を記録して安静を抜ける
                  actionTime = millis();
                  restFlg = false;
                }
            }//if安静60sec続けたら
        }//if文字オフ時間経ったら
    
    
    //開始または終了時の安静以外(アクション)の処理
    }else{
        
        //actionのメッセージをactionCountに応じて決定
        setMessage();
        
        /* メッセージ表示 */
        cleanText();
        if(millis() > actionTime + strOffTime){
          onceActionCountFlg = true; //次は一回だけactionCount++するよ
          writeText(message); //メッセージ表示
        }else{
          onceBeepFlg = true;
        }
        
        /* 表示メッセージ更新 */
        //アクションに応じた秒数(keepSec)経ったら
        if(millis() > actionTime + keepSec * 1000){
          actionFlg = !actionFlg; //アクションを切り替える(ex. 立つ <-> 座る)
          
          //draw()内で継続して呼び出される1回目だけactionCount++
          if(onceActionCountFlg){
            actionCount++;
            onceActionCountFlg = false; //継続して呼び出される次からは++しないよ
          }
          
          actionTime = millis(); //アクションの直前時間を記録
        }
        
        //アクションを24回(2アクション*3セット*4種類)繰り返したら
        if(actionCount > 23){
          restFlg = true; //安静処理に戻る
        }
        
    }//restFlg
  }//startFlg
  
  
  String tempLine = myPort.readString();
  if(tempLine != null){
    String[] tempFirstLine = split(tempLine, "_");
    millisForTime = millis() - floor(millis()/1000) * 1000;
    String ac = "";
    if(restFlg){
      if(actionCount > 23){
        ac = "post_rest";
      }else{
        ac = "pre_rest";
      }
    }else{
      ac = actionCode[actionCount/6];
    }
    buffer[count] = ac
                  + "+"
                  //+ nf(year(), 4) + "-" + nf(month(), 2) + "-" + nf(day(), 2) + "-"
                  + nf(hour(), 2) + ":" + nf(minute(),2) + ":" + nf(second(),2) + "." + str(millisForTime)
                  + "+"
                  + tempFirstLine[0];
  }
  
  if(count > NUM-2){
    for(int i=0; i<NUM; i++){
      println(i + " " + buffer[i]);
    
      if(buffer[i] != null){
          String[] temp = split(buffer[i], "+");
          if(temp.length == 8){
              String _actionCode = temp[0];
              String _dateMillis = temp[1];
              accelGyro[0] = int(temp[2]);
              accelGyro[1] = int(temp[3]);
              accelGyro[2] = int(temp[4]);
              accelGyro[3] = int(temp[5]);
              accelGyro[4] = int(temp[6]);
              accelGyro[5] = int(temp[7]);
              
              output.println(
                            _actionCode + ","
                          + _dateMillis + "," 
                          + accelGyro[0] + ","
                          + accelGyro[1] + ","
                          + accelGyro[2] + ","
                          + accelGyro[3] + ","
                          + accelGyro[4] + ","
                          + accelGyro[5]);
         }//temp.length==6
      }//buffer != null    
    }//for buffer1行
    count = -1;
  }//if count > NUM
  
  count++;
  
}//end of draw




///////////////////////////////////////////////
// STOP
///////////////////////////////////////////////
//マウスが押されたときのイベント処理
void stop(){
  player.close();  //サウンドデータを終了
  minim.stop();
  super.stop();
}




///////////////////////////////////////////////
// WRITE TEXT
///////////////////////////////////////////////
void writeText(String message){
  PFont font = createFont("MS Gothic", 3.0f, true);//文字の作成
  textSize(14);
  textFont (font); // 選択したフォントを指定する
  
  //文字描画
  fill(0);              //描画色を黒に設定
  textSize(70);         //サイズを設定
  textAlign(CENTER);    //まんなか
  text(message, width/2, height/2);
  
  if(onceBeepFlg){
    player.play();  //再生
    player.rewind();  //再生が終わったら巻き戻しておく
    onceBeepFlg = false;
  }
  
}

///////////////////////////////////////////////
// CLEAN TEXT
///////////////////////////////////////////////
void cleanText(){
  //文字を見かけ上消す処理
  fill(255);              //描画色を白に設定
  noStroke();             //枠線はなし
  rect(0, 0, width, height); //四角を上から描画して見かけ上前の文字を消す
}


///////////////////////////////////////////////
// SET MESSAGE
///////////////////////////////////////////////
void setMessage(){
  //6回(2アクション*3セット)ずつ更新したいので, 
  //(0,1,2,3,4,5,/6,7,8,9,10,11,/12,13,14,15,16,17,/18,19,20,21,22,23),
  //6で割った商で場合分けすれば簡単!
  switch(actionCount/6){
    case 0:
      if(actionFlg){
        message = "立ってください";
        keepSec = 30;
      }else{
        message = "座ってください";
        keepSec = 30;
      }
      break;
      
    case 1:
      if(actionFlg){
        message = "立ってその場で\n足踏みをしてください";
        keepSec = 10;
      }else{
        message = "足踏みを止めて\n待機してください";
        keepSec = 60;
      }
      break;
      
    case 2:
      if(actionFlg){
        message = "座った状態で\n読み上げてください";
        keepSec = 30;
      }else{
        message = "読み上げを\nやめてください";
        keepSec = 30;
      }
      break;
      
    case 3:
      if(actionFlg){
        message = "書いてください";
        keepSec = 30;
      }else{
        message = "書くのを\nやめてください";
        keepSec = 30;
      }
      break;
      
  }//switch
  
}


//end of ScrMotion