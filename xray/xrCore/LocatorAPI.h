// LocatorAPI.h: interface for the CLocatorAPI class.
//
//////////////////////////////////////////////////////////////////////

#ifndef LocatorAPIH
#define LocatorAPIH
#pragma once

#pragma warning(push)
#pragma warning(disable:4995)
#include <io.h>
#pragma warning(pop)

#include "LocatorAPI_defs.h"

typedef struct _FILE_ACCESS_STAT
{
	__time64_t			first_open;
	__time64_t			first_read;
	__time64_t			full_read;
	u64					total_read;
	u32					open_count;
	u32					extra[5];
} FILE_ACCESS_STAT, *PFILE_ACCESS_STAT;


class XRCORE_API CStreamReader;

class XRCORE_API CLocatorAPI  
{
	friend class FS_Path;
public:
	struct	file
	{
		LPCSTR					name;			// low-case name
		u32						vfs;			// 0xffffffff - standart file
		file_ptr				ptr;			// pointer inside vfs
		u32						crc;			// contents CRC		
		file_size				size_real;		// 64-bit size only for big archives
		u32						size_compressed;// if (size_real==size_compressed) - uncompressed
		u32						flags;			// file flags and position desc 
		u32						chunk;			// chunk in archive for new format
        __time64_t 				modif;			// for editor
	};
private:
	struct	file_pred: public 	std::binary_function<file&, file&, bool> 
	{	
		IC bool operator()	(const file& x, const file& y) const
		{	return xr_strcmp(x.name,y.name)<0;	}
	};
	struct	archive
	{
		shared_str				path;
		void					*hSrcFile, *hSrcMap;
		u64						src_size;
	};
	DEFINE_MAP_PRED				(LPCSTR,FS_Path*,PathMap,PathPairIt,pred_str);
	PathMap						pathes;

	DEFINE_SET_PRED				(file,files_set,files_it,file_pred);
    DEFINE_VECTOR				(archive,archives_vec,archives_it);

	DEFINE_VECTOR				(_finddata_t,FFVec,FFIt);
	FFVec						rec_files;

    int							m_iLockRescan	; 
    void						rescan_path		(LPCSTR full_path, BOOL bRecurse);
    void						check_pathes	();

	files_set					files			;
    archives_vec				archives		;
	BOOL						bNoRecurse		;

	xrCriticalSection			m_auth_lock		;
	u64							m_auth_code		;

	void						Register		(LPCSTR name, u32 vfs, u32 crc, file_ptr ptr, file_size size_real, u32 size_compressed, __time64_t modif);
	void						Register		(file  &desc);  // alpet: для прямой инициализации  
	void						ProcessArchive	(LPCSTR path, LPCSTR base_path=NULL);
	void						ProcessOne		(LPCSTR path, void* F);
	bool						Recurse			(LPCSTR path);	
//	bool						CheckExistance	(LPCSTR path);

	files_it					file_find_it	(LPCSTR n);

	typedef		xr_map<shared_str, FILE_ACCESS_STAT> FILE_STATS_MAP;
	FILE_STATS_MAP				m_access_stat;
	typedef		xr_vector<IReader*> FILE_CHUNK_LIST;  // for super-cached files
	typedef		xr_map<shared_str,  FILE_CHUNK_LIST> FILE_CHUNK_MAP;
	FILE_CHUNK_MAP				m_chunk_map;
	
public:
	enum{
		flNeedRescan			= (1<<0),
		flBuildCopy				= (1<<1),
		flReady					= (1<<2),
		flEBuildCopy			= (1<<3),
		flEventNotificator      = (1<<4),
		flTargetFolderOnly		= (1<<5),
		flCacheFiles			= (1<<6),
		flScanAppRoot			= (1<<7),
		flNeedCheck				= (1<<8),
		flDumpFileActivity		= (1<<9),
	};    
	Flags32						m_Flags			;
	u32							dwAllocGranularity;
	u32							dwOpenCounter;

	FILE_ACCESS_STAT*			gather_file_stat	(LPCSTR name, char op, file_ptr ptr, u32 cb); // alpet: для сбора статистики доступа к файлам
	void						gather_file_stat	(FILE_ACCESS_STAT *s, const file *desc, char op, file_ptr ptr, u32 cb);
	void						gather_file_stat	(IReader	   *R, char op, file_ptr ptr, u32 cb);
	void						gather_file_stat	(CStreamReader *R, char op, file_ptr ptr, u32 cb);
private:
			void				check_cached_files	(LPSTR fname, const file &desc, LPCSTR &source_name);

			void				file_from_cache_impl(IReader *&R, LPSTR fname, const file &desc);
			void				file_from_cache_impl(CStreamReader *&R, LPSTR fname, const file &desc);
	template <typename T>
			void				file_from_cache		(T *&R, LPSTR fname, const file &desc, LPCSTR &source_name);
			
