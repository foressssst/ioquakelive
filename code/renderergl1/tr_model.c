/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"

#define	LL(x) x=LittleLong(x)

static qboolean R_LoadMD3(model_t *mod, int lod, void *buffer, int fileSize, const char *name );
static qboolean R_LoadMDR(model_t *mod, void *buffer, int filesize, const char *name );

static qhandle_t R_RegisterMD3(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	int			lod;
	uint32_t	ident;
	qboolean	loaded;
	int			fileSize;
	char filename[MAX_QPATH], namebuf[MAX_QPATH+20];
	const char *fext;

	fext = COM_GetExtension(name);
	COM_StripExtension(name, filename, sizeof(filename));

	for ( lod = 0 ; lod < MD3_MAX_LODS ; lod++ ) {
		if ( lod )
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		fileSize = ri.FS_ReadFile( namebuf, &buf.v );
		if ( !buf.v )
			continue;

		if ( fileSize < sizeof( md3Header_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
			ri.FS_FreeFile( buf.v );
			break;
		}

		ident = LittleLong( *buf.u );
		if ( ident == MD3_IDENT ) {
			loaded = R_LoadMD3( mod, mod->numLods, buf.v, fileSize, name );
		} else {
			ri.Printf( PRINT_WARNING, "%s: unknown fileid for %s\n", __func__, name );
			loaded = qfalse;
		}

		ri.FS_FreeFile( buf.v );

		if ( loaded ) {
			mod->numLods++;
		} else {
			mod->md3[mod->numLods] = NULL;
			break;
		}
	}

	if ( mod->numLods )
		return mod->index;

	ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "%s: couldn't load %s\n", __func__, name );

	mod->type = MOD_BAD;
	return 0;
}

static qhandle_t R_RegisterMDR(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	uint32_t ident;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile( name, &buf.v );
	if ( !buf.v ) {
		mod->type = MOD_BAD;
		return 0;
	}

	if ( filesize < sizeof( ident ) ) {
		ri.FS_FreeFile( buf.v );
		mod->type = MOD_BAD;
		return 0;
	}

	ident = LittleLong( *buf.u );
	if ( ident == MDR_IDENT )
		loaded = R_LoadMDR( mod, buf.v, filesize, name );

	ri.FS_FreeFile( buf.v );

	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}

	return mod->index;
}

static qhandle_t R_RegisterIQM(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile(name, (void **) &buf.v);
	if(!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}

	loaded = R_LoadIQM(mod, buf.u, filesize, name);

	ri.FS_FreeFile (buf.v);

	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}

	return mod->index;
}

typedef struct
{
	const char *ext;
	qhandle_t (*ModelLoader)( const char *, model_t * );
} modelExtToLoaderMap_t;

static modelExtToLoaderMap_t modelLoaders[ ] =
{
	{ "iqm", R_RegisterIQM },
	{ "mdr", R_RegisterMDR },
	{ "md3", R_RegisterMD3 }
};

static int numModelLoaders = ARRAY_LEN(modelLoaders);

