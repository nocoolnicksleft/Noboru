
#include <16F687.h>

#fuses INTRC_IO,NOWDT,NOPROTECT,NOPUT,NOBROWNOUT

#include <stdlib.h>

#use fast_io(A)
//#use fast_io(B)
#use fast_io(C)

#use delay(clock=8000000)

//#use rs232(stream=terminal, baud=9600, xmit=PIN_C6, rcv=PIN_C7, ERRORS)
#use i2c(slave,scl=PIN_B6,sda=PIN_B4,force_hw,stream=uplink)

#define TIMER1_50MSEC_OFF_10MHZ 30000
#define TIMER1_50MSEC_10MHZ 34285
#define TIMER1_100MSEC_10MHZ 3035
#define TIMER1_10MSEC_20MHZ 15536
#define TIMER1_1MSEC_20MHZ 60536
#define TIMER1_3MSEC_20MHZ 50536
#define TIMER1_5MSEC_20MHZ 40536

#define TIMER1_1MSEC_10MHZ 63036
#define TIMER1_1MSEC_8MHZ 63535

#define INT_PIN PIN_B5

#define LED_PIN PIN_C0


#define DISPLAYMODE_BARTOP 0
#define DISPLAYMODE_BARBOTTOM 1
#define DISPLAYMODE_ONEFREE 2
#define DISPLAYMODE_THREEFREE 3
#define DISPLAYMODE_PAN 4
#define DISPLAYMODE_LAST 4

int1 timeout1msec = 0;
int1 reconfigurenow = 0;
//int1 pop2c = 0;
int1 ledstatus = 0;
int minposition = 0;
int maxposition = 38;

/**********************************************************
/ Common Timer
/**********************************************************/
#INT_TIMER1
void timeproc()
{
   set_timer1(get_timer1() + TIMER1_1MSEC_8MHZ); 
   timeout1msec = 1;
}

/**********************************************************
/ CONFIGURATION DATA
/**********************************************************/

BYTE packet_address = 0;

#define MBIT_MAKE  0b01000000
#define MBIT_BREAK 0b10000000

#define MBIT_LEFT  0b00010000
#define MBIT_RIGHT 0b00100000

#define MBIT_ID_ROTARY 0x1
#define MBIT_ID_ROTARYPUSH 0x2

#define MOD1_AUT (1 << 4)
#define MOD1_ANI (1 << 5)
#define MOD1_CYC (1 << 6)
#define MOD1_RST (1 << 7)

/*
   
NAME |ADR | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
MOD1 | 00 |RST|CYC|ANI|AUT|DIS|DIS|DIS|DIS|
MOD2 | 01 | x | x | x | x | x | x | x | x |
VALU | 02 |VAL|VAL|VAL|VAL|VAL|VAL|VAL|VAL|
STAT | 03 | x | x | x | x | x | x | x |INT|
DATA | 04 |MOR|MAK|KEY|KEY|KEY|KEY|KEY|KEY|

MODE - Read / Write Mode Byte
	DIS = Display Mode
	AUT = Refresh Display Value On Rotate
	ANI = Animate Display
	CYC = Cycle Display Mode On Push
	RST = Set to 1 to Reset Event Buffer

VALU - Read / Write Display Value

STAT - Status Byte 
	INT - Event Waiting (RO)

DATA - Value Of last popped Event (only valid when STAT-INT or DATA-MOR were 1, 0 otherwise)

*/

struct TYPE_NOBORU_Data {

   // BYTE 0
   int DisplayMode : 4;
   int AutoDisplay : 1;
   int AnimatedDisplay : 1;
   int AutoCycleModes : 1;
   int Reset : 1;

   // BYTE 1
   int SpareSetting : 8;

   // BYTE 2
   int Value : 8;

   // BYTE 3
   int Interrupt : 1;
   int KeyState : 1;
   int SpareStatus : 6;

   // BYTE 4
   int8 Data : 8;

   // BYTE 5
   int8 Magic;
   
};

union config_type {
 struct TYPE_NOBORU_Data Bits;
 int8 Code[6];
} config;

