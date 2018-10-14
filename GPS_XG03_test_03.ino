// 17-03-29 NEO-6M+SDカード
//   ＳＤカード　初期化失敗時　0.2秒のLED点滅で警告　動作ストップ
// ｓｗをビットへ　可能な限り関数不可
// ベースはESP8266__GPS_test_pin15_02から固体管理番号でリネーム
// WIFI_OFF
// ファイル名のタイムスタンプはまだ02にて更新
// NEO-6M:  (TX pin ->  8266 RX) -> Arduino Digital 0
//          (RX pin ->  8266 TX) -> Arduino Digital 2

// SD:      CS pin    -> Arduino Digital 15
//          SCK pin   -> Arduino Digital 14
//          MOSI pin  -> Arduino Digital 13
//          MISO pin  -> Arduino Digital 12
//          VCC pin   -> +5
//          GND       -> GND
//  LED           -> Arduino Digital 3
//  SW            -> Arduino Digital 16
//#include <TinyGPS++.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>
//#include <ctype.h>
//#include <Time.h>
#include <SD.h>
#define FLAG_FILE    1 //0000 0001
#define FLAG_LED3    2 //0000 0010
#define FLAG_SW1     4 //0000 0100
#define FLAG_SW2     8 //0000 1000
#define FLAG_A1     16 //0001 0000
#define FLAG_A2     32 //0010 0000

//■■■■下記必須変数
SoftwareSerial g_gps(2,0);// RX, TX
const int chipSelect = 15; //SDカード　シールド CS-10 GPSシールド　CS-8
File myFile;//ファイルポインタ
char filename[13];
int swin1,swin2;//スイッチ　ｏｎ－ｏｆｆでファイル制御
int files_sw1=0;
int all_sw=0;

  int comma[30]; // カンマカウント
  int ck =0;//
  int k; 
  int LED3_SW;       //
  char nmea[140];    //メインテーブル

//■■■■時間
unsigned long LED3_TIME;//LED動作カウンタ　動作含める
unsigned long LED3_INVAL = 500;//0.5秒おきに送信

unsigned long RECON_TIME;//FILE動作カウンタ　動作含める
unsigned long GET_TIME;//FILE動作カウンタ　動作含める
unsigned long FILE_TIME;//FILE動作カウンタ　動作含める
unsigned long FILE_INVAL = 60000;//60000=1分　120000=2分　180000=3分