			void				file_from_archive	(IReader *&R, LPCSTR fname, const file &desc);
			void				file_from_archive	(CStreamReader *&R, LPCSTR fname, const file &desc);
			IReader*			get_file_chunk		(LPCSTR fname, const file &desc);

			void				copy_file_to_build	(IWriter *W, IReader *r);
			void				copy_file_to_build	(IWriter *W, CStreamReader *r);
	template <typename T>
			void				copy_file_to_build	(T *&R, LPCSTR source_name);

			bool				check_for_file		(LPCSTR path, LPCSTR _fname, string_path& fname, const file *&desc);
	
	template <typename T>
	IC		T					*r_open_impl		(LPCSTR path, LPCSTR _fname);
			void				ProcessExternalArch	();			
public:
								CLocatorAPI		();
								~CLocatorAPI	();
	void						_initialize		(u32 flags, LPCSTR target_folder=0, LPCSTR fs_name=0);
	void						_destroy		();

	CStreamReader*				rs_open			(LPCSTR initial, LPCSTR N);
	IReader*					r_open			(LPCSTR initial, LPCSTR N);
	IC IReader*					r_open			(LPCSTR N){return r_open(0,N);}
	void						r_close			(IReader* &S);
	void						r_close			(CStreamReader* &fs);

	IWriter*					w_open			(LPCSTR initial, LPCSTR N);
	IC IWriter*					w_open			(LPCSTR N){return w_open(0,N);}
	IWriter*					w_open_ex		(LPCSTR initial, LPCSTR N);
	IC IWriter*					w_open_ex		(LPCSTR N){return w_open_ex(0,N);}
	void						w_close			(IWriter* &S);

	const file*					exist			(LPCSTR N);
	const file*					exist			(LPCSTR path, LPCSTR name);
	const file*					exist			(string_path& fn, LPCSTR path, LPCSTR name);
	const file*					exist			(string_path& fn, LPCSTR path, LPCSTR name, LPCSTR ext);

    BOOL 						can_write_to_folder	(LPCSTR path); 
    BOOL 						can_write_to_alias	(LPCSTR path); 
    BOOL						can_modify_file	(LPCSTR fname);
    BOOL						can_modify_file	(LPCSTR path, LPCSTR name);

    BOOL 						dir_delete			(LPCSTR path,LPCSTR nm,BOOL remove_files);
    BOOL 						dir_delete			(LPCSTR full_path,BOOL remove_files){return dir_delete(0,full_path,remove_files);}
    void 						file_delete			(LPCSTR path,LPCSTR nm);
    void 						file_delete			(LPCSTR full_path){file_delete(0,full_path);}
	void 						file_copy			(LPCSTR src, LPCSTR dest);
	void 						file_rename			(LPCSTR src, LPCSTR dest,bool bOwerwrite=true);
    file_size					file_length			(LPCSTR src);

    __time64_t   				get_file_age		(LPCSTR nm);
    void 						set_file_age		(LPCSTR nm, __time64_t age);

	xr_vector<LPSTR>*			file_list_open		(LPCSTR initial, LPCSTR folder,	u32 flags=FS_ListFiles);
	xr_vector<LPSTR>*			file_list_open		(LPCSTR path,					u32 flags=FS_ListFiles);
	void						file_list_close		(xr_vector<LPSTR>* &lst);
                                                     
    bool						path_exist			(LPCSTR path);
    FS_Path*					get_path			(LPCSTR path);
    FS_Path*					append_path			(LPCSTR path_alias, LPCSTR root, LPCSTR add, BOOL recursive);
    LPCSTR						update_path			(string_path& dest, LPCSTR initial, LPCSTR src);

	int							file_list			(FS_FileSet& dest, LPCSTR path, u32 flags=FS_ListFiles, LPCSTR mask=0);
//.    void						update_path			(xr_string& dest, LPCSTR initial, LPCSTR src);

	// 
	void						register_archieve	(LPCSTR path);
	void						auth_generate		(xr_vector<xr_string>&	ignore, xr_vector<xr_string>&	important);
	u64							auth_get			();
	void						auth_runtime		(void*);

	// editor functions
	void						rescan_pathes		();
	void						lock_rescan			();
	void						unlock_rescan		();
};

extern XRCORE_API	CLocatorAPI*					xr_FS;
#define FS (*xr_FS)

class CArchiveChunkDesc:
	public CResourceDesc
{
public:
	CArchiveChunkDesc			(const void *owner);
	int				vfs;            // archive ref
	file_ptr		pos;
};


class CFileResourceDesc:
	public CResourceDesc
{
public:
	CFileResourceDesc			(const void *owner);
	virtual				~CFileResourceDesc();
	CLocatorAPI::file*   f_desc;
	FILE_ACCESS_STAT*	 f_stat;
	u32					 self_desc; // флажки приведения типов 
};



#endif // LocatorAPIH

