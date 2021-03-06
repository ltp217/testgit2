#include "EM78P301N.h"

#define DISI()                _asm{disi}
#define WDTC()                _asm{wdtc}
#define NOP()                 _asm{nop}
#define ENI()                 _asm{eni}
#define SLEP()                _asm{slep}
#define LOW_BAT_VOLT_TH       0x955                                 //(3.5V/2)/3V*4096=2389
#define MID_BAT_VOLT_TH       0xA22                                 //(3.8V/2)/3V*4096=2594
#define HIG_BAT_VOLT_TH       0xB33                                 //(4.2V/2)/3V*4096=2389
#define FAULT_BAT_VOLT        0x800                                 //(3.0V/2)/3V*4096=2048
#define LOW_BAT_VOLT          0x888                                 //(3.2V/2)/3V*4096=2184
#define STABLE_BAT_VOLT       0x9DD                                 //(3.7V/2)/3V*4096=2525
#define LOW_LOAD_VOLT         0x888                                 //(3.2V/2)/3V*4096=2184
#define SHORT_LOAD_VOLT       0x400                                 //(1.5V/2)/3V*4096=1024
#define STABLE_LOAD_VOLT_MAX  0xA00                                 //(3.75V/2)/3V*4096=2560
#define STABLE_LOAD_VOLT_MIN  0x9BB                                 //(3.65V/2)/3V*4096=2491
#define CHARGE_BAT_VOLT_TH    0xB11                                 //(4.15V/2)/3V*4096=2833
#define WAKEUP_LOAD_VOLT      0x155                                 //(0.5V/2)/3V*4096=341
#define DUTY(n)               do{DT1 = n; T1EN = 1;}while(0)        //n 0~100
#define MOS_ON                0                                     //打开MOS
#define MOS_OFF               100                                   //关闭MOS
#define VBAT37                0x9DD                                 //(3.7V/2)/3V*4096=2525  --100
#define VBAT38                0xA22                                 //(3.8V/2)/3V*4096=2594  --94
#define VBAT39                0xA66                                 //(3.9V/2)/3V*4096=2662  --90
#define VBAT40                0xAAB                                 //(4.0V/2)/3V*4096=2731  --85
#define VBAT41                0xAEF                                 //(4.1V/2)/3V*4096=2799  --81
#define VBAT42                0xB33                                 //(4.2V/2)/3V*4096=2867  --77
#define MOS_37_38             97                           
#define MOS_38_39             92                            
#define MOS_39_40             88                            
#define MOS_40_41             83                            
#define MOS_41_42             79                            

#define uchar unsigned char 
#define ushort unsigned short 
#define uint unsigned int 

uchar g_fault_state        @0X20:bank 0;  //故障状态字
ushort g_batteryvolt       @0X21:bank 0;
uchar g_batteryvolt_l      @0X21:bank 0;
uchar g_batteryvolt_h      @0X22:bank 0;
ushort g_loadvolt          @0X23:bank 0;
uchar g_loadvolt_l         @0X23:bank 0;
uchar g_loadvolt_h         @0X24:bank 0;
uchar g_keyval             @0X25:bank 0;
uchar g_time1ms_cnt        @0X26:bank 0;
uchar g_time50ms_cnt       @0X27:bank 0;
uchar g_time50ms_flag      @0X28:bank 0;
uchar g_time1s_cnt         @0X29:bank 0;
uchar g_time1s_flag        @0X2A:bank 0;
uchar g_time2s_flag        @0X2B:bank 0;
bit g_led_r                @0X2C @0:bank 0;
bit g_led_g                @0X2C @1:bank 0;
bit g_led_b                @0X2C @2:bank 0;
bit g_led_onoff_status     @0X2C @3:bank 0;
bit g_led_occupied         @0X2C @7:bank 0;
uchar g_led_light_times    @0X2D:bank 0;
uchar g_keypress_times     @0X2E:bank 0;
uchar g_keypress_maxtime   @0X2F:bank 0;
//uchar g_led_onoff          @0X30:bank 0;

uchar g_cur_state          @0X31:bank 0;
uchar g_next_state         @0X32:bank 0;

extern int IntVecIdx; //occupied 0x10:rpage 0

//将RAM区通用寄存器清零
void Clr_Ram(void)        
{
    RSR = 0x10;               //BANK 0 的0x10寄存器
    do
    {  
        R0 = 0;         
        RSR++;
        if (RSR == 0x40)
        {    
           RSR = 0x60;
        }
     }while(RSR < 0x80);

}