void eeprom_read(unsigned int8 address, unsigned int8 count, void *dataptr)
{
     int i;
	 for (i=0;i<count;i++) 
		*(dataptr+count) = read_eeprom(address + count);
}

void eeprom_write(unsigned int8 address, unsigned int8 count, void *dataptr)
{
     int i;
	 for (i=0;i<count;i++) 
		write_eeprom(address + count, *(dataptr+count));
}

#define EEPROM_CONFIG 0
#define MAGICNUMBER 49

void write_config()
{
	eeprom_write(EEPROM_CONFIG,sizeof(config_type),&config);
}

void read_config()
{
	eeprom_read(EEPROM_CONFIG,sizeof(config_type),&config);
	if (config.Bits.Magic != MAGICNUMBER) {
		config.Bits.DisplayMode = DISPLAYMODE_THREEFREE;
		config.Bits.Autodisplay = 1;
		config.Bits.AnimatedDisplay = 0;
		config.Bits.AutoCycleModes = 1;
		config.Bits.Reset = 0;
		config.Bits.SpareSetting = 0;
		config.Bits.Value = 2;
		config.Bits.Interrupt = 0;
        config.Bits.KeyState = 0;
		config.Bits.Data = 0;
		config.Bits.Magic = MAGICNUMBER;
		write_config();
	}
}

//**********************************************************
// I2C ADDRESS CONFIGURATION
//**********************************************************
int8 i2c_address = 1;

void read_address()
{


	i2c_address = 0b00100000;
/*
	if (!(input(PIN_C6))) i2c_address |= 0b00000010;
	if (!(input(PIN_C7))) i2c_address |= 0b00000100;
	if (!(input(PIN_A2))) i2c_address |= 0b00001000;
*/
    i2c_SlaveAddr(uplink,i2c_address);
 
}


//**********************************************************
// DISPLAY MODE
//**********************************************************
void setdisplaymode(int newmode)
{
		config.Bits.Displaymode = newmode;
		if (config.Bits.Displaymode == DISPLAYMODE_BARTOP)
    	{
			minposition = 0;
			maxposition = 40;
    	} 
    	else if (config.Bits.Displaymode == DISPLAYMODE_BARBOTTOM)
    	{
			minposition = 0;
			maxposition = 40;
    	} 
    	else if (config.Bits.Displaymode == DISPLAYMODE_ONEFREE)
    	{
			minposition = 0;
			maxposition = 40;
    	} 
    	else if (config.Bits.Displaymode == DISPLAYMODE_THREEFREE)
    	{
			minposition = 2;
			maxposition = 36;
    	} 
    	else if (config.Bits.Displaymode == DISPLAYMODE_PAN)
    	{
			minposition = 2;
			maxposition = 36;
    	} 
/*
    	else if (config.Bits.Displaymode == DISPLAYMODE_DIP)
    	{
			minposition = 0;
			maxposition = 38;
    	}
*/
	    if (config.Bits.Value < minposition) config.Bits.Value = minposition;
        if (config.Bits.Value > maxposition) config.Bits.Value = maxposition;
}


//**********************************************************
// QUEUE HANDLING
//**********************************************************
#define QUEUE_LENGTH 10
char queue[QUEUE_LENGTH];
int8 queue_start = 0;
int8 queue_stop = 0;
int8 popone = 0;
// int1 pop2c = 0;
int1 bytesending = 0;

int8 get_queue_length()
{
  if (queue_start == queue_stop) return 0;
  if (queue_start < queue_stop) return  (queue_stop - queue_start);
  else return  (QUEUE_LENGTH-queue_start) + queue_stop;
}

void push(char c)
{
// fprintf(terminal,"push %c qlen: %u qstart: %u qstop: %u \r\n",c,get_queue_length(),queue_start,queue_stop);

 disable_interrupts(global);

 queue[queue_stop] = c;
 if (queue_stop == (QUEUE_LENGTH-1)) {
   if (queue_start>0) {
    queue_stop = 0;
   }
 } else {
  if (queue_stop == (queue_start - 1)) {
  } else {
   queue_stop++;
  }
 } 

 enable_interrupts(global);

 if (!config.Bits.AutoDisplay)
 {
 	ledstatus = 1;
 	output_high(INT_PIN); 
 	config.Bits.Interrupt = 1;
 }
}

