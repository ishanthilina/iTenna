#include <main.h>
#include <math.h>

//d4,d5,d6

//Configuration pins
//PIN_D1|PIN_D0|PIN_C5|PIN_C4|PIN_D3|PIN_D2
//RESET | half | Clk  |  Dir |Enable| CON

#define RESET PIN_D1
#define HALF PIN_D0
#define CLK PIN_C5
#define DIR PIN_C4
#define ENABLE PIN_D3
#define CON PIN_D2

#define CUR_CH_LOC 65


int8 mask_A[] = {0x02, 0x2E, 0x01, 0x08, 0x2C, 0x18, 0x10, 0x0E, 0x00, 0x0C};
int1 mask_E[] = {0,1,1,1,0,0,0,1,0,0};
//int channel_number = 0;
int16 timer_counter = 0;

//Global variables

int state = 0; // 0 - normal 1 - tuning 

//Speed of the motor
int8 speed=2;

//current position of the IR wave
int i = 0;

//indicates a new pulse
int1 newpulse=0;

int16 array[40];

//global vairables
int16 old_ccp = 0;

unsigned int8 current_step = 0;

unsigned int8 current_ch=0;

//variables used for button polling
int1 ch_select_mode=1;
int1 up_button_pressed=0;
int1 down_button_pressed=0;
int1 timeout = 1;

int1 ch_select_mode_prestate=0;
int1 up_button_pressed_prestate=0;
int1 down_button_pressed_prestate=0;

int1 tuning_state = 0;

#int_ccp1 
void ccp1_isr(void)
{
   disable_interrupts(GLOBAL);
   
  
   int16  ccp_delta;
   ccp_delta = CCP_1 - old_ccp;
   if (i > 33|| (ccp_delta > 20000&&i > 2))
   {
      i = 0;
      newpulse = 1;
      return;
   }

   array[i] = ccp_delta;
   i++;
   old_ccp = CCP_1;
   enable_interrupts(GLOBAL);
}

#int_TIMER0
void  TIMER0_isr(void) 
{
     int16 x = 32;
     
     if(tuning_state == 1){
     x = 1000;
     }
     
//disable_interrupts(INT_TIMER0);
  if (timer_counter == 0) {
     output_low(PIN_B5);
     output_a(mask_A[current_ch/10]);
     output_e(mask_E[current_ch/10]);
     output_high(PIN_B4);
  } else if (timer_counter == x/2) {
     output_low(PIN_B4);
     output_a(mask_A[current_ch%10]);
     output_e(mask_E[current_ch%10]);
     output_high(PIN_B5);
  }  
  timer_counter = (timer_counter + 1) % x;
  
timeout = 1;

}

#int_EXT
void  EXT_isr(void) 
{
 disable_interrupts(INT_EXT); 
 output_low(ENABLE);
 current_ch = 0;
}

void initialize_motor(){

   //set up
   output_high(CON);
   output_low(ENABLE); //disable
   output_high(DIR);
   output_high(HALF);
   output_high(RESET);

   //enable the clock
   output_high(CLK);

}


//sends a pulse to the motor
void give_pulse(unsigned int8 x, int1 clk_wise){
   
   output_high(ENABLE);
   unsigned int count=0;
   
   if(clk_wise==1){
      
      output_high(DIR);
      
      for(count=0;count<x;count++){
         output_low(CLK);
         delay_ms(1);
         output_high(CLK);
         current_step++;
         //current_step%256;
         delay_ms(speed);
      }
    }else{
    
      output_low(DIR);
      
      for(count=0;count<x;count++){
         output_low(CLK);
         delay_ms(1);
         output_high(CLK);
         current_step--;
         //if(current_step==0) current_step=256;
         delay_ms(speed);
      }
    
    }
   output_low(ENABLE);
}


void give_pulse_manual(int1 clk_wise){
   
   unsigned int count=0;
   
   if(clk_wise==1){
      
      output_high(DIR);
      

      output_low(CLK);
      delay_ms(1);
      output_high(CLK);
      current_step++;
      delay_ms(speed);
      
    }else{
    
      output_low(DIR);
      
      
      output_low(CLK);
      delay_ms(1);
      output_high(CLK);
      current_step--;
      delay_ms(speed);
   }

   delay_ms(50);
}


void rotate(unsigned int8 old_pos, unsigned int8 new_pos){
   
   unsigned int8 clkwise_count = 0;
   unsigned int8 anticlk_count = 0;
   
   unsigned int8 count=old_pos;
   
   //printf("Function Called");
   
    
   while(1){
         count= (count + 1);
         //count = count%256 ;
         clkwise_count++;
         if(count==new_pos) break;
   }
   
   count=old_pos;
   
  
   while(1){
         count-- ;
         anticlk_count++;
         //if(count==0) count=256;
         if(count==new_pos) break;
   }
   
   if(clkwise_count < anticlk_count){
      give_pulse(clkwise_count, 1);
   }else{
      give_pulse(anticlk_count, 0);
   }
   
   //printf("%u  %u \n", clkwise_count, anticlk_count);
   //give_pulse(clkwise_count,1);
}