tmElements_t tm;
//■■■■予備変数
/*
  float lat;//　緯度予備
  float lng;//  経度予備  
  int nmea_year;
  int nmea_nom;
  int nmea_date;
  int nmea_hour;
  int nmea_min;
*/
// time_t timer;
// struct tm *utc;
//■■■■■■■■■■■■■■■■■■■■
//    setup()
//■■■■■■■■■■■■■■■■■■■■ 
void setup()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  Serial.begin(115200);   //ＵＳＢ速度 
  g_gps.begin(9600);      //ＧＰＳユニット
  //pinMode(2, INPUT) ;     //スイッチに接続ピンをデジタル入力に設定
  //pinMode(3, OUTPUT);     // LED用
  pinMode(16,INPUT) ;    //スイッチに接続ピンをデジタル入力に設定
  pinMode(3,OUTPUT) ;  //ＬＥＤに接続ピンをデジタル出力に設定
  digitalWrite(3, LOW);   //■■■■動作時ｌｅｄ　ｏｆｆ

 //■■■■ファイル処理
  files_sw1 = 0;  //defでファイルスイッチＯＮ
 
  //■■■■バージョン表記
  Serial.println("null");
  Serial.println("ESP82GPS_XG03_test_03");

  //■■■■ＳＤカード
  if (!SD.begin(chipSelect)){ //CSピン番号を指定しない場合のデフォルトはSSピン(Unoの場合は10番ピン)です
      // カードの初期化に失敗したか、またはＳＤが入っていない
      Serial.println("SDcard init NG");
      while (1) {
        digitalWrite(3, HIGH);   // off時 LED on
        delay (200);
        digitalWrite(3, LOW);   // ｌｅｄ　ｏｆｆ
        delay (200);
      }
      //return;
    }
    else{
      Serial.println("SDcard init OK");
    }
    //■■■■ＳＤカード end
 
    for(k=0;k<30;k++){
        comma[k]=0;  
    }
    swin2 = swin1 = 0;
  
}
//■■■■■■■■■■■■■■■■■■■■
//    LOOP
//■■■■■■■■■■■■■■■■■■■■ 
void loop()
{
   ////////////////////////////////////////////////// 
   //    入力
   ////////////////////////////////////////////////// 
   
   swin2 = swin1;//状態保存
   
   if (digitalRead(16) == LOW ) { //スイッチの状態を調べる　プルアップ抵抗 low時キーＯＮ
    swin1 = 1;//delay (20);
   }
   else {
    swin1 = 0;//delay (2);
   }
yield();
   ////////////////////////////////////////////////// 
   //    判定 
   //////////////////////////////////////////////////   
   if (swin1 == 1 && swin2 == 0) { //■立ち上げ時 ON
      Serial.println("---  FILE record sw ON    ---");
      GET_TIME = LED3_TIME = millis();     //■■■■ 時間ゲット
      FILE_TIME = GET_TIME + FILE_INVAL;  //インターバルタイマ
      LED3_TIME = LED3_TIME+LED3_INVAL;   //
      LED3_SW =1;digitalWrite(3, HIGH);   //  LED on

      int breakflag=0;//while文脱出フラグ

      while(1){//●
      led3_sw_flas();//点滅割り込み
      one_line_read();//1行読んで
      yield();
      if(nmea[3]=='R' && nmea[4]=='M'&& nmea[5] =='C'){//判定 RMCだったら
         k=gps_nmea_rcm();                             //データチェック
         if(k==0){                                     //エラー無しデータまで読み出し
            rmc_dateTime();                           //RMCからtmへ時刻転送
            UTC_DateTimeConv(9);                      //tmからJST(+9)処理1～+23まで
            SdFile::dateTimeCallback(&dateTime);    // 日付と時刻を返す関数を登録
            filecop();                                //ファイル名生成+ファイルop
            myFile.println(nmea);                     //最初の1行書き込み　他のデータは省略　不関数でメモリ対策
            breakflag=1;                              //終了フラグ
            break;
          }
          if(breakflag) break;
        }
      if(breakflag) break;

      if (digitalRead(16) == HIGH ) { //スイッチの状態を調べる HI時キーOFF ＯＮ=>OFF時測位中キャンセル
        swin1 = 0;digitalWrite(3, LOW);
        break;
      } 
  }//●while終了
   RECON_TIME = millis();     //■■■■ 時間ゲット
   Serial.println("---  FILE record start    ---");
   } //■立ち上げ時 ON 終了
    /////////////////////////////////////////// 
    if (swin1 == 0 && swin2 == 1) {//■立下りｏｆｆ
      Serial.println("--- FILE Recording end  ---");
      fileccl();//ファイルクローズ
    }//■ 
    
   ////////////////////////////////////////////////// 
   //    メイン
   ////////////////////////////////////////////////// 
/*
Serial.print(swin1); //表示
Serial.print(","); //表示
Serial.print(swin2); //表示
Serial.println(); //表示
*/
  /*  Serial.print(millis() );
    Serial.print(" , "); //表示
    Serial.println(FILE_TIME );
  */
    GET_TIME = millis();
    /*
    if(GET_TIME > FILE_TIME && files_sw1 == 1 ){//インターバルクローズ＆オープン
      myFile.print("// FILE-TIME "); myFile.println(millis()/60000);
      Serial.print("// FILE-TIME "); Serial.println(millis());
      interval_file_oc(); //  クローズ　＆　再度オーブン
      //GET_TIME = millis();     //■■■■ 時間ゲット
      FILE_TIME = GET_TIME + FILE_INVAL;
    }
    */
    one_line_read();

}
//●●●●●●●●●●●●●●●●●●●●                                                                                    
//●●●●●●●●●●●●●●●●●●●●