char pop()
{
 char c = 0;
 if (queue_start != queue_stop) {
  disable_interrupts(global);

  c = queue[queue_start];
//  fprintf(terminal,"pop %c qstart: %u qstop: %u \r\n",c,queue_start,queue_stop);
  if (queue_start == (QUEUE_LENGTH-1)) queue_start = 0;
  else {
    queue_start++;
  }
  
enable_interrupts(global);

 }
 return c;
}


#BIT SSPOV = 0x14.6
/*Indicates overflow - BIT 6 - SSPCON: SYNC SERIAL PORT CONTROL REGISTER (ADDRESS 14h)*/

#BIT BF = 0x94.0
/*Buffer Full - BIT 0 - SSPSTAT: SYNC SERIAL PORT STATUS REGISTER ADDRESS: 94h)*/


//**********************************************************
// I2C HANDLING
//**********************************************************
#INT_SSP
void ssp_interrupt ()
{
   BYTE incoming, state;

	state = i2c_isr_state(uplink);
    /*
        State is an 8 bit int
        0 = Adress match received with RW bit clear
        1 - 0x7F = Master has written data, i2c_read will immediately return the data
        0x80 - 0xFF = Transmission completed and acknowledged; respond with i2c_write
    */

	if(state < 0x80)							//Master is sending data
	{
		incoming = i2c_read(uplink);
		if(state == 1)	{						//First received byte is address
            if (incoming < 5)
			 packet_address = incoming;
            else 
             packet_address = 0;

        } else if(state == 2) {					//Second received byte is data

			config.Code[packet_address] = incoming;

        	if (packet_address == 0) { // NEW CONFIGURATION
				reconfigurenow = 1;
			}
	        if (packet_address == 1) { // NEW CONFIGURATION
				reconfigurenow = 1;
			}
            if (packet_address == 2) { // NEW VALUE
				reconfigurenow = 1;		 	
			}



	    }

	} else 	{ // Master is requesting data 

   		// DATA FROM QUEUE REQUESTED
		if (packet_address == 4) { 

			if (queue_start != queue_stop) {

            	i2c_write(uplink,queue[queue_start]);

				if (queue_start == (QUEUE_LENGTH-1)) queue_start = 0;
  				else queue_start++;
  
	  			if (queue_start == queue_stop) {
					output_low(INT_PIN);
					ledstatus = 0;
					config.Bits.Interrupt = 0;
 	  			}

			} else {
				i2c_write(uplink,0);

				output_low(INT_PIN);
				ledstatus = 0;
				config.Bits.Interrupt = 0;

 			}

        } else if (packet_address == 2) { 

			i2c_write(uplink,config.Code[packet_address]);

			output_low(INT_PIN);
			ledstatus = 0;
			config.Bits.Interrupt = 0;

		} else {
            i2c_write(uplink,config.Code[packet_address]);
		}
		packet_address++;
	}

}



//**********************************************************
// 1x8 STATIC KEYBOARD HANDLING (ROTARY SWITCH PUSH)
//**********************************************************
void dorotarypush()
{
  static int8 keystate1 = 0xFF;
  int8 keystate;
  int8 keydelta;

//  keystate = (input_b() & (1 << 7));
  keystate = (input(PIN_B7)  << 7);
  if (keystate != keystate1) 
  {
    keydelta = (keystate ^ keystate1);
    if (keydelta & (1 << 7)) {
      if (keystate & (1 << 7)) {
		push (MBIT_ID_ROTARYPUSH | MBIT_BREAK );
        config.Bits.KeyState = 0;
      } else {
		push (MBIT_ID_ROTARYPUSH | MBIT_MAKE );
		config.Bits.KeyState = 1;
      }
    }
    keystate1 = keystate;
  }
  
}

