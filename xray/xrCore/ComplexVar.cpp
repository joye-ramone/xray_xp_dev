#include "stdafx.h"
#include "ComplexVar.h"

// #pragma optimize("gyts", off)

CComplexVar::CComplexVar()
{ 	
	SYSTEMTIME st;
	GetLocalTime(&st);		
	s32 sv = st.wMilliseconds ^ st.wYear + (st.wHour << 16) + (st.wDay << 24); // very weak seed
	m_rnd.seed(sv);
	m_mixer = NULL;  
}

CComplexVar::~CComplexVar()
{ 
	xr_free(m_mixer); 
}


int CComplexVarInt::get() const
{
	// MsgCB("$#CONTEXT: complex_var_int read %p, data = %p", this, m_mixer);		
	s64 *mixer = (s64*)m_mixer;
	s64 diff = mixer[1] - mixer[3];
	diff /= mixer[2];
	int result = (int)diff;
	CComplexVarInt *self = (CComplexVarInt*)this; // never const
	self->set(result);
	return result;
}
void CComplexVarInt::set(int value)
{
	s64 *mixer = (s64*)m_mixer;
	if (!mixer)
	{
		mixer = xr_alloc<s64>(4);
		mixer[0] = m_rnd.randI(0x7fffFFFF);
	}
	mixer[0] /= 2;  // полураспад )
	if (mixer[0] < 2)
	{
		s64 *upd = xr_alloc<s64>(4);
		xr_delete(mixer); mixer = upd;
		mixer[0] = m_rnd.randI(0x7fffFFFF);
	}

	mixer[2] = m_rnd.randI(3, 10);
	mixer[3] = ((s64) m_rnd.randI(-50000, 50000)) << 20;
	mixer[1] =  (s64)(value) * mixer[2] + mixer[3];
	m_mixer = mixer;

}



float CComplexVarFloat::get() const
{
	if (!m_mixer) return 0;
	f64 *mixer = (f64*)m_mixer;
	float result = (float) pow ( mixer[2] + mixer[1] * mixer[3] , 1.0l / 3.0l );
	CComplexVarFloat *self = (CComplexVarFloat*)this; // never const
	self->set(result);
	return result;
}
void CComplexVarFloat::set(float value)
{
	f64 *mixer = (f64*)m_mixer;
	if ((u32)mixer < 0x10000)
	{
		mixer = xr_alloc<f64>(4);
		mixer[0] = m_rnd.randF(1000.f) * 1e95;
	}
	
	mixer[0] = sqrt(mixer[0]); // значение полураспада переменой

	if (mixer[0] < 1)
	{
		f64 *upd = xr_alloc<f64>(4);
		xr_free(mixer);
		mixer = upd;
		mixer[0] = m_rnd.randF(1000.f) * 1e90;
	}
	double sqr = value * value * value;
	double p1 = sqr / PI + 1.0f; // first  part
	double p2 = sqr - p1;		 // second part
	mixer[2] = p1;
	mixer[1] = m_rnd.randF(1, 3e9);
	mixer[3] = p2 / mixer[1];	
	m_mixer = mixer;
}
