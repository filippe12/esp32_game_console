#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "driver/rtc_io.h"
#include "../main/globals.h"

#define SW            128       // screen width
#define SH            64        // screen height
#define Gravity       2*9.8
#define PIPEW         2         //pipe width
#define GAPH          25        //gap height
#define PipeSpeed     7         //speed at which pipes move horizontaly
#define NumOfPipes    3         //number of pipes on the screen
#define SectionWidth  (SW+1)/3  //width of one pipe section
#define BirdPos       25        //horizontal bird position
#define LiftVel       12        //vertical flapping velocity

static int rand_num = 0;

//---------------------------OLEDI2C to u8g2 adapter -----------------------------------------------
void OLEDI2C_clrScr()
{
    u8g2_ClearBuffer(&u8g2);
}

void OLEDI2C_drawLine(short int x1, short int y1, short int x2, short int y2)
{
    u8g2_DrawLine(&u8g2, x1, y1, x2, y2);
}

void OLEDI2C_setPixel(short int x, short int y)
{
    u8g2_DrawPixel(&u8g2, x, y);
}

void OLEDI2C_drawCircle(short int x, short int y, short int r)
{
    u8g2_DrawCircle(&u8g2, x, y, r, U8G2_DRAW_ALL);
}

void nrgen(int cx, int cy, int br);
void OLEDI2C_printNumI(int num, short int x, short int y, short int un1, short int un2)
{
    nrgen(x + 1, y - 2, num);
}

void OLEDI2C_update()
{
    u8g2_SendBuffer(&u8g2);
}

void Delay(int milliseconds)
{
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
}
//--------------------------------------------------------------------------------------------------

int Random_Number(){
  rand_num = (17 * rand_num + 19) % 93;
  return rand_num;
}

void Draw_Bird_WingsUp(int height){
  if((height-1) >= 0 && (height-1) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-4,SH - (height-1),BirdPos+1,SH - (height-1));
      OLEDI2C_drawLine(BirdPos+2,SH - (height-1),BirdPos+5,SH - (height-1));
  }
  if(height >= 0 && height <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-8,SH - height,BirdPos+3,SH - height);
      OLEDI2C_drawLine(BirdPos+4,SH - height,BirdPos+7,SH - height);
  }
  if((height+1) >=0 && (height+1) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-5,SH - (height+1),BirdPos,SH - (height+1));
      OLEDI2C_drawLine(BirdPos+2,SH - (height+1),BirdPos+5,SH - (height+1));
  }
  if((height+2) >= 0 && (height+2) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-6,SH - (height+2),BirdPos-1,SH - (height+2));
  }
  if((height+3) >= 0 && (height+3) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-8,SH - (height+3),BirdPos-2,SH - (height+3));
  }
}

void Draw_Bird_WingsDown(int height){
  if((height-3) >= 0 && (height-3) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-8,SH - (height-3),BirdPos-2,SH - (height-3));
  }
  if((height-2) >= 0 && (height-2) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-6,SH - (height-2),BirdPos-1,SH - (height-2));
  }
  if((height-1) >= 0 && (height-1) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-5,SH - (height-1),BirdPos,SH - (height-1));
      OLEDI2C_drawLine(BirdPos+2,SH - (height-1),BirdPos+5,SH - (height-1));
  }
  if(height >= 0 && height <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-4,SH - height,BirdPos+3,SH - height);
      OLEDI2C_drawLine(BirdPos+4,SH - height,BirdPos+7,SH - height);
  }
  if((height+1) >= 0 && (height+1) <= (SH-1)){
      OLEDI2C_drawLine(BirdPos-8,SH - (height+1),BirdPos+1,SH - (height+1));
      OLEDI2C_drawLine(BirdPos+2,SH - (height+1),BirdPos+5,SH - (height+1));
  }
}

