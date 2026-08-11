#ifndef __BSP_KEY_H
#define __BSP_KEY_H
//按键结构体

typedef struct{
	   unsigned int key_up  : 1;
     unsigned int key_mid : 1;
    
     unsigned int key_down: 1;
     unsigned int key_left: 1;
     unsigned int key_right:1;
}KEY_STATUS;

KEY_STATUS Key_Scan(void);
void key_test(void);


#endif


