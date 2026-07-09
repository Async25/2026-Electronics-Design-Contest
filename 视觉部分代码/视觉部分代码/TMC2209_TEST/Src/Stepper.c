#include "stepper.h"

// 1. 初始化函数
void Stepper_Init(Stepper_t *stepper, TIM_HandleTypeDef *htim, uint32_t tim_channel, 
				  GPIO_TypeDef *dir_port,uint16_t dir_pin, GPIO_TypeDef *en_port, uint16_t en_pin,
                  Stepper_Mode_t mode) 
{
  stepper->htim = htim;
  stepper->tim_channel = tim_channel;
  stepper->dir_port = dir_port;
  stepper->dir_pin = dir_pin;
  stepper->run_mode = mode;

  stepper->timer_freq = 1000000.0f;

  stepper->state = STOP;
  stepper->current_pos = 0;

  stepper->target_speed = 0.0f;
  stepper->current_speed = 0.0f;
  stepper->accel = 1.0f;
  stepper->decel = 1.0f;

  HAL_GPIO_WritePin(en_port, en_pin, 
					GPIO_PIN_RESET);
}

void Stepper_Set_Speed_Profile(Stepper_t *stepper, float speed, float accel)
{	
	if(accel < 0)return;
	if (stepper->run_mode == MODE_POSITION)
	{
		if (speed < 0 || accel < 0)
			return;
		stepper->accel = accel;
		stepper->decel = accel;
		stepper->max_speed = speed;
		if (speed < 1.0f)
		{
			stepper->min_delay = 1000000; // 1s
		}
		else
		{
			stepper->min_delay = (uint32_t)(stepper->timer_freq / fabsf(speed));
		}
	}
	else if (stepper->run_mode == MODE_SPEED)
	{
		if (speed > 0)
		{
			stepper->direction = DIR_FORWARD;
		}
		else
		{
			stepper->direction = DIR_REVERSE;
		}
		HAL_GPIO_WritePin(stepper->dir_port, stepper->dir_pin, (GPIO_PinState)stepper->direction);
		stepper->accel = accel;
		stepper->target_speed = speed;
		if (fabsf(speed) < 1.0f)
		{
			stepper->min_delay = 1000000;
		}
		else
		{
			stepper->min_delay = (uint32_t)(stepper->timer_freq / fabsf(speed));
		}
	}
}

// 位置模式移动
void Stepper_Move(Stepper_t *stepper, int32_t relative_steps)
{
	if (stepper->run_mode == MODE_SPEED)
		return;

	if (stepper->state != STOP || relative_steps == 0)
		return;
	// 方向
	if (relative_steps > 0)
	{
		stepper->direction = DIR_FORWARD;
	}
	else
	{
		stepper->direction = DIR_REVERSE;
	}
	HAL_GPIO_WritePin(stepper->dir_port, stepper->dir_pin, (GPIO_PinState)stepper->direction);

	stepper->steps_to_move = labs(relative_steps);
	stepper->target_pos = stepper->current_pos + relative_steps;

	uint32_t decel_steps_needed = (stepper->max_speed * stepper->max_speed) / (2.0f * stepper->decel); // 加速步数及减速步数

	if (stepper->steps_to_move / 2 < decel_steps_needed)
	{ // 无法加速到最高速
		stepper->decel_start_step = stepper->steps_to_move / 2;
	}
	else
	{
		stepper->decel_start_step = stepper->steps_to_move - decel_steps_needed;
	}

	stepper->irq_call_count = 0;
	stepper->accel_count = 0;
	stepper->state = ACCEL;

	uint32_t initial_delay = (uint32_t)(0.676f * sqrtf(2.0f / stepper->accel) * stepper->timer_freq);
	if (initial_delay == 0)
		initial_delay = 1;
	if (initial_delay > 65535)
		initial_delay = 65535;

	stepper->step_delay = initial_delay;
	__HAL_TIM_SET_AUTORELOAD(stepper->htim, stepper->step_delay);
	__HAL_TIM_SET_COMPARE(stepper->htim, stepper->tim_channel, stepper->step_delay / 2);

	// 手动、分步启动
	__HAL_TIM_CLEAR_IT(stepper->htim, TIM_IT_UPDATE);
	HAL_TIM_PWM_Start(stepper->htim, stepper->tim_channel);
	__HAL_TIM_ENABLE_IT(stepper->htim, TIM_IT_UPDATE);
}