void Draw_Pipe(int position, int bottom_height){
  int pipe_position = position - (SectionWidth - 1)/2;
  if(bottom_height != -1){
      if((pipe_position - 3) >= 0){
          OLEDI2C_drawLine(pipe_position-3,SH-bottom_height,pipe_position-3,SH-(bottom_height-4));
          OLEDI2C_drawLine(pipe_position-3,SH-bottom_height-(GAPH-1),pipe_position-3,SH-(bottom_height+3)-GAPH);
      }
      if((pipe_position - 2) >=0){
          OLEDI2C_setPixel(pipe_position-2, SH-bottom_height);
          OLEDI2C_setPixel(pipe_position-2, SH-bottom_height - GAPH + 1);
          OLEDI2C_drawLine(pipe_position-2,SH-(bottom_height-4),pipe_position-2,SH);
          OLEDI2C_drawLine(pipe_position-2,SH-(bottom_height+3)-GAPH,pipe_position-2,0);
      }
      if((pipe_position - 1) >= 0) {
          OLEDI2C_setPixel(pipe_position-1, SH-bottom_height);
          OLEDI2C_setPixel(pipe_position-1, SH-bottom_height - GAPH + 1);
      }
      if(pipe_position >= 0) {
          OLEDI2C_setPixel(pipe_position, SH-bottom_height);
          OLEDI2C_setPixel(pipe_position, SH-bottom_height - GAPH + 1);
      }
      if((pipe_position - 1) >= 0) {
          OLEDI2C_setPixel(pipe_position+1, SH-bottom_height);
          OLEDI2C_setPixel(pipe_position+1, SH-bottom_height - GAPH + 1);
      }
      if((pipe_position - 2) >=0){
          OLEDI2C_setPixel(pipe_position+2, SH-bottom_height);
          OLEDI2C_setPixel(pipe_position+2, SH-bottom_height - GAPH + 1);
          OLEDI2C_drawLine(pipe_position+2,SH-(bottom_height-4),pipe_position+2,SH);
          OLEDI2C_drawLine(pipe_position+2,SH-(bottom_height+3)-GAPH,pipe_position+2,0);
      }
      if((pipe_position - 3) >= 0){
          OLEDI2C_drawLine(pipe_position+3,SH-bottom_height,pipe_position+3,SH-(bottom_height-4));
          OLEDI2C_drawLine(pipe_position+3,SH-bottom_height-(GAPH-1),pipe_position+3,SH-(bottom_height+3)-GAPH);
      }
  }
}

bool Collision_Check(float bird_height, float pipe_position, int pipe_height, float velocity){

  if(pipe_height == -1){
      if( bird_height<0 || bird_height>64 ) return true ;
      else return false ;
  }


  //wings up
  if(velocity >= 0){
      //lower pipe collision
      //bird is on level of thin part of pipe
      if(bird_height < (pipe_height-3)){
          if((pipe_position+2) >= (BirdPos-8) && (pipe_position-2) <= (BirdPos+6)) return true;
      }

      //bird is on level of wide part of pipe
      if(bird_height < (pipe_height+1) && bird_height >= (pipe_height-3)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+6)) return true;
      }

      //bird's nose is one pixel above pipe
      if(bird_height < (pipe_height+2) && bird_height >= (pipe_height+1)){
          if((pipe_position+3) >= (BirdPos-4) && (pipe_position-3) <= (BirdPos+4)) return true;
      }

      //upper pipe collision
      //bird is on level of thin part of pipe
      if(bird_height >= (pipe_height+GAPH+4)){
                if((pipe_position+2) >= (BirdPos-8) && (pipe_position-2) <= (BirdPos+6)) return true;
      }

      //bird is on level of wide part of pipe
      if(bird_height >= (pipe_height+GAPH) && bird_height < (pipe_height+GAPH+4)){
                if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+6)) return true;
      }

      //bird's nose is one pixel below pipe
      if(bird_height < (pipe_height+GAPH) && bird_height >= (pipe_height+GAPH-1)){
                if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+4)) return true;
      }

      //bird's nose is two pixels below pipe
      if(bird_height < (pipe_height+GAPH-1) && bird_height >= (pipe_height+GAPH-2)){
                if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos-2)) return true;
      }

      //bird's nose is three pixels below pipe
      if(bird_height < (pipe_height+GAPH-2) && bird_height >= (pipe_height+GAPH-3)){
                if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos-3)) return true;
      }
  }


  //wings down
  else{
      //lower pipe collision
      //bird is on level of thin part of pipe
      if(bird_height < (pipe_height-3)){
          if((pipe_position+2) >= (BirdPos-8) && (pipe_position-2) <= (BirdPos+6)) return true;
      }

      //bird is on level of wide part of pipe
      if(bird_height < (pipe_height+1) && bird_height >= (pipe_height-3)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+6)) return true;
      }

      //bird's nose is one pixel above pipe
      if(bird_height < (pipe_height+2) && bird_height >= (pipe_height+1)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+4)) return true;
      }

      //bird's nose is two pixels above pipe
      if(bird_height < (pipe_height+3) && bird_height >= (pipe_height+2)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos-2)) return true;
      }

      //bird's nose is three pixels above pipe
      if(bird_height < (pipe_height+4) && bird_height >= (pipe_height+3)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos-3)) return true;
      }

      //upper pipe collision
      //bird is on level of thin part of pipe
      if(bird_height >= (pipe_height+GAPH+4)){
          if((pipe_position+2) >= (BirdPos-8) && (pipe_position-2) <= (BirdPos+6)) return true;
      }

      //bird is on level of wide part of pipe
      if(bird_height >= (pipe_height+GAPH) && bird_height < (pipe_height+GAPH+4)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+6)) return true;
      }

      //bird's nose is one pixel below pipe
      if(bird_height < (pipe_height+GAPH) && bird_height >= (pipe_height+GAPH-1)){
          if((pipe_position+3) >= (BirdPos-8) && (pipe_position-3) <= (BirdPos+4)) return true;
      }
  }

  return false;
}

