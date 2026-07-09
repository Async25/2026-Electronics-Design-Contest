#include "main.h"
#include "PID.h"

void PID_Init(PID_t *p)
{
	p->Error0 = 0;
	p->Error1 = 0;
	p->Error2 = 0;
	p->ErrorInt = 0;
	p->Out = 0;
	p->Target = 0;
	p->Actual = 0;
	p->Actual1 = 0;
}

void PID_Update(PID_t *p)
{
	float delta;
	
	p->Error2 = p->Error1;
	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;
	
	delta = p->Kp * (p->Error0 - p->Error1)
	      + p->Ki *  p->Error0
	      + p->Kd * (p->Error0 - 2 * p->Error1 + p->Error2);
	
	p->Out += delta;
	
	if (p->Out > 0) p->Out += p->OutOffset; 
	if (p->Out < 0) p->Out -= p->OutOffset;
	
	if (p->Out > p->OutMax) {p->Out = p->OutMax;}
	if (p->Out < p->OutMin) {p->Out = p->OutMin;}

	p->Actual1 = p->Actual;
}
