#include "Key.h"


int8_t Key_Scan(void){
		if(!HAL_GPIO_ReadPin(KEY1_GPIO_Port,KEY1_Pin))
				return 1;
		else if(!HAL_GPIO_ReadPin(KEY2_GPIO_Port,KEY2_Pin))
				return 2;
		else if(!HAL_GPIO_ReadPin(KEY3_GPIO_Port,KEY3_Pin))
				return 3;
		else if(!HAL_GPIO_ReadPin(KEY4_GPIO_Port,KEY4_Pin))
				return 4;
		else 
				return 0;
}