model_t	*R_GetModelByHandle( qhandle_t index ) {
	model_t		*mod;

	if ( index < 1 || index >= tr.numModels ) {
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

model_t *R_AllocModel( void ) {
	model_t		*mod;

	if ( tr.numModels >= MAX_MOD_KNOWN ) {
		return NULL;
	}

	mod = ri.Hunk_Alloc( sizeof( *tr.models[tr.numModels] ), h_low );
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

qhandle_t RE_RegisterModel( const char *name ) {
	model_t		*mod;
	qhandle_t	hModel;
	qboolean	orgNameFailed = qfalse;
	int			orgLoader = -1;
	int			i;
	char		localName[ MAX_QPATH ];
	const char	*ext;
	char		altName[ MAX_QPATH ];

	if ( !name || !name[0] ) {
		ri.Printf( PRINT_ALL, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

	for ( hModel = 1 ; hModel < tr.numModels; hModel++ ) {
		mod = tr.models[hModel];
		if ( !strcmp( mod->name, name ) ) {
			if( mod->type == MOD_BAD ) {
				return 0;
			}
			return hModel;
		}
	}

	if ( ( mod = R_AllocModel() ) == NULL ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}

	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

	mod->type = MOD_BAD;
	mod->numLods = 0;

	Q_strncpyz( localName, name, MAX_QPATH );

	ext = COM_GetExtension( localName );

	if( *ext )
	{
		for( i = 0; i < numModelLoaders; i++ )
		{
			if( !Q_stricmp( ext, modelLoaders[ i ].ext ) )
			{
				hModel = modelLoaders[ i ].ModelLoader( localName, mod );
				break;
			}
		}

		if( i < numModelLoaders )
		{
			if( !hModel )
			{
				orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				return mod->index;
			}
		}
	}

	for( i = 0; i < numModelLoaders; i++ )
	{
		if (i == orgLoader)
			continue;

		Com_sprintf( altName, sizeof (altName), "%s.%s", localName, modelLoaders[ i ].ext );

		hModel = modelLoaders[ i ].ModelLoader( altName, mod );

		if( hModel )
		{
			if( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
						name, altName );
			}

			break;
		}
	}

	return hModel;
}

static qboolean R_LoadMD3( model_t *mod, int lod, void *buffer, int fileSize, const char *mod_name ) {
	int					i, j;
	md3Header_t			*pinmodel, *hdr;
	md3Frame_t			*frame;
	md3Surface_t		*surf;
	md3Shader_t			*shader;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3XyzNormal_t		*xyz;
	md3Tag_t			*tag;
	uint32_t			version;
	uint32_t			size, bytesToEnd;

	pinmodel = (md3Header_t *)buffer;

	version = LittleLong( pinmodel->version );
	if ( version != MD3_VERSION ) {
		ri.Printf( PRINT_WARNING, "%s: %s has wrong version (%i should be %i)\n", __func__, mod_name, version, MD3_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MESH;
	mod->dataSize += size;
	mod->md3[lod] = ri.Hunk_Alloc( size, h_low );

	Com_Memcpy( mod->md3[lod], buffer, size );

	hdr = mod->md3[lod];

	LL( hdr->ident );
	LL( hdr->version );
	LL( hdr->numFrames );
	LL( hdr->numTags);
	LL( hdr->numSurfaces);
	LL( hdr->numSkins );
	LL( hdr->ofsFrames );
	LL( hdr->ofsTags );
	LL( hdr->ofsSurfaces );
	LL( hdr->ofsEnd );

	if ( hdr->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "%s: %s has no frames\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->ofsFrames > size || hdr->ofsTags > size || hdr->ofsSurfaces > size ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->numFrames > (size - hdr->ofsFrames) / sizeof( md3Frame_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numTags > (size - hdr->ofsTags) / (sizeof( md3Tag_t ) * hdr->numFrames) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->numSurfaces > (size - hdr->ofsSurfaces) / sizeof( md3Surface_t ) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	frame = (md3Frame_t *) ( (byte *)hdr + hdr->ofsFrames );
	for ( i = 0 ; i < hdr->numFrames ; i++, frame++) {
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0 ; j < 3 ; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
		}
	}

	tag = (md3Tag_t *) ( (byte *)hdr + hdr->ofsTags );
	for ( i = 0 ; i < hdr->numTags * hdr->numFrames; i++, tag++ ) {
		tag->name[sizeof( tag->name ) - 1] = '\0';
		for ( j = 0 ; j < 3; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
		}
	}

	surf = (md3Surface_t *) ( (byte *)hdr + hdr->ofsSurfaces );
	for ( i = 0 ; i < hdr->numSurfaces; i++) {

		bytesToEnd = size - ((byte*)surf - (byte*)hdr);

		if ( bytesToEnd < sizeof(*surf)) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		LL(surf->ident);
		LL(surf->flags);
		LL(surf->numFrames);
		LL(surf->numShaders);
		LL(surf->numTriangles);
		LL(surf->numVerts);
		LL(surf->ofsTriangles);
		LL(surf->ofsShaders);
		LL(surf->ofsSt);
		LL(surf->ofsXyzNormals);
		LL(surf->ofsEnd);

		if ( surf->ofsTriangles > bytesToEnd || surf->ofsShaders > bytesToEnd || surf->ofsSt > bytesToEnd ||
			 surf->ofsXyzNormals > bytesToEnd || surf->ofsEnd > bytesToEnd ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numTriangles > (bytesToEnd - surf->ofsTriangles) / sizeof( md3Triangle_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numShaders > (bytesToEnd - surf->ofsShaders) / sizeof( md3Shader_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numVerts > (bytesToEnd - surf->ofsSt) / sizeof( md3St_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->numVerts > (bytesToEnd - surf->ofsXyzNormals) / sizeof( md3XyzNormal_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i verts on %s (%i).\n", __func__,
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles >= SHADER_MAX_INDEXES / 3 ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i triangles on %s (%i).\n", __func__,
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		surf->ident = SF_MD3;

		surf->name[sizeof( surf->name ) - 1] = '\0';

		Q_strlwr( surf->name );

		j = strlen( surf->name );
		if ( j > 2 && surf->name[j-2] == '_' ) {
			surf->name[j-2] = 0;
		}

		shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
		for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
			shader_t	*sh;

			shader->name[sizeof( shader->name ) - 1] = '\0';

			sh = R_FindShader( shader->name, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				shader->shaderIndex = 0;
			} else {
				shader->shaderIndex = sh->index;
			}
		}

		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
		for ( j = 0 ; j < surf->numTriangles; j++, tri++ ) {
			LL(tri->indexes[0]);
			LL(tri->indexes[1]);
			LL(tri->indexes[2]);
			if (tri->indexes[0] >= surf->numVerts ||
				tri->indexes[1] >= surf->numVerts ||
				tri->indexes[2] >= surf->numVerts) {
				ri.Printf( PRINT_WARNING, "%s: %s has corrupted indexes\n", __func__, mod_name );
				return qfalse;
			}
		}

		st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
		for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
			st->st[0] = LittleFloat( st->st[0] );
			st->st[1] = LittleFloat( st->st[1] );
		}

		xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
		for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ ) 
		{
			xyz->xyz[0] = LittleShort( xyz->xyz[0] );
			xyz->xyz[1] = LittleShort( xyz->xyz[1] );
			xyz->xyz[2] = LittleShort( xyz->xyz[2] );

			xyz->normal = LittleShort( xyz->normal );
		}

		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}

static qboolean R_LoadMDR( model_t *mod, void *buffer, int filesize, const char *mod_name ) 
{
	int					i, j, k, l;
	mdrHeader_t			*pinmodel, *mdr;
	mdrFrame_t			*frame;
	mdrLOD_t			*lod, *curlod;
	mdrSurface_t			*surf, *cursurf;
	mdrTriangle_t			*tri, *curtri;
	mdrVertex_t			*v, *curv;
	mdrWeight_t			*weight, *curweight;
	mdrTag_t			*tag, *curtag;
	int					size;
	shader_t			*sh;

	pinmodel = (mdrHeader_t *)buffer;

	pinmodel->version = LittleLong(pinmodel->version);
	if ( pinmodel->version != MDR_VERSION ) 
	{
		ri.Printf(PRINT_WARNING, "%s: %s has wrong version (%i should be %i)\n", __func__, mod_name, pinmodel->version, MDR_VERSION);
		return qfalse;
	}

	size = LittleLong(pinmodel->ofsEnd);

	if ( size > filesize )
	{
		ri.Printf( PRINT_WARNING, "%s: Header of %s is broken. Wrong filesize declared!\n", __func__, mod_name );
		return qfalse;
	}

	mod->type = MOD_MDR;

	LL(pinmodel->numFrames);
	LL(pinmodel->numBones);
	LL(pinmodel->ofsFrames);

	if(pinmodel->ofsFrames < 0)
	{
		size += pinmodel->numFrames * sizeof(frame->name);
		size += pinmodel->numFrames * pinmodel->numBones * ((sizeof(mdrBone_t) - sizeof(mdrCompBone_t)));
	}

	if(pinmodel->numBones < 0 ||
		sizeof(*mdr) + pinmodel->numFrames * (sizeof(*frame) + (pinmodel->numBones - 1) * sizeof(*frame->bones)) > size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}

	mod->dataSize += size;
	mod->modelData = mdr = ri.Hunk_Alloc( size, h_low );

	mdr->ident = LittleLong(pinmodel->ident);
	mdr->version = pinmodel->version;
	Q_strncpyz(mdr->name, pinmodel->name, sizeof(mdr->name));
	mdr->numFrames = pinmodel->numFrames;
	mdr->numBones = pinmodel->numBones;
	mdr->numLODs = LittleLong(pinmodel->numLODs);
	mdr->numTags = LittleLong(pinmodel->numTags);

	mod->numLods = mdr->numLODs;

	if ( mdr->numFrames < 1 ) 
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has no frames\n", mod_name);
		return qfalse;
	}

	frame = (mdrFrame_t *)(mdr + 1);
	mdr->ofsFrames = (int)((byte *) frame - (byte *) mdr);

	if (pinmodel->ofsFrames < 0)
	{
		mdrCompFrame_t *cframe;

		cframe = (mdrCompFrame_t *)((byte *) pinmodel - pinmodel->ofsFrames);

		for(i = 0; i < mdr->numFrames; i++)
		{
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(cframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(cframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(cframe->localOrigin[j]);
			}

			frame->radius = LittleFloat(cframe->radius);
			frame->name[0] = '\0';

			for(j = 0; j < mdr->numBones; j++)
			{
				for(k = 0; k < (sizeof(cframe->bones[j].Comp) / 2); k++)
				{

					((unsigned short *)(cframe->bones[j].Comp))[k] =
						LittleShort( ((unsigned short *)(cframe->bones[j].Comp))[k] );
				}

				MC_UnCompress(frame->bones[j].matrix, cframe->bones[j].Comp);
			}

			cframe = (mdrCompFrame_t *) &cframe->bones[j];
			frame = (mdrFrame_t *) &frame->bones[j];
		}
	}
	else
	{
		mdrFrame_t *curframe;

		curframe = (mdrFrame_t *)((byte *) pinmodel + pinmodel->ofsFrames);

		for ( i = 0 ; i < mdr->numFrames ; i++) 
		{
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(curframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(curframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(curframe->localOrigin[j]);
			}

			frame->radius = LittleFloat(curframe->radius);
			Q_strncpyz(frame->name, curframe->name, sizeof(frame->name));

			for (j = 0; j < (int) (mdr->numBones * sizeof(mdrBone_t) / 4); j++) 
			{
				((float *)frame->bones)[j] = LittleFloat( ((float *)curframe->bones)[j] );
			}

			curframe = (mdrFrame_t *) &curframe->bones[mdr->numBones];
			frame = (mdrFrame_t *) &frame->bones[mdr->numBones];
		}
	}

	lod = (mdrLOD_t *) frame;
	mdr->ofsLODs = (int) ((byte *) lod - (byte *)mdr);

	curlod = (mdrLOD_t *)((byte *) pinmodel + LittleLong(pinmodel->ofsLODs));

	for ( l = 0 ; l < mdr->numLODs ; l++)
	{
		if((byte *) (lod + 1) > (byte *) mdr + size)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
			return qfalse;
		}

		lod->numSurfaces = LittleLong(curlod->numSurfaces);

		surf = (mdrSurface_t *) (lod + 1);
		lod->ofsSurfaces = (int)((byte *) surf - (byte *) lod);
		cursurf = (mdrSurface_t *) ((byte *)curlod + LittleLong(curlod->ofsSurfaces));

		for ( i = 0 ; i < lod->numSurfaces ; i++)
		{
			if((byte *) (surf + 1) > (byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			surf->ident = SF_MDR;
			Q_strncpyz(surf->name, cursurf->name, sizeof(surf->name));
			Q_strncpyz(surf->shader, cursurf->shader, sizeof(surf->shader));

			surf->ofsHeader = (byte *) mdr - (byte *) surf;

			surf->numVerts = LittleLong(cursurf->numVerts);
			surf->numTriangles = LittleLong(cursurf->numTriangles);

			if ( surf->numVerts >= SHADER_MAX_VERTEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i verts on %s (%i).\n",
					  mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
					  surf->numVerts );
				return qfalse;
			}
			if ( surf->numTriangles*3 >= SHADER_MAX_INDEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i triangles on %s (%i).\n",
					  mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
					  surf->numTriangles );
				return qfalse;
			}
			Q_strlwr( surf->name );

			sh = R_FindShader(surf->shader, LIGHTMAP_NONE, qtrue);
			if ( sh->defaultShader ) {
				surf->shaderIndex = 0;
			} else {
				surf->shaderIndex = sh->index;
			}

			v = (mdrVertex_t *) (surf + 1);
			surf->ofsVerts = (int)((byte *) v - (byte *) surf);
			curv = (mdrVertex_t *) ((byte *)cursurf + LittleLong(cursurf->ofsVerts));

			for(j = 0; j < surf->numVerts; j++)
			{
				LL(curv->numWeights);

				if(curv->numWeights < 0 || (byte *) (v + 1) + (curv->numWeights - 1) * sizeof(*weight) > (byte *) mdr + size)
				{
					ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
					return qfalse;
				}

				v->normal[0] = LittleFloat(curv->normal[0]);
				v->normal[1] = LittleFloat(curv->normal[1]);
				v->normal[2] = LittleFloat(curv->normal[2]);

				v->texCoords[0] = LittleFloat(curv->texCoords[0]);
				v->texCoords[1] = LittleFloat(curv->texCoords[1]);

				v->numWeights = curv->numWeights;
				weight = &v->weights[0];
				curweight = &curv->weights[0];

				for(k = 0; k < v->numWeights; k++)
				{
					weight->boneIndex = LittleLong(curweight->boneIndex);
					weight->boneWeight = LittleFloat(curweight->boneWeight);

					weight->offset[0] = LittleFloat(curweight->offset[0]);
					weight->offset[1] = LittleFloat(curweight->offset[1]);
					weight->offset[2] = LittleFloat(curweight->offset[2]);

					weight++;
					curweight++;
				}

				v = (mdrVertex_t *) weight;
				curv = (mdrVertex_t *) curweight;
			}

			tri = (mdrTriangle_t *) v;
			surf->ofsTriangles = (int)((byte *) tri - (byte *) surf);
			curtri = (mdrTriangle_t *)((byte *) cursurf + LittleLong(cursurf->ofsTriangles));

			if(surf->numTriangles < 0 || (byte *) (tri + surf->numTriangles) > (byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			for(j = 0; j < surf->numTriangles; j++)
			{
				tri->indexes[0] = LittleLong(curtri->indexes[0]);
				tri->indexes[1] = LittleLong(curtri->indexes[1]);
				tri->indexes[2] = LittleLong(curtri->indexes[2]);

				tri++;
				curtri++;
			}

			surf->ofsEnd = (byte *) tri - (byte *) surf;
			surf = (mdrSurface_t *) tri;

			cursurf = (mdrSurface_t *) ((byte *) cursurf + LittleLong(cursurf->ofsEnd));
		}

		lod->ofsEnd = (int)((byte *) surf - (byte *) lod);
		lod = (mdrLOD_t *) surf;

		curlod = (mdrLOD_t *)((byte *) curlod + LittleLong(curlod->ofsEnd));
	}

	tag = (mdrTag_t *) lod;
	mdr->ofsTags = (int)((byte *) tag - (byte *) mdr);
	curtag = (mdrTag_t *) ((byte *)pinmodel + LittleLong(pinmodel->ofsTags));

	if(mdr->numTags < 0 || (byte *) (tag + mdr->numTags) > (byte *) mdr + size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}

	for (i = 0 ; i < mdr->numTags ; i++)
	{
		tag->boneIndex = LittleLong(curtag->boneIndex);
		Q_strncpyz(tag->name, curtag->name, sizeof(tag->name));

		tag++;
		curtag++;
	}

	mdr->ofsEnd = (int)((byte *) tag - (byte *) mdr);

	return qtrue;
}

void RE_BeginRegistration( glconfig_t *glconfigOut ) {

	R_Init();

	R_ImageLoadStatsReset();
	R_PrefetchStart( "common" );

	*glconfigOut = glConfig;

	tr.viewCluster = -1;
	R_ClearFlares();
	RE_ClearScene();

	tr.registered = qtrue;
}

void R_ModelInit( void ) {
	model_t		*mod;

	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = MOD_BAD;
}

void R_Modellist_f( void ) {
	int		i;
	model_t	*mod;
	int		total;

	total = 0;
	for ( i = 1 ; i < tr.numModels; i++ ) {
		mod = tr.models[i];
		ri.Printf( PRINT_ALL, "%8i : (%i) %s\n",mod->dataSize, mod->numLods, mod->name );
		total += mod->dataSize;
	}
	ri.Printf( PRINT_ALL, "%8i : Total models\n", total );

#if	0
	if ( tr.world ) {
		ri.Printf( PRINT_ALL, "\n%8i : %s\n", tr.world->dataSize, tr.world->name );
	}
#endif
}

static md3Tag_t *R_GetTag( md3Header_t *mod, int frame, const char *tagName ) {
	md3Tag_t		*tag;
	int				i;

	if ( frame >= mod->numFrames ) {
		frame = mod->numFrames - 1;
	}

	tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for ( i = 0 ; i < mod->numTags ; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;
		}
	}

	return NULL;
}

static md3Tag_t *R_GetAnimTag( mdrHeader_t *mod, int framenum, const char *tagName, md3Tag_t * dest) 
{
	int				i, j, k;
	int				frameSize;
	mdrFrame_t		*frame;
	mdrTag_t		*tag;

	if ( framenum >= mod->numFrames ) 
	{
		framenum = mod->numFrames - 1;
	}

	tag = (mdrTag_t *)((byte *)mod + mod->ofsTags);
	for ( i = 0 ; i < mod->numTags ; i++, tag++ )
	{
		if ( !strcmp( tag->name, tagName ) )
		{
			Q_strncpyz(dest->name, tag->name, sizeof(dest->name));

			frameSize = (intptr_t)( &((mdrFrame_t *)0)->bones[ mod->numBones ] );
			frame = (mdrFrame_t *)((byte *)mod + mod->ofsFrames + framenum * frameSize );

			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 3; k++)
					dest->axis[j][k]=frame->bones[tag->boneIndex].matrix[k][j];
			}

			dest->origin[0]=frame->bones[tag->boneIndex].matrix[0][3];
			dest->origin[1]=frame->bones[tag->boneIndex].matrix[1][3];
			dest->origin[2]=frame->bones[tag->boneIndex].matrix[2][3];				

			return dest;
		}
	}

	return NULL;
}

int R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, 
					 float frac, const char *tagName ) {
	md3Tag_t	*start, *end;
	md3Tag_t	start_space, end_space;
	int		i;
	float		frontLerp, backLerp;
	model_t		*model;

	model = R_GetModelByHandle( handle );
	if ( !model->md3[0] )
	{
		if(model->type == MOD_MDR)
		{
			start = R_GetAnimTag((mdrHeader_t *) model->modelData, startFrame, tagName, &start_space);
			end = R_GetAnimTag((mdrHeader_t *) model->modelData, endFrame, tagName, &end_space);
		}
		else if( model->type == MOD_IQM ) {
			return R_IQMLerpTag( tag, model->modelData,
					startFrame, endFrame,
					frac, tagName );
		} else {
			start = end = NULL;
		}
	}
	else
	{
		start = R_GetTag( model->md3[0], startFrame, tagName );
		end = R_GetTag( model->md3[0], endFrame, tagName );
	}

	if ( !start || !end ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return qfalse;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for ( i = 0 ; i < 3 ; i++ ) {
		tag->origin[i] = start->origin[i] * backLerp +  end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp +  end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp +  end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp +  end->axis[2][i] * frontLerp;
	}
	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );
	return qtrue;
}

void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs ) {
	model_t		*model;

	model = R_GetModelByHandle( handle );

	if(model->type == MOD_BRUSH) {
		VectorCopy( model->bmodel->bounds[0], mins );
		VectorCopy( model->bmodel->bounds[1], maxs );

		return;
	} else if (model->type == MOD_MESH) {
		md3Header_t	*header;
		md3Frame_t	*frame;

		header = model->md3[0];
		frame = (md3Frame_t *) ((byte *)header + header->ofsFrames);

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );

		return;
	} else if (model->type == MOD_MDR) {
		mdrHeader_t	*header;
		mdrFrame_t	*frame;

		header = (mdrHeader_t *)model->modelData;
		frame = (mdrFrame_t *) ((byte *)header + header->ofsFrames);

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );

		return;
	} else if(model->type == MOD_IQM) {
		iqmData_t *iqmData;

		iqmData = model->modelData;

		if(iqmData->bounds)
		{
			VectorCopy(iqmData->bounds, mins);
			VectorCopy(iqmData->bounds + 3, maxs);
			return;
		}
	}

	VectorClear( mins );
	VectorClear( maxs );
}