//■■■■■■■■■■■■■■■■■■■■
//    NMEAデータ　チェック1　データなし時
//■■■■■■■■■■■■■■■■■■■■
int NMEA_data_chk1(int a) 
{

  int w;
  w=a+1;
/*Serial.println(a);
  Serial.println(nmea[w]);
*/
  if(nmea[w] == ','){
    return 1;//データ無し
  }

  if(nmea[w] == '9'){  
    return 1;//
  }
  else{ 
    return 0;
  }

}
//■■■■■■■■■■■■■■■■■■■■
//     GPS RMC 日付データ　チェック
//■■■■■■■■■■■■■■■■■■■■
int gps_nmea_rcm() {
  int w;
        int p=0;
        ////日月年UTC　, [8]-[9]
        w = comma[0];
        if(NMEA_data_chk1(w) == 1) {
          //Serial.println("ERR1");
          p++;}//エラーがあればｎ1 ','
        w = comma[8];
        if(NMEA_data_chk1(w) == 1) {
          //Serial.println("ERR2");
          p++;}//エラーがあればｎ1

        return p;

}
//■■■■■■■■■■■■■■■■■■■■
//      ファイルオープン処理
//■■■■■■■■■■■■■■■■■■■■
void filecop()
{
      files_sw1 = 1;//ファイルスイッチＯＮ
      filenamemake_JST();//ファイル名生成
      myFile = SD.open(filename, FILE_WRITE);
      delay (200);
      digitalWrite(3, HIGH);   //  LED on
      Serial.println("File.open");
}
//■■■■■■■■■■■■■■■■■■■■
//      ファイルクローズ処理
//■■■■■■■■■■■■■■■■■■■■
void fileccl()
{
      files_sw1 = 0;
      //ファイルクローズ処理
      myFile.close();
      delay (200);
      digitalWrite(3, LOW);   //  LED off　
      Serial.println("File.close");
}
//■■■■■■■■■■■■■■■■■■■■
//      タイムクローズ＆上書きオープン
//■■■■■■■■■■■■■■■■■■■■
void interval_file_oc()
{
      myFile.close();
      myFile = SD.open(filename, FILE_WRITE);
      delay (80);
      Serial.println("File.close&open");
}


//■■■■■■■■■■■■■■■■■■■■
//      スイッチ点滅
//■■■■■■■■■■■■■■■■■■■■
void  led3_sw_flas()
{
  if(millis() > LED3_TIME){
          if(LED3_SW ==1){
            LED3_SW=0;digitalWrite(3,LOW);
          }
          else {
            LED3_SW=1;digitalWrite(3,HIGH);
          }
    LED3_TIME = millis();     //■■■■ 時間ゲット
    LED3_TIME = LED3_TIME+LED3_INVAL; 
  }
}