void _intcall ALLInt(void) @ int 
{     
    switch(IntVecIdx)
    {
        case 0x16:
        if(PWM2IF == 1)
        {
            PWM2IF = 0;    //清PWM1中断标志位
            g_time1ms_cnt++;
            if(g_time1ms_cnt == 50)
            {
                g_time1ms_cnt = 0;
                g_time50ms_flag = 1;
                g_time50ms_cnt++;
                if(g_time50ms_cnt == 20)
                {
                    g_time50ms_cnt = 0;
                    g_time1s_flag = 1;

                    if(g_time2s_flag==0)
                    {
                        g_time1s_cnt++;
                    }
                    if(g_time1s_cnt == 2)
                    {
                        g_time1s_cnt = 0;
                        g_time2s_flag = 1;
                    } 
                }
            }
        }
        break;
    }     
}

void _intcall PWM2P_l(void) @0x15:low_int 6
{
    _asm{MOV A,0x2};
}

void battery_volt_sample(void)     //电池电源采样
{    
    P7CR = 0x01;
    AISR = 0x20;        //P71/ADC5引脚作为ADC5输入口
    ADCON = 0x0D;       //选择ADC5输入口
    
    ADRUN = 1;
    while(ADRUN == 1);
    g_batteryvolt_h = ADDATA1H;
    g_batteryvolt_l = ADDATA1L;
    P7CR = 0x00;
}

void load_volt_sample(void)   //负载电压采样
{
    P5CR = 0X21;
    AISR = 0X40;        //P55/ADC6引脚作为ADC6输入口 
    ADCON = 0x0E;       //选择ADC6输入口
    ADRUN = 1;
    while(ADRUN == 1);   
    g_loadvolt_h = ADDATA1H;
    g_loadvolt_l = ADDATA1L;
}

void led_disp(void)    //LED控制
{
    if(g_led_light_times >= 1) 
    {
        if(g_led_onoff_status)   
        {
            if(g_led_light_times != 0xff)
            {
                g_led_onoff_status = 0;
            }
            g_led_r = 1;
			g_led_g = 1;
			g_led_b = 1;
            //g_led_onoff = 0;          //灯亮
        }
        else
        {
			g_led_r = 0;
			g_led_g = 0;
			g_led_b = 0;
            g_led_onoff_status = 1;
            //g_led_onoff = 1;         //灯灭            
            g_led_light_times--;
        }
    
		//else
		//{
			//g_led_onoff = 1;             //灯灭
		//}        
			
		if(g_led_r)
		{
			P70 = 0;           //红灯
		}
		else
		{
			P70 = 1;                    //没被控制就灭灯
		}
		
		if(g_led_g)
		{
			P71 = 0;          //绿灯
		}
		else
		{
			P71 = 1;					 //没被控制就灭灯
		}
		if(g_led_b)
		{
			P51 = 1;         //蓝灯
		}
		else
		{
			P51 = 0;					 //没被控制就灭灯
		}
	}
}

void led_ctrl_by_voltage(void)        //不同电压区间灯闪亮
{
     if(g_loadvolt < LOW_BAT_VOLT_TH)      
    {
        g_led_r = 1;                  //红灯   
        g_led_g = 0;                  //绿灯    
        g_led_b = 0;                  //蓝灯        
    }
    else if(g_loadvolt < MID_BAT_VOLT_TH)    
    {
        g_led_r = 1;                  //红灯
        g_led_g = 0;                  //绿灯
        g_led_b = 1;                  //蓝灯
    }
    else
    {
        g_led_r = 0;                  //红灯 
        g_led_g = 0;                  //绿灯
        g_led_b = 1;                  //蓝灯
    }        
    
    g_led_onoff_status = 1;           //灯亮
    g_led_light_times = 0xff;   
}

void mcu_init(void)   //MCU初始化
{

    WDTC();
    DISI();
    SCR = 0X7F;         //选择4MHz,TIMER 选择主频
    
    Clr_Ram();
    P6CR = 0;           //P67设为输出
    TMRCON = 0X22;      //PWM1预分频比设为1：4，PWM2预分频比设为1：16
    PWMCON = 0X01;      //使能PWM1输出口
    PRD1 = 99;          //周期=1/4*(99+1)*4=100us
    DUTY(MOS_OFF);      //关闭MOS管
    PRD2 = 249;         //周期=1/4*(249+1)*16=1ms
    IMR = 0X10;         //使能PWM2中断
    T2EN = 1;           //PWM2定时开始
    ENI();    
    
    P5CR = 0X21;        //P50,P55设为输入 ,P51设为输出
    P5PHCR = 0XFC;      //P50,P51上拉
    P51 = 0;            //默认蓝灯关
    
    ADOC = 0x4;         //内部参考电压3V  
    
    do
    {
        battery_volt_sample();
    }
    while(g_batteryvolt < LOW_BAT_VOLT);

    P7CR = 0x00;
    AISR = 0x00;
    g_led_r = 1;                          //红灯
    g_led_g = 1;                          //红灯
    g_led_b = 1;                          //蓝灯
    g_led_onoff_status = 1;
    g_led_occupied = 0;
    g_led_light_times = 0x3;
    
    do
    {
        if(g_time1s_flag == 1)
        {
            g_time1s_flag = 0;
            led_disp();
        }  
    }while(g_led_light_times != 0);

    g_keyval = P50;
    
    if(g_keyval == 0)
    {
        g_next_state = 0x01; 
    }
    else
    {
        g_next_state = 0x08;
    }
    
    g_fault_state = 0x00;
}