//**********************************************************
// ROTARY SWITCH HANDLING
//**********************************************************
void dorotary(int8 id)
{
     int8 yy;
     int8 abn;
     static int8 abn1[3];
   
     // if (id > 1) abn = (input_c() & 0b00000011);  // rotary 2 liegt auf port c
     // else abn = (input_a() & (0b00000011 << (id*2)) ) >> (id*2); // die anderen auf port a

	 abn = ((input_a() & 0b00110000) >> 4);

     // der folgende algorithmus stammt aus dem datenblatt -
     // der zweite zweig mit den alternativen kombinationen wurde mit absicht weggelassen,
     // sonst werden die umschaltungen doppelt gezählt!!!
     if (abn != abn1[id]) {
      yy = (abn ^ abn1[id]);
      if ((abn & 0b00000001) == ((abn & 0b00000010) >> 1)) {
        if (yy == 0b00000001) push(MBIT_ID_ROTARY | MBIT_MAKE | MBIT_RIGHT | id);
        else if (yy == 0b00000010) push(MBIT_ID_ROTARY | MBIT_MAKE | MBIT_LEFT | id);
      } 
      abn1[id] = abn;
     }
}

//**********************************************************
// LED HANDLING
//**********************************************************
int charlie[20][2] =
 {
  {0b11111000,0b00000010}, // 00 
  {0b11111000,0b00000100}, // 01

  {0b11110010,0b00000100}, // 02
  {0b11110010,0b00001000}, // 03

  {0b11100110,0b00001000}, // 04
  {0b11100110,0b00010000}, // 05

  {0b11001110,0b00010000}, // 06
  {0b11001110,0b00100000}, // 07

  {0b11110100,0b00000010}, // 08
  {0b11110100,0b00001000}, // 09

  {0b11101010,0b00000100}, // 10
  {0b11101010,0b00010000}, // 11

  {0b11010110,0b00001000}, // 12
  {0b11010110,0b00100000}, // 13

  {0b11101100,0b00000010}, // 14
  {0b11101100,0b00010000}, // 15
  

  {0b11011010,0b00000100}, // 16
  {0b11011010,0b00100000}, // 17

  {0b11011100,0b00000010}, // 18
  {0b11011100,0b00100000}  // 19
  
 };


void setdisplay(int nbr, int val)
{
	set_tris_c(charlie[19-nbr][0]);
    if (val) output_c(charlie[19-nbr][1] | ledstatus);
    else output_c(0 | ledstatus);
}

void doled()
{
	static int8 currentled = 0;

	if (config.Bits.Displaymode == DISPLAYMODE_BARTOP)
    {
        if (config.Bits.Value <= 1) setdisplay(currentled,0);
    	else if (((config.Bits.Value >> 1) -1) >= currentled) setdisplay(currentled,1);
    	else  setdisplay(currentled,0);

    	if (currentled < 19) currentled++;
    	else currentled = 0;
    } 
    else if (config.Bits.Displaymode == DISPLAYMODE_BARBOTTOM)
    {
        if (config.Bits.Value <= 1) setdisplay(currentled,1);
    	else if (((config.Bits.Value >> 1) +1) <= currentled) setdisplay(currentled,1);
    	else setdisplay(currentled,0);

    	if (currentled < 19) currentled++;
    	else currentled = 0;
    } 
    else if (config.Bits.Displaymode == DISPLAYMODE_ONEFREE)
    {
		if (config.Bits.Value <= 1) setdisplay(currentled,0);
        else if (currentled == 1) setdisplay((config.Bits.Value >> 1) - 1,1);
        else setdisplay((config.Bits.Value >> 1) - 1,0);

    	//if (currentled < 3) currentled++;
    	//else 
		currentled = 1;
		
    } 
    else if (config.Bits.Displaymode == DISPLAYMODE_THREEFREE)
    {
        if (currentled == 1) setdisplay((config.Bits.Value >> 1) -1,1);
        if (currentled == 2) setdisplay((config.Bits.Value >> 1),1);
        if (currentled == 3) setdisplay((config.Bits.Value >> 1) + 1,1);

    	if (currentled < 3) currentled++;
    	else currentled = 0;
    } 
    else if (config.Bits.Displaymode == DISPLAYMODE_PAN)
    {
        if (currentled == 1) setdisplay(0,1);
        if (currentled == 2) setdisplay((config.Bits.Value >> 1),1);
        if (currentled == 3) setdisplay(19,1);

    	if (currentled < 3) currentled++;
    	else currentled = 0;
    } 
/*
    else if (config.Bits.Displaymode == DISPLAYMODE_DIP)
    {
  		read_address();

    	if (i2c_address & (1 << currentled)) setdisplay(currentled,1);
    	else  setdisplay(currentled,0);

    	if (currentled < 3) currentled++;
    	else currentled = 0;
    }
*/
}


/**********************************************************
/ MAIN
/**********************************************************/

void main () {

  int i;

  setup_oscillator(OSC_8MHZ);

  output_a(0x00);
  output_c(0x00);

  set_tris_a(0b00110100); // rotary switch (bit 4,5) dip (bit 2)
  set_tris_c(0b11000000); // led (bit 0) ledrow (bit 1-5) dip (bit 6,7)

  read_address();
  read_config();

  output_low(INT_PIN);
  ledstatus = 1;

  for (i=0;i<20;i++)
  {
   setdisplay(i,1);
   delay_ms(30);
  }
  setdisplay(0,1);

  output_low(INT_PIN);
  ledstatus = 0;


  setup_timer_1(T1_INTERNAL | T1_DIV_BY_1 );    // Start timer 1
  enable_interrupts(INT_TIMER1);
  enable_interrupts(INT_SSP);
  enable_interrupts(global);

  setdisplaymode(config.Bits.Displaymode);

 /**********************************************************
 / STATE LOOP
 /**********************************************************/
  for (;;) {

	if (SSPOV || BF)
	{
		SSPOV = 0;
		BF = 0;
	}
/*
	if (pop2c) {
     pop2c = 0;
     if (queue_start != queue_stop) {
	  i2c_write(uplink,queue[queue_start]);
      pop();
	  if (queue_start == queue_stop) {
		output_low(INT_PIN);
        ledstatus = 0;
		config.Bits.Interrupt = 0;
 	  }
     } 
    }
*/
    if (popone) {
     popone = 0;
     bytesending = 0;
     pop();
     //output_low(INT_PIN);
    }

    if (reconfigurenow) {
        reconfigurenow = 0;
		setdisplaymode(config.Bits.Displaymode);
    }
/*
    if (queue_start != queue_stop) {
      if (!bytesending) {
       WRITE_SSP(queue[queue_start]);
       bytesending = 1;
       output_high(INT_PIN); 
      }
    }
*/
   if (config.Bits.Autodisplay) {
    if (queue_start != queue_stop) {
        switch (queue[queue_start]) {
			case (MBIT_ID_ROTARY | MBIT_MAKE | MBIT_LEFT):
					if (config.Bits.Value > minposition) {
						config.Bits.Value--;
						output_high(INT_PIN); 
 						ledstatus = 1;
        				config.Bits.Interrupt = 1;
					}
				break;
			case (MBIT_ID_ROTARY | MBIT_MAKE | MBIT_RIGHT):
					if (config.Bits.Value < maxposition) {
						config.Bits.Value++;
						output_high(INT_PIN); 
 						ledstatus = 1;
        				config.Bits.Interrupt = 1;
					}
				break;
			case (MBIT_ID_ROTARYPUSH | MBIT_BREAK):
				if (config.Bits.AutoCycleModes) {
                	if (config.Bits.Displaymode == DISPLAYMODE_LAST) setdisplaymode(0);
                	else setdisplaymode(config.Bits.Displaymode + 1);
                    write_config();
				}
				break;

		}
		popone = 1;
    }
   }

    // ***********************************
    // alle 1 ms 
    // ***********************************
    if (timeout1msec) {
     timeout1msec = 0;
     doled();
     dorotary(0);
     dorotarypush();
     
     /*
     if (timeout10msec > 2)
     {
		timeout10msec = 0;
     } else {
		timeout10msec++;
     }
     */

    } // if (timeout1msec)


  } // for (;;)

} // main()