void Stepper_Run_Speed(Stepper_t *stepper)
{
	if (stepper->run_mode != MODE_SPEED)
		return;

	if (stepper->state != STOP)
		return;

	stepper->accel_count = 0;
	stepper->state = ACCEL;

	uint32_t initial_delay = (uint32_t)(0.676f * sqrtf(2.0f / stepper->accel) * stepper->timer_freq);
	if (initial_delay > 65535)
		initial_delay = 65535;
	else if (initial_delay < 100)
		initial_delay = 100;

	stepper->step_delay = initial_delay;
	__HAL_TIM_SET_AUTORELOAD(stepper->htim, stepper->step_delay);
	__HAL_TIM_SET_COMPARE(stepper->htim, stepper->tim_channel, stepper->step_delay / 2);

	// 手动、分步启动
	__HAL_TIM_CLEAR_IT(stepper->htim, TIM_IT_UPDATE);
	HAL_TIM_PWM_Start(stepper->htim, stepper->tim_channel);
	__HAL_TIM_ENABLE_IT(stepper->htim, TIM_IT_UPDATE);
}

// 5. 强制停止
void Stepper_Stop(Stepper_t *stepper)
{	
	if (stepper->state == STOP || stepper->run_mode != MODE_SPEED)
		return;
	stepper->state = STOP;
	stepper->step_delay = 65535;
	stepper->target_speed = 0;
	stepper->current_speed = 0;
	HAL_TIM_PWM_Stop(stepper->htim, stepper->tim_channel);
	__HAL_TIM_DISABLE_IT(stepper->htim, TIM_IT_UPDATE);
}

// 6. 中断处理函数 (算法核心)
void Stepper_TIM_IRQ_Handler(Stepper_t *stepper)
{       
    if (stepper->direction == DIR_FORWARD) 
	{
        stepper->current_pos++;
    } 

	else 
	{
        stepper->current_pos--;
    }

    uint32_t new_delay = stepper->step_delay;

		if (stepper->run_mode == MODE_SPEED) 
		{    
				
				float current_speed_real = stepper->timer_freq / (float)stepper->step_delay;
				
				float speed_change = stepper->accel * ((float)stepper->step_delay / stepper->timer_freq);
				
				if (stepper->step_delay > stepper->min_delay) 
				{
						if (stepper->step_delay - stepper->min_delay > 2) 
						{
								float next_speed = current_speed_real + speed_change;
								new_delay = (uint32_t)(stepper->timer_freq / next_speed);
						} else {
								new_delay = stepper->min_delay;
						}
				}
				else if (stepper->step_delay < stepper->min_delay) 
				{
						if (stepper->min_delay - stepper->step_delay > 2) 
						{
								float next_speed = current_speed_real - speed_change;
								if (next_speed < 10.0f) next_speed = 10.0f;
								
								new_delay = (uint32_t)(stepper->timer_freq / next_speed);
						} 
						
						else 
						{
								new_delay = stepper->min_delay;
						}
				}
		}
  		else if (stepper->run_mode == MODE_POSITION)
  		{
			stepper->irq_call_count++;
			if (stepper->irq_call_count >= stepper->steps_to_move) 
			{
				stepper->state = STOP;
				__HAL_TIM_DISABLE_IT(stepper->htim, TIM_IT_UPDATE);
				HAL_TIM_PWM_Stop(stepper->htim, stepper->tim_channel);
				return;
			}
			uint32_t steps_remaining = stepper->steps_to_move - stepper->irq_call_count; 

			switch (stepper->state) {
				case ACCEL:
					stepper->accel_count++;
					new_delay = stepper->step_delay - (2 * stepper->step_delay) / (4 * stepper->accel_count + 1);
					if (new_delay < stepper->min_delay) 
					{
						new_delay = stepper->min_delay;
						stepper->state = RUN;
					}
					if (stepper->irq_call_count >= stepper->decel_start_step) 
					{
						stepper->state = DECEL;
					}
					break;
				case RUN:
					new_delay = stepper->min_delay;
					if (stepper->irq_call_count >= stepper->decel_start_step) 
					{
						stepper->state = DECEL;
					}
					break;
				case DECEL:
					new_delay = stepper->step_delay + (2 * stepper->step_delay) / (4 * steps_remaining + 1);
					break;
				default:
					new_delay = stepper->step_delay;
					break;
			}
		}

    if (new_delay > 65535) new_delay = 65535;
    stepper->step_delay = new_delay;
    __HAL_TIM_SET_AUTORELOAD(stepper->htim, stepper->step_delay);
    __HAL_TIM_SET_COMPARE(stepper->htim, stepper->tim_channel, stepper->step_delay / 2);
}


