#ifndef _PURE_H_AAA_
#define _PURE_H_AAA_

// messages
#define REG_PRIORITY_LOW		0x11111111ul
#define REG_PRIORITY_NORMAL		0x22222222ul
#define REG_PRIORITY_HIGH		0x33333333ul
#define REG_PRIORITY_CAPTURE	0x7ffffffful
#define REG_PRIORITY_INVALID	0xfffffffful

typedef void __fastcall RP_FUNC		(void *obj);
#define DECLARE_MESSAGE(name)		extern ENGINE_API RP_FUNC rp_##name; class ENGINE_API pure##name { public: virtual void  On##name(void)=0;	}
#define DECLARE_RP(name) void __fastcall rp_##name(void *p) { ((pure##name *)p)->On##name(); }

DECLARE_MESSAGE(Frame);
DECLARE_MESSAGE(Render);
DECLARE_MESSAGE(AppActivate);
DECLARE_MESSAGE(AppDeactivate);
DECLARE_MESSAGE(AppStart);
DECLARE_MESSAGE(AppEnd);
DECLARE_MESSAGE(DeviceReset);



//-----------------------------------------------------------------------------
struct _REG_INFO {
	void*	Object;
	int		Prio;
	u32		Flags;
};

ENGINE_API extern int	__cdecl	_REG_Compare(const void *, const void *);

ENGINE_API bool RP_SafeCall(RP_FUNC *f, void *obj, int profiling);

template <class T> class CRegistrator		// the registrator itself
{
	friend ENGINE_API int	__cdecl	_REG_Compare(const void *, const void *);
public:
	xr_vector<_REG_INFO>	R;
	// constructor
	struct {
		u32		in_process	:1;
		u32		changed		:1;
	};
	int						profiling;

	CRegistrator()			{ in_process = false; changed = false; profiling = 0; }

	//
	void Add	(T *obj, int priority=REG_PRIORITY_NORMAL, u32 flags=0)
	{
#ifdef DEBUG
		VERIFY	(priority!=REG_PRIORITY_INVALID);
		VERIFY	(obj);
		for		(u32 i=0; i<R.size(); i++) VERIFY( !((R[i].Prio!=REG_PRIORITY_INVALID)&&(R[i].Object==(void*)obj))   );
#endif
		_REG_INFO			I;
		I.Object			=obj;
		I.Prio				=priority;
		I.Flags				=flags;
		R.push_back			(I);
		
		if(in_process)		changed=true;
		else Resort			( );
	};
	void Remove	(T *obj)
	{
		for (u32 i=0; i<R.size(); i++) {
			if (R[i].Object==obj) R[i].Prio = REG_PRIORITY_INVALID;
		}
		if(in_process)		changed=true;
		else Resort			( );
	};
	void Process(RP_FUNC *f)
	{
		in_process = true;
		if (profiling) MsgCB("$#CONTEXT: CRegistrator::Process in, size = %d", R.size());

    	if (R.empty()) return;
		u32 i = 0;

		if (R[i].Prio==REG_PRIORITY_CAPTURE)
			RP_SafeCall(f, R[i].Object, profiling);
		else {
			for (i = 0; i < R.size(); i++)
				if (R[i].Prio != REG_PRIORITY_INVALID)
				{
					RP_SafeCall(f, R[i].Object, profiling);					
				}

		}		
		if(changed)	Resort();
		in_process = false;
		if (profiling) MsgCB("$#CONTEXT: CRegistrator::Process out, last = %d", i);
	};
	void Resort	(void)
	{
		if (profiling) MsgCB("$#CONTEXT: CRegistrator::Resort");
		qsort	(&*R.begin(),R.size(),sizeof(_REG_INFO),_REG_Compare);
		while	((R.size()) && (R[R.size()-1].Prio==REG_PRIORITY_INVALID)) R.pop_back();
		if (R.empty())		R.clear		();
		busy_warn(DEBUG_INFO, 3);
		changed				= false;
	};
};

#endif
