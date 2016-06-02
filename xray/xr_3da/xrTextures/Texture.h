// Texture.h  - разное для работы с текстурами на уровне рендера
// created 01.02.16 by alpet
#pragma once
// #include "xrTextures/xrTextures.h"
#pragma warning(disable:4995)
#include <d3dx9.h>
#pragma warning(default:4995)

#define MAX_TEXTURES_COMBINE 8

class CTextureContext;

typedef struct _TEXTURE_COMBINE_CONTEXT
{
	shared_str					operation;
	CTextureContext			   *source     [MAX_TEXTURES_COMBINE];
	CTextureContext			   *results    [MAX_TEXTURES_COMBINE];
	Irect						i_coords   [MAX_TEXTURES_COMBINE];
	Frect						f_coords   [MAX_TEXTURES_COMBINE];
	unsigned long				mem_usage  [MAX_TEXTURES_COMBINE];	
	__int64						i_params	[MAX_TEXTURES_COMBINE];
	float 						f_params	[MAX_TEXTURES_COMBINE];
		
} TEXTURE_COMBINE_CONTEXT, *PTEXTURE_COMBINE_CONTEXT;