void main(void)
{
    uchar temp_keyval;
  
    mcu_init();
    g_time2s_flag=1;
    temp_keyval = 1;
  
    while(1)
    {
        battery_volt_sample();
 
        g_cur_state = g_next_state;

        switch(g_cur_state)
        {
            case 0x01:                                 //正常模式
                if(g_keypress_maxtime > 0)
                {
					led_ctrl_by_voltage();
                    if(g_keypress_maxtime >= 200)       //判断吸烟超过10s情况
                    {
                        DUTY(MOS_OFF);
                        if(g_led_occupied == 0)        //判断LED是否被占用
                        {
                            g_led_occupied = 1;        //占用灯
                            g_led_light_times = 0x8;   //闪烁8次
                        }  
                       
                        if(g_led_light_times == 0)     //解除占用
                        {
                            g_led_occupied = 0;
                        }
                        break;   
                    }     
                    
                    DUTY(MOS_ON);

                    load_volt_sample();

                    if(g_loadvolt < SHORT_LOAD_VOLT)           //检测雾化器短路故障
                    {
                        DUTY(MOS_OFF);
                        g_led_occupied = 0;
                        g_fault_state = 0x08;
                        g_next_state = 0x02;
                            
                        break;
                    }
                    
                    if(g_batteryvolt < LOW_BAT_VOLT)        //检测电池低压故障 
                    {
                        g_led_occupied = 0;
                        g_fault_state = 0x04;
                        g_next_state = 0x02;
                    } 
                    else
                    {
                        if(g_batteryvolt <= VBAT37)
						{
							DUTY(MOS_ON);
						}
						if(g_batteryvolt > VBAT37 && g_batteryvolt <= VBAT38)
						{
							DUTY(MOS_37_38);
						}
						if(g_batteryvolt > VBAT38 && g_batteryvolt <= VBAT39)
						{
							DUTY(MOS_38_39);
						}
						if(g_batteryvolt > VBAT39 && g_batteryvolt <= VBAT40)
						{
							DUTY(MOS_39_40);
						}
						if(g_batteryvolt > VBAT40 && g_batteryvolt <= VBAT41)
						{
							DUTY(MOS_40_41);
						}
						if(g_batteryvolt > VBAT41 && g_batteryvolt <= VBAT42)
						{
							DUTY(MOS_41_42);
						}
                        g_led_light_times = 0xff;
                    }                 
                }
                else
                {         
                    g_led_occupied = 0;
                    g_next_state = 0x08;
                }
                     
            break;
                
            case 0x02:                        //故障模式
                DUTY(MOS_OFF);
                
                if(g_fault_state == 0x02)       //过充保护
                {
                    if(g_led_occupied == 0)
                    {
                        g_led_r = 0;
                        g_led_g = 0;
                        g_led_b = 1;
                        g_led_onoff_status = 1;
                        g_led_occupied = 1;
                        g_led_light_times = 0x14;
                    } 
                    
                    if(g_led_light_times == 0)
                    {
                        g_led_occupied = 0;
                        g_next_state = 0x04;
                    }
                }
                else if(g_fault_state == 0x04)  //低压保护
                {
                    if(g_led_occupied == 0)
                    {
                        g_led_r = 1;
                        g_led_g = 0;
                        g_led_b = 0;
                        g_led_onoff_status = 1;
                        g_led_occupied = 1;
                        g_led_light_times = 0xa;
                    }       
                    
                    if(g_led_light_times == 0)
                    {
                        if(g_batteryvolt > LOW_BAT_VOLT)  
                        {
                            g_led_occupied = 0;
                            g_next_state = 0x01;
                        }
                    }      
                }
                else if(g_fault_state==0x08)    //发热丝短路保护
                {
                    if(g_led_occupied == 0)
                    {
                        g_led_r = 1;
                        g_led_g = 1;
                        g_led_b = 1;
                        g_led_onoff_status = 1;
                        g_led_occupied = 1;
                        g_led_light_times = 0x3;
                    }         
            
                    if(g_led_light_times == 0)
                    {
                        g_led_occupied = 0;
                        g_next_state = 0x01; 
                    }  
                }     
                else if(g_fault_state == 0x10)    //充电器短路保护
                {
                    if(g_led_occupied == 0)
                    {
                        g_led_r = 1;
                        g_led_g = 1;
                        g_led_b = 1;
                        g_led_onoff_status = 1;
                        g_led_occupied = 1;
                        g_led_light_times = 0x6;
                    }    
                    
                    if(g_led_light_times == 0)
                    {
                        if(g_batteryvolt > LOW_BAT_VOLT)    
                        {
                            g_led_occupied = 0;
                            g_next_state = 0x04;
                        }  
                    }      
                }    
                else if(g_fault_state==0x20)    //过渡进入充电状态		
                {
                    if(g_led_occupied == 0)
                    {
                        g_led_r = 1;
                        g_led_g = 0;
                        g_led_b = 1;
                        g_led_onoff_status = 1;
                        g_led_occupied = 1;
                        g_led_light_times = 3;
                    }        
              
                    if(g_led_light_times == 0)          
                    {
                        g_led_occupied = 0;
                        g_next_state = 0x04;
                    }
					else
					{
						g_led_occupied = 0;
						g_next_state = 0x01;
					}
				}
                 
           
            break;
    
            case 0x04:                                 //充电模式
                if(g_loadvolt < SHORT_LOAD_VOLT)   
                {
                    g_fault_state = 0x10;
                    g_next_state = 0x02;
                }
                else if(g_loadvolt > CHARGE_BAT_VOLT_TH)   
                {
                    g_fault_state = 0x02;
                    g_next_state = 0x02;
                }
                else
                {
                    DUTY(MOS_ON);
					led_ctrl_by_voltage();
                }
 
                
            
            break;
    
            case 0x08:                                  //睡眠模式
                DUTY(MOS_OFF);
				P70 = 1;                                //睡眠前关所有的灯
				P71 = 1;
				P51 = 0;
                P5CR = 0X01;                            //P50设为输入 ,P51,P55设为输出
                ADE6 = 0;
                //AISR = 0X00;                            //P55设为IO端口
                P5PDCR = 0xDF;                          //P55下拉
                P55 = 0;
                ISR1 = 0X02;                            //使能PORT5状态改变唤醒功能
                PORT5 = PORT5;                          //读取PORT5状态
                IDLE = 0;
                NOP();
                NOP();
                SLEP();                                 //进入睡眠
                NOP();
                NOP();

                g_keypress_maxtime = 0;
                load_volt_sample();             
#if 1              
                if(g_loadvolt < WAKEUP_LOAD_VOLT)       //由按键唤醒，进入吸烟状态
                {
                    g_next_state = 0x01;
                }
				else
#endif                                                  //由c_sens唤醒，充电器正常
                {
					g_fault_state = 0x20;
					g_next_state = 0x02;
					
                }
              
              break;
    
            default:
                g_next_state = 0x00;
            break;                
        }
        //while(!g_time50ms_flag);   
        if((g_time50ms_flag == 1)||(g_cur_state == 0X08))           //key处理
        {
            g_time50ms_flag = 0;
            g_keyval = P50;
            
            if(((temp_keyval == g_keyval)&&(g_keyval == 0))||(g_cur_state == 0X08))
            {
               // if(g_loadvolt > LOW_BAT_VOLT)
                {                    
                    
					if(g_keypress_maxtime == 255)
						g_keypress_maxtime = 255;
					else
						g_keypress_maxtime++;
                }
            }
            else if((temp_keyval == 0)&&(g_keyval == 1))
            {
                if(g_keypress_times == 0)
                {
                    g_time2s_flag = 0;
                }
                  
                if( g_keypress_maxtime < 40)
                g_keypress_times++;
                  
                g_keypress_maxtime = 0;
  
            }
            
            temp_keyval = g_keyval;
        }
        
        //while(!g_time1s_flag);
        if(g_time1s_flag == 1)           //指示灯处理
        {
            g_time1s_flag = 0;
            led_disp();
        }   

        if(g_time2s_flag == 1)
        {
            if(g_keypress_times >= 5)
            { 
                g_keypress_times = 0;
                 
                 DUTY(MOS_OFF);
                  
                if(g_cur_state == 0x00)
                {
                    g_next_state = 0x01;
                }
                else
                {
                    g_next_state = 0x00;
                }
            }
            g_keypress_times  =  0;
        }  
		
    }
}