void save_channel(int8 ch_number,unsigned int8 steps ){
   
    //calculate the postion to save
    //int8 save_position=ch_number;
    
    //save data
   write_eeprom(ch_number,steps);
   
//!   printf("\n %x",read_eeprom(ch_number));
//!   printf("\n %x",read_eeprom(ch_number+1));
   

}

unsigned int8 read_channel_position(int8 ch_number){
   
   return read_eeprom(ch_number);

}


void select_channel(int8 ch_number){

   //location FF is used to currently viewing channel
   
   write_eeprom(CUR_CH_LOC,ch_number);
   
   //rotate the antenna
   
   //get the desired location
   unsigned int8 desired_loc=read_channel_position(ch_number);
   printf("%u\n",desired_loc);
   printf("%u\n",current_step);
   
   //rotate to the location
   rotate(current_step,desired_loc);
   
   //set the current channel
   current_ch=ch_number;
   
   

}

int decode()
{
   int1 code[16];
   int16 decoded_code = 0;
   int index = 15;
   int y = 0;
   
   int x = 0;
   for (x = 0; x<16; x++)
   {
      int y = x +18;
      
      if (array[y] < 800&&600 < array[y])
      {
         code[x] = 0;
      }else if (array[y] < 1500&&1300 < array[y])

      code[x] = 1;
   }
   
   newpulse = 0;

   
   for (y = 0; y < 16; y++)
   {
      // printf (" % u ", code[y]);
      int16 tmp = code[y];
      int16 tmp2 = pow (2, index);
      decoded_code = tmp * tmp2 + decoded_code;
      index--;
   }

   
   printf (" %Lu ", decoded_code);
   switch (decoded_code)
   {
      
      case 255 :{ select_channel(0); printf("0"); break;}
      case 32895 : select_channel(1); break;
      case 16575 : select_channel(2); break;
      case 49215 : select_channel(3); break;
      case 8415 : select_channel(4); break;
      case 41055 : select_channel(5); break;
      case 24735 : select_channel(6); break;
      case 57375 : select_channel(7); break;
      case 4335 : select_channel(8); break;
      case 36975 : select_channel(9); break;
      
      //default : select_channel(102); printf("default"); break;
      default : printf("default"); break;
   }
   
   clear_interrupt(int_ccp1); //Important
      enable_interrupts(GLOBAL);
}

void menuPressed(){
   if (tuning_state) {
      printf("Channel select State");  
      
      //save the current channel in the persistence space
      save_channel(current_ch, current_step);
      tuning_state = 0;
      output_low(ENABLE);
   } else {
      printf("Tuning State");     
      tuning_state = 1;
      output_high(ENABLE);
   }  

 }

//called in the channel select mode
void upPressed(){
   printf("Up released");
   select_channel(current_ch + 1); 
}


void downPressed(){
   printf("Down released");
   select_channel(current_ch - 1);   
}


//polls the status of the device buttons
void check_buttons(){
//Buttons D4 D5 D6
int1 ch_select_mode_curstate = input(PIN_D5);
if(!ch_select_mode_curstate && ch_select_mode_prestate)
{ 
   menuPressed();
}

if (tuning_state) {
   up_button_pressed=input(PIN_D4);
   down_button_pressed=input(PIN_D6);
} else {
   int1 up_button_pressed_curstate = input(PIN_D4);
   int1 down_button_pressed_curstate = input(PIN_D6);
   if(!up_button_pressed_curstate && up_button_pressed_prestate)
   { 
      upPressed();
   } else if(!down_button_pressed_curstate && down_button_pressed_prestate)
   { 
      downPressed();
   }
   
   up_button_pressed_prestate = up_button_pressed_curstate;
   down_button_pressed_prestate = down_button_pressed_curstate;
}
   

ch_select_mode_prestate = ch_select_mode_curstate;

}



void main()
{

//setup_timer_0(RTCC_INTERNAL|RTCC_DIV_1|RTCC_8_bit);      //102 us overflow
setup_timer_0(RTCC_INTERNAL|RTCC_DIV_8|RTCC_8_BIT);      //102 us overflow
setup_timer_1(T1_INTERNAL|T1_DIV_BY_4);      //26.2 ms overflow
   setup_timer_3(T3_DISABLED|T3_DIV_BY_1);
   
   
   setup_ccp1(CCP_CAPTURE_RE);
   
   enable_interrupts(INT_TIMER0);
   enable_interrupts(INT_EXT);
   enable_interrupts(INT_CCP1);
   enable_interrupts(GLOBAL);

   

   //initialization
   int j=0;
   for(j=0;j<40;j++){
      array[j]=0;
   }
   
   initialize_motor();
   
   //rotating motor to its zero position
   give_pulse(255,1);  
   
   
   while(1){
   
      
      check_buttons () ;
      
      if (tuning_state) {
     
         if (up_button_pressed) {
                give_pulse_manual(1);
            } else if (down_button_pressed) {
                give_pulse_manual(0);
            }
      }
      
      if (newpulse)
      {
         decode() ;
      }
     
     
   }
   
}


