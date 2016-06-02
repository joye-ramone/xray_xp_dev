#pragma once

class XRCORE_API  CComplexVar
{
protected:
	CRandom    m_rnd;
	void    *m_mixer;

	IC   bool			check_ptr			() const			{ return (UINT_PTR)this > 0x10000; }

public:
	CComplexVar								();
	virtual ~CComplexVar					();	
};


class XRCORE_API CComplexVarInt:
	public CComplexVar
{
public:

	virtual int			get					() const;
	virtual void		set					(int value);

	virtual	int			operator=			(int v)				{ if (!check_ptr()) return 0; set(v); return get(); }
	IC					operator int		()	const			{ return check_ptr() ? get() : 0; }	
};


class XRCORE_API CComplexVarFloat:
	public CComplexVar
{
public:
	virtual float		get						() const;
	virtual void		set						(float value);

	virtual	float		operator=			(float v)			{ if (!check_ptr()) return 0.f; set(v); return get(); }
	IC					operator float		()	const			{ return check_ptr() ? get() : 0.f; }	
};