void Start_Screen(){
  // Clear the screen
  OLEDI2C_clrScr();

    //bird
    OLEDI2C_drawLine(BirdPos-8,SH - (32-3),BirdPos-2,SH - (32-3));
    OLEDI2C_drawLine(BirdPos-6,SH - (32-2),BirdPos-1,SH - (32-2));
    OLEDI2C_drawLine(BirdPos-5,SH - (32-1),BirdPos,SH - (32-1));
    OLEDI2C_drawLine(BirdPos+2,SH - (32-1),BirdPos+5,SH - (32-1));
    OLEDI2C_drawLine(BirdPos-4,SH - 32,BirdPos+3,SH - 32);
    OLEDI2C_drawLine(BirdPos+4,SH - 32,BirdPos+7,SH - 32);
    OLEDI2C_drawLine(BirdPos-8,SH - (32+1),BirdPos+1,SH - (32+1));
    OLEDI2C_drawLine(BirdPos+2,SH - (32+1),BirdPos+5,SH - (32+1));

    //letters
    OLEDI2C_drawLine(35,22,35,40);
    OLEDI2C_setPixel(36,21);
    OLEDI2C_drawLine(37,20,50,20);
    OLEDI2C_drawLine(45,21,45,40);
    OLEDI2C_drawLine(40,25,44,25);
    OLEDI2C_drawLine(40,31,44,31);
    OLEDI2C_drawLine(40,32,40,38);
    OLEDI2C_drawLine(36,39,40,39);
    OLEDI2C_drawLine(50,21,50,40);
    OLEDI2C_drawLine(46,39,49,39);
    OLEDI2C_setPixel(51,27);
    OLEDI2C_setPixel(52,26);
    OLEDI2C_drawLine(51,37,51,38);
    OLEDI2C_drawLine(52,38,52,39);
    OLEDI2C_drawLine(53,39,59,39);
    OLEDI2C_drawLine(53,25,59,25);
    OLEDI2C_drawLine(60,25,60,44);
    OLEDI2C_drawLine(55,30,55,34);
    OLEDI2C_drawLine(65,30,65,34);
    OLEDI2C_drawLine(65,40,65,43);
    OLEDI2C_drawLine(70,25,70,44);
    OLEDI2C_drawLine(75,30,75,34);
    OLEDI2C_drawLine(75,40,75,43);
    OLEDI2C_drawLine(80,24,80,37);
    OLEDI2C_drawLine(83,38,83,42);
    OLEDI2C_drawLine(85,25,85,32);
    OLEDI2C_drawLine(90,20,90,41);
    OLEDI2C_drawLine(95,25,95,27);
    OLEDI2C_drawLine(95,33,95,35);
    OLEDI2C_drawLine(100,23,100,39);
    OLEDI2C_drawLine(105,24,105,38);
    OLEDI2C_drawLine(110,31,110,38);
    OLEDI2C_drawLine(111,30,111,31);
    OLEDI2C_drawLine(114,25,114,37);
    OLEDI2C_drawLine(119,20,119,25);
    OLEDI2C_drawLine(124,20,124,38);
    OLEDI2C_drawLine(61,25,66,25);
    OLEDI2C_drawLine(71,25,76,25);
    OLEDI2C_drawLine(81,24,89,24);
    OLEDI2C_drawLine(91,20,97,20);
    OLEDI2C_drawLine(101,24,104,24);
    OLEDI2C_drawLine(101,29,104,29);
    OLEDI2C_drawLine(108,25,113,25);
    OLEDI2C_drawLine(120,20,123,20);
    OLEDI2C_drawLine(61,43,65,43);
    OLEDI2C_drawLine(65,39,68,39);
    OLEDI2C_drawLine(71,43,75,43);
    OLEDI2C_drawLine(75,39,78,39);
    OLEDI2C_drawLine(83,43,88,43);
    OLEDI2C_drawLine(91,39,98,39);
    OLEDI2C_drawLine(101,39,110,39);
    OLEDI2C_drawLine(116,39,124,39);
    OLEDI2C_drawLine(117,25,118,25);
    OLEDI2C_setPixel(116,26);
    OLEDI2C_setPixel(115,27);
    OLEDI2C_setPixel(106,26);
    OLEDI2C_setPixel(105,27);
    OLEDI2C_setPixel(98,21);
    OLEDI2C_setPixel(99,22);
    OLEDI2C_setPixel(98,38);
    OLEDI2C_drawLine(99,37,99,39);
    OLEDI2C_drawLine(66,26,68,26);
    OLEDI2C_drawLine(76,26,78,26);
    OLEDI2C_drawLine(119,30,119,34);
    OLEDI2C_drawLine(115,37,115,38);
    OLEDI2C_setPixel(116,38);
    OLEDI2C_drawLine(112,30,113,30);
    OLEDI2C_setPixel(69,27);
    OLEDI2C_setPixel(79,27);
    OLEDI2C_drawLine(69,36,69,38);
    OLEDI2C_drawLine(79,36,79,38);
    OLEDI2C_drawLine(89,40,89,42);
    OLEDI2C_setPixel(68,38);
    OLEDI2C_setPixel(78,38);
    OLEDI2C_setPixel(88,42);
    OLEDI2C_drawLine(82,37,84,37);
    OLEDI2C_drawLine(83,38,84,38);
    OLEDI2C_setPixel(81,35);
    OLEDI2C_setPixel(82,36);

    //instructions
    OLEDI2C_drawLine(36,55,36,62);
    OLEDI2C_drawLine(39,56,39,58);
    OLEDI2C_drawLine(41,55,41,58);
    OLEDI2C_drawLine(44,55,44,59);
    OLEDI2C_drawLine(51,52,51,59);
    OLEDI2C_drawLine(54,56,54,59);
    OLEDI2C_drawLine(60,52,60,59);
    OLEDI2C_drawLine(62,56,62,58);
    OLEDI2C_drawLine(65,56,65,58);
    OLEDI2C_drawLine(75,52,75,59);
    OLEDI2C_drawLine(77,58,77,59);
    OLEDI2C_drawLine(80,56,80,59);
    OLEDI2C_drawLine(82,55,82,59);
    OLEDI2C_drawLine(87,52,87,59);
    OLEDI2C_drawLine(37,55,38,55);
    OLEDI2C_drawLine(37,59,38,59);
    OLEDI2C_drawLine(42,59,43,59);
    OLEDI2C_drawLine(47,55,48,55);
    OLEDI2C_drawLine(47,57,48,57);
    OLEDI2C_drawLine(47,59,48,59);
    OLEDI2C_drawLine(52,55,54,55);
    OLEDI2C_drawLine(63,55,64,55);
    OLEDI2C_drawLine(63,58,64,58);
    OLEDI2C_drawLine(71,55,72,55);
    OLEDI2C_drawLine(71,57,72,57);
    OLEDI2C_drawLine(71,59,72,59);
    OLEDI2C_drawLine(78,55,79,55);
    OLEDI2C_drawLine(78,57,79,57);
    OLEDI2C_drawLine(78,58,79,58);
    OLEDI2C_drawLine(84,55,85,55);

    OLEDI2C_setPixel(46,56);
    OLEDI2C_setPixel(48,58);
    OLEDI2C_setPixel(59,53);
    OLEDI2C_setPixel(61,53);
    OLEDI2C_setPixel(74,53);
    OLEDI2C_setPixel(76,53);
    OLEDI2C_setPixel(86,53);
    OLEDI2C_setPixel(88,53);
    OLEDI2C_setPixel(70,56);
    OLEDI2C_setPixel(72,58);
    OLEDI2C_setPixel(83,56);


  // Update the display
  OLEDI2C_update();
}