//■■■■■■■■■■■■■■■■■■■■
//     1行読み込み
//■■■■■■■■■■■■■■■■■■■■
int one_line_read()
{
  int breakflag=0;  //
  int v=0;          //行カウンタリセット
  int ck=0;   //comma[k]位置最大　リセット
  unsigned long  s;
   //yield();
  while(1){//●
    
    //以下 GPS　データリード　(1文字)関数べた書き■■■■
    while(1){
      nmea[v] = g_gps.read();//■gpsデータ読み出し
    //  Serial.println(nmea[v],HEX);
      if(-1 == nmea[v] || 0x0a == nmea[v] || 0xFF == nmea[v])continue;   //-1の時は読み飛ばし    
      else{ 
      //
      
      break;
      }
    } 
   //GPS　データリード　(1文字)関数べた書き終了■■■■
    if(',' == nmea[v]){comma[ck]=v;ck++;}  //カンマカウント
    //■■■■■■■■■■■■■■■■■■■■
    //     
    //■■■■■■■■■■■■■■■■■■■■ 
    if( 0x0d == nmea[v])//改行
    {
      nmea[v]='\0'; //端末処理

// if(files_sw1 == 1 && ( nmea[3] == 'R' || nmea[4] == 'G')){  //sw1で書き込み   GPRMC + GPGGAの書き込み
// if(files_sw1 == 1 &&  nmea[3] == 'R'){  //sw1で書き込み   GPRMC の書き込み
   if(files_sw1 == 1 ){  //sw1で書き込み   全ての書き込み
        GET_TIME = GET_TIME -  RECON_TIME;
        //myFile.print(FILE_TIME);myFile.print(",");myFile.println(GET_TIME);
        myFile.print(nmea);//■■■■■ 書き込み
        myFile.print("[");
//        myFile.print(GET_TIME/1000);
        s = GET_TIME/3600000;
        if(s<10) myFile.print("0");
        myFile.print(s);
        myFile.print(":");
        s = (GET_TIME/60000)%60;
        if(s<10) myFile.print("0");
        myFile.print(s);
        myFile.print(":");
        s =(GET_TIME/1000) % 60;
        if(s<10) myFile.print("0");
        myFile.print(s);
        myFile.println("]");//端末改行
     /*   Serial.print("F");
        Serial.print(nmea); //■■■■■ 表示     
        Serial.print("[");
        Serial.print(GET_TIME/1000);
        Serial.println("]");
     */   
      }
      else{
      //  Serial.print("S"); //
        Serial.print(nmea); //■■■■■ 表示
        Serial.print("[");
        s = GET_TIME/3600000;
        if(s<10) Serial.print("0");
        Serial.print(s);
        Serial.print(":");
        s = (GET_TIME/60000)%60;
        if(s<10) Serial.print("0");
        Serial.print(s);
        Serial.print(":");
        s =(GET_TIME/1000) % 60;
        if(s<10) Serial.print("0");
        Serial.print(s);
        Serial.println("]");
        
      }
      v=0;    //行カウンタリセット
      ck=0;   //comma[k]位置最大　リセット
      breakflag=1;//ループ終了フラグ
    }//改行処理終了 TRUE
    else{
      v++; if(v>=148)v=0;             //念のため
    }//改行処理終了 FALSE
    //////////////////////////////////////////
   if(breakflag) break; //ループ終了
  }
}
//■■■■■■■■■■■■■■■■■■■■
//  世界時から日本時間へ (とりあえず+時間のみ)　UTC_DateTimeConv(9); //UTCからJSTへ
//■■■■■■■■■■■■■■■■■■■■
int UTC_DateTimeConv(int s)
{
  int ndays[]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int w=0;
if(s>23 || s<-23)return 1;//エラー
if(s>0){
  tm.Hour = tm.Hour + s ;//時間加算
  if(tm.Hour>= 24){
    tm.Day++;
    tm.Hour = tm.Hour % 24;
  }
  else{
    return 0;         //日付変わらないならそのまま
  }

  //tm.Day　最大値チェック

  if (tm.Month == 2 && is_leap_year( tm.Year+1970 ) == 1){//2月ならうるう年チェック
//  Serial.println("ok");
    if(tm.Day>29){      //うるう年の2月 
        tm.Day =1;
        tm.Month =3;
      return 0;         //でのまま終了
    }
  }
  else{
   if(tm.Day>=ndays[tm.Month-1]){  
     tm.Day =1;
     tm.Month =tm.Month +1;
   }
  }
  //////////////////////////////
    if(tm.Month>12){            //12月超えたら　年+1
      tm.Year = tm.Year + 1;
      tm.Month = 1;
    }
  }
}
//■■■■■■■■■■■■■■■■■■■■
//  うるう年判定　(1:うるう年 0:平年)
//■■■■■■■■■■■■■■■■■■■■
int is_leap_year(int year)
{
   if (year % 400 == 0) return 1;
   else if (year % 100 == 0) return 0;
   else if (year % 4 == 0) return 1;
   else return 0;
}
//■■■■■■■■■■■■■■■■■■■■
//   RMCから年月日時分秒抜き出し  2017年時　経過年数47 
//■■■■■■■■■■■■■■■■■■■■
void rmc_dateTime()
{
  int w;
  char s[5];
  w = comma[8];w++;//(日月年)指定位置カンマから数字先頭へ

//■■年  (20)17-1970  経過年数47 
s[0] = nmea[w+4];
s[1] = nmea[w+5];
s[2]= '\0';
tm.Year = atoi(s)+30; //変換時+30で経過年数へ    
//■■月
s[0] = nmea[w+2];
s[1] = nmea[w+3];
s[2]= '\0';
tm.Month = atoi(s);
//■■日
s[0] = nmea[w+0];
s[1] = nmea[w+1];
s[2]= '\0';
tm.Day = atoi(s);

w = comma[0];w++;//(時分秒)指定位置カンマから数字先頭へ
//■■時
s[0] = nmea[w+0];
s[1] = nmea[w+1];
s[2]= '\0';
tm.Hour = atoi(s);
//■■分
s[0] = nmea[w+2];
s[1] = nmea[w+3];
s[2]= '\0';
tm.Minute = atoi(s);
//■■秒
s[0] = nmea[w+4];
s[1] = nmea[w+5];
s[2]= '\0';
tm.Second = atoi(s);//秒　

}
//■■■■■■■■■■■■■■■■■■■■
//   ＳＤカードタイムスタンプ
//■■■■■■■■■■■■■■■■■■■■
void dateTime(uint16_t* date, uint16_t* time)
{
 // (RTC.read(tm));
  uint16_t year = tm.Year+1970;
  uint8_t month = tm.Month; 

  // GPSやRTCから日付と時間を取得
  // FAT_DATEマクロでフィールドを埋めて日付を返す
  *date = FAT_DATE(year, month, tm.Day);

  // FAT_TIMEマクロでフィールドを埋めて時間を返す
  *time = FAT_TIME(tm.Hour, tm.Minute, tm.Second);

}
//■■■■■■■■■■■■■■■■■■■■
//　　　　　　ファイル名生成
//■■■■■■■■■■■■■■■■■■■■
void filenamemake_JST() 
{
  //GPSデータＵＴＣなので日本時間　UTC_DateTimeConv()直後に呼ぶ事
String cy =  String((tm.Year+1970)-2000);  
//Serial.println(cy);
String cm =  String(tm.Month);    
//Serial.println(cm);
String cd =  String(tm.Day);    
//Serial.println(cd);
String ch =  String(tm.Hour);    
//Serial.println(ch);
  filename[0] = '2' ;
  filename[1] = '0' ;
  filename[2] = cy[0];//年
  filename[3] = cy[1];
  //月
  if(tm.Month>=10){filename[4] = cm[0];filename[5] = cm[1];}
    else{ filename[4] = '0';filename[5] = cm[0];}
  //日
  if(tm.Day>=10){filename[6] = cd[0];filename[7] = cd[1];}
    else {filename[6] = '0';filename[7] = cd[0];}
  filename[8] = '.' ;
  filename[9] = 'l' ;
  filename[10] = 'o' ;
  filename[11] = 'g' ;
  filename[12] = '\0' ;
//  Serial.print("filename");  Serial.println(filename);

}
//■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
//  ボード設定　ESP-WROOM-02(ESP8266)
//■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
/*
設定項目   パラメーター
マイコンボード   Generic ESP8266 Module
Flash Mode  QIO
Flash Frequency   80MHz
Upload Using  Serial
CPU Frequency   80MHz
Flash Size  4M (1M SPIFFS)
Debug port  Disabled
Debug Level   None
Reset Method  nodemcu
Upload Speed  115200
*/

/*
動作説明
ＳＤカード　初期化失敗時　0.2秒のLED点滅で警告　動作ストップ
ＳＷ　ＯＮにてGPRMC 読み取り　データ有効であれば年月日　時間取得して閏年判定+日本時間に変更して
ファィル名タイムスタンプへ送りファィル追記モードでオープン
データ有効でない時にＳＷがＯＦＦになった場合そのままで待機(データをSerial.printするだけ)
他のArduino　2台のソースと違ってなんの芸もなくただnmeaデータ(すべて)を記録するだけです
デバッグ用に初期Arduinoとからソース引継ぎいで作ってある為　他のソースと比べ手探りがそのまま

メモリと速度も安定してる為記録が止まるってことはそうそう無いと思う。
が一部山中で受信が不能状態が出るとその後安定してない場合があるようで
さてどうした物かと？

2018/10/14　ソース公開の為　文末追記
*/