void nrgen(int cx, int cy, int br){
  if (br==1){
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
  }
  if (br==2){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx,cy+6,cx,cy+10);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==3){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==4){
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx,cy+1,cx,cy+5);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==5){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx,cy+1,cx,cy+5);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==6){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx,cy+1,cx,cy+5);
      OLEDI2C_drawLine(cx,cy+6,cx,cy+10);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==7){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
  }
  if (br==8){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx,cy+1,cx,cy+5);
      OLEDI2C_drawLine(cx,cy+6,cx,cy+10);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==9){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx,cy+1,cx,cy+5);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
      OLEDI2C_drawLine(cx+1,cy+5,cx+5,cy+5);
  }
  if (br==0){
      OLEDI2C_drawLine(cx+1,cy,cx+5,cy);
      OLEDI2C_drawLine(cx+5,cy+1,cx+5,cy+5);
      OLEDI2C_drawLine(cx+5,cy+6,cx+5,cy+10);
      OLEDI2C_drawLine(cx,cy+1,cx,cy+5);
      OLEDI2C_drawLine(cx,cy+6,cx,cy+10);
      OLEDI2C_drawLine(cx+1,cy+10,cx+5,cy+10);
  }
}

void GameOver_Screen(int score){
  // Clear the screen
  OLEDI2C_clrScr();
  OLEDI2C_drawLine(45,18,51,18);
  OLEDI2C_drawLine(44,18,44,23);
  OLEDI2C_drawLine(45,23,50,23);
  OLEDI2C_drawLine(50,23,50,28);
  OLEDI2C_drawLine(44,28,50,28);
  OLEDI2C_drawLine(53,18,59,18);
  OLEDI2C_drawLine(53,19,53,28);
  OLEDI2C_drawLine(53,28,59,28);
  OLEDI2C_drawLine(61,18,67,18);
  OLEDI2C_drawLine(61,28,67,28);
  OLEDI2C_drawLine(61,19,61,28);
  OLEDI2C_drawLine(67,19,67,28);
  OLEDI2C_drawLine(69,18,75,18);
  OLEDI2C_drawLine(69,19,69,29);
  OLEDI2C_drawLine(70,23,74,23);
  OLEDI2C_drawLine(75,19,75,23);
  OLEDI2C_setPixel(74,24);
  OLEDI2C_setPixel(74,25);
  OLEDI2C_drawLine(75,25,75,29);
  OLEDI2C_drawLine(77,18,77,28);
  OLEDI2C_drawLine(78,18,83,18);
  OLEDI2C_drawLine(78,23,83,23);
  OLEDI2C_drawLine(78,28,83,28);

  OLEDI2C_drawLine(44,34,44,44);
  OLEDI2C_drawLine(44,34,49,34);
  OLEDI2C_drawLine(49,34,49,39);
  OLEDI2C_drawLine(44,39,50,39);
  OLEDI2C_drawLine(50,39,50,44);
  OLEDI2C_drawLine(44,44,50,44);
  OLEDI2C_drawLine(52,34,58,34);
  OLEDI2C_drawLine(52,39,58,39);
  OLEDI2C_drawLine(52,44,58,44);
  OLEDI2C_drawLine(52,34,52,44);
  OLEDI2C_drawLine(60,34,67,34);
  OLEDI2C_drawLine(60,34,60,39);
  OLEDI2C_drawLine(60,39,67,39);
  OLEDI2C_drawLine(66,39,66,44);
  OLEDI2C_drawLine(60,44,67,44);
  OLEDI2C_drawLine(72,34,72,45);
  OLEDI2C_drawLine(69,34,76,34);

  OLEDI2C_drawCircle(20,32,13);

  if (score>=50){
      OLEDI2C_printNumI(1,17,29,1,4);
  }
  if (score>=20){
      OLEDI2C_printNumI(2,17,29,1,4);
  }
  if (score>=10){
      OLEDI2C_printNumI(3,17,29,1,4);
  }
  else{
      OLEDI2C_printNumI(0,17,29,1,4);
  }

  int pcp , dcp , tcp , pcd, dcd, tcd ;

  if (score>flappy_bird_highscore){
      flappy_bird_highscore=score;
  }

  tcp=(score%10) ;
  dcp=(score%100)/10 ;
  pcp=(score/100) ;
  nrgen( 91, 18, pcp) ;
  nrgen( 99, 18, dcp) ;
  nrgen( 107, 18, tcp) ;

  tcd=(flappy_bird_highscore%10) ;
  dcd=(flappy_bird_highscore%100)/10 ;
  pcd=(flappy_bird_highscore/100) ;
  nrgen( 91, 34, pcd) ;
  nrgen( 99, 34, dcd) ;
  nrgen( 107, 34, tcd) ;

  u8g2_DrawStr(&u8g2, 5, 60, "Play Again");
  u8g2_DrawStr(&u8g2, 95, 60, "Exit");

  // Update the display
  OLEDI2C_update();
}

void run_spyrometry_bird()
{
  int section_pos;              // position of pipes on screen
  int pipe[NumOfPipes+1];       // array of pipe heights (3 on screen + next one)
  int score;                    // current score
  float velocity;               // bird velocity
  float height;                 // bird height
  float deltaT = 0.3;
  bool PointScored;
  u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);


  while(1){

      Start_Screen();

      //initialization of starting parameters
      section_pos = SectionWidth;
      height = SH/2;
      velocity = 0;
      PointScored = 0;
      score = 0;
      for(int i = 0; i < (NumOfPipes + 1); i++) pipe[i] = -1;

      //check for any button press to start
      esp_light_sleep_start();


      //game loop
      while(1){

          //clear start screen or previous screen
          OLEDI2C_clrScr();

          //draw bird
          if(velocity >= 0) Draw_Bird_WingsUp(height);
          else Draw_Bird_WingsDown(height);

          //draw pipes
          for(int i=0; i < (NumOfPipes+1);i++) Draw_Pipe(section_pos + i * SectionWidth, pipe[i]);

          //refresh screen
          OLEDI2C_update();

          //check for collision
          if(section_pos < 18){
              if(Collision_Check(height,section_pos-(SectionWidth-1)/2+SectionWidth,pipe[1],velocity)) break;
          }
          else{
              if(Collision_Check(height,section_pos-(SectionWidth-1)/2,pipe[0],velocity)) break;
          }

          //score update
          if(section_pos < 21 && !PointScored && pipe[0] != -1){
              score++;
              PointScored = 1;
          }

          //check for button press
          if (gpio_get_level(UP_BUTTON)){
              //if button is pressed lift the bird
              velocity = -LiftVel;
          }
          else{
              //if button isn't pressed apply gravity
              velocity += deltaT * Gravity;   
          }

          //update coordinates
          height -= velocity * deltaT;
          section_pos -= deltaT * PipeSpeed;

          //generate new pipe when one goes off screen
          if(section_pos < 0){
              section_pos += SectionWidth;
              PointScored = 0;
              for(int i = 0; i < NumOfPipes; i++){
                  pipe[i] = pipe[i+1];
              }
              pipe[NumOfPipes] = Random_Number() % 30 + 5;
          }
      }


      //game over section
      Delay(2000);

      //clear game screen and load game over screen
      OLEDI2C_clrScr();
      GameOver_Screen(score);
      OLEDI2C_update();

      //check for any button press to start
      esp_light_sleep_start();

      if(!gpio_get_level(LEFT_BUTTON))
        break; //exit game

      Delay(1000);
  }
}

void flappy_bird_run()
{
    run_spyrometry_bird();
}