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
#include "q_shared.h"
#include "qcommon.h"
#include "q_steam.h"
#include "unzip.h"

#define MAX_ZPATH 256
#define MAX_SEARCH_PATHS 4096
#define MAX_FILEHASH_SIZE 1024

typedef struct fileInPack_s {
	char* name;
	unsigned long pos;
	unsigned long len;
	unsigned long offset;
	unsigned long csize;
	struct fileInPack_s* next;
} fileInPack_t;

typedef struct {
	char pakPathname[MAX_OSPATH];
	char pakFilename[MAX_OSPATH];
	char pakBasename[MAX_OSPATH];
	char pakGamename[MAX_OSPATH];
	unzFile handle;
	int checksum;
	int pure_checksum;
	int numfiles;
	int referenced;
	unsigned long long workshopId;
	int hashSize;
	fileInPack_t** hashTable;
	fileInPack_t* buildBuffer;
} pack_t;

typedef struct {
	char path[MAX_OSPATH];
	char fullpath[MAX_OSPATH];
	char gamedir[MAX_OSPATH];
} directory_t;

typedef struct searchpath_s {
	struct searchpath_s* next;

	pack_t* pack;
	directory_t* dir;
} searchpath_t;

static char fs_gamedir[MAX_OSPATH];
static cvar_t* fs_debug;
static cvar_t* fs_homepath;

#ifdef __APPLE__
static cvar_t* fs_apppath;
#endif
static cvar_t* fs_steampath;

static cvar_t* fs_basepath;
static cvar_t* fs_basegame;
static cvar_t* fs_gamedirvar;
static searchpath_t* fs_searchpaths;
static int fs_readCount;
static int fs_loadCount;
static int fs_loadStack;
static int fs_packFiles = 0;

static int fs_checksumFeed;

#define IOBIN_PAKNAME  "iobin"
#define IOBIN_FILENAME "iobin.pk3"

static qboolean FS_IsGamecodePak(const char* pakBasename) {
	return Q_stricmp(pakBasename, IOBIN_PAKNAME) == 0;
}

#define ENGINE_CONTENT_PAKNAME "pak01"

static qboolean FS_IsEnginePak(const pack_t* pack) {
	return pack && (FS_IsGamecodePak(pack->pakBasename) || !Q_stricmp(pack->pakBasename, ENGINE_CONTENT_PAKNAME));
}

typedef union qfile_gus {
	FILE* o;
	unzFile z;
} qfile_gut;

typedef struct qfile_us {
	qfile_gut file;
	qboolean unique;
} qfile_ut;

typedef struct {
	qfile_ut handleFiles;
	qboolean handleSync;
	int fileSize;
	int zipFilePos;
	int zipFileLen;
	qboolean zipFile;
	char name[MAX_ZPATH];
	qboolean memoryMapped;
	long mapPos;
	void* mapData;
	int mapSize;
} fileHandleData_t;

static fileHandleData_t fsh[MAX_FILE_HANDLES];

static qboolean fs_reordered;

static int fs_numServerPaks = 0;
static int fs_serverPaks[MAX_SEARCH_PATHS];
static char* fs_serverPakNames[MAX_SEARCH_PATHS];

static int fs_numServerReferencedPaks;
static int fs_serverReferencedPaks[MAX_SEARCH_PATHS];
static char* fs_serverReferencedPakNames[MAX_SEARCH_PATHS];

char lastValidBase[MAX_OSPATH];
char lastValidComBaseGame[MAX_OSPATH];
char lastValidFsBaseGame[MAX_OSPATH];
char lastValidGame[MAX_OSPATH];

static int fs_uiBinChecksum;
static int fs_cgameBinChecksum;
static int fs_qagameBinChecksum;

#ifdef FS_MISSING
FILE* missingFiles = NULL;
#endif

#if __STDC_VERSION__ < 199901L
#if __GNUC__ >= 2 || _MSC_VER >= 1300
#define __func__ __FUNCTION__
#else
#define __func__ "(unknown)"
#endif
#endif

qboolean FS_Initialized(void) {
	return (fs_searchpaths != NULL);
}

qboolean FS_PakIsPure(pack_t* pack) {
	int i;

	if (FS_IsEnginePak(pack)) {
		return qtrue;
	}
	if (fs_numServerPaks) {
		for (i = 0; i < fs_numServerPaks; i++) {
			if (pack->checksum == fs_serverPaks[i]) {
				return qtrue;
			}
		}
		return qfalse;
	}
	return qtrue;
}

int FS_LoadStack(void) {
	return fs_loadStack;
}

static long FS_HashFileName(const char* fname, int hashSize) {
	int i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter == '.')
			break;
		if (letter == '\\')
			letter = '/';
		if (letter == PATH_SEP)
			letter = '/';
		hash += (long)(letter) * (i + 119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (hashSize - 1);
	return hash;
}

static fileHandle_t FS_HandleForFile(void) {
	int i;

	for (i = 1; i < MAX_FILE_HANDLES; i++) {
		if (fsh[i].handleFiles.file.o == NULL) {
			return i;
		}
	}
	Com_Error(ERR_DROP, "FS_HandleForFile: none free");
	return 0;
}

static FILE* FS_FileForHandle(fileHandle_t f) {
	if (f < 1 || f >= MAX_FILE_HANDLES) {
		Com_Error(ERR_DROP, "FS_FileForHandle: out of range");
	}
	if (fsh[f].zipFile == qtrue) {
		Com_Error(ERR_DROP, "FS_FileForHandle: can't get FILE on zip file");
	}
	if (!fsh[f].handleFiles.file.o) {
		Com_Error(ERR_DROP, "FS_FileForHandle: NULL");
	}

	return fsh[f].handleFiles.file.o;
}

void FS_ForceFlush(fileHandle_t f) {
	FILE* file;

	file = FS_FileForHandle(f);
	setvbuf(file, NULL, _IONBF, 0);
}

long FS_fplength(FILE* h) {
	long pos;
	long end;

	pos = ftell(h);
	fseek(h, 0, SEEK_END);
	end = ftell(h);
	fseek(h, pos, SEEK_SET);

	return end;
}

long FS_filelength(fileHandle_t f) {
	FILE* h;

	h = FS_FileForHandle(f);

	if (h == NULL)
		return -1;
	else
		return FS_fplength(h);
}

static void FS_ReplaceSeparators(char* path) {
	char* s;
	qboolean lastCharWasSep = qfalse;

	for (s = path; *s; s++) {
		if (*s == '/' || *s == '\\') {
			if (!lastCharWasSep) {
				*s = PATH_SEP;
				lastCharWasSep = qtrue;
			} else {
				memmove(s, s + 1, strlen(s));
			}
		} else {
			lastCharWasSep = qfalse;
		}
	}
}

char* FS_BuildOSPath(const char* base, const char* game, const char* qpath) {
	char temp[MAX_OSPATH];
	static char ospath[2][MAX_OSPATH];
	static int toggle;

	toggle ^= 1;

	if (!game || !game[0]) {
		game = fs_gamedir;
	}

	Com_sprintf(temp, sizeof(temp), "/%s/%s", game, qpath);
	FS_ReplaceSeparators(temp);
	Com_sprintf(ospath[toggle], sizeof(ospath[0]), "%s%s", base, temp);

	return ospath[toggle];
}

qboolean FS_CreatePath(char* OSPath) {
	char* ofs;
	char path[MAX_OSPATH];

	if (strstr(OSPath, "..") || strstr(OSPath, "::")) {
		Com_Printf("WARNING: refusing to create relative path \"%s\"\n", OSPath);
		return qtrue;
	}

	Q_strncpyz(path, OSPath, sizeof(path));
	FS_ReplaceSeparators(path);

	ofs = strchr(path, PATH_SEP);
	if (ofs != NULL) {
		ofs++;
	}

	for (; ofs != NULL && *ofs; ofs++) {
		if (*ofs == PATH_SEP) {
			*ofs = 0;
			if (!Sys_Mkdir(path)) {
				Com_Error(ERR_FATAL, "FS_CreatePath: failed to create path \"%s\"",
						  path);
			}
			*ofs = PATH_SEP;
		}
	}

	return qfalse;
}

static void FS_CheckFilenameIsMutable(const char* filename,
									  const char* function) {
	if (Sys_DllExtension(filename) || COM_CompareExtension(filename, ".pk3")) {
		Com_Error(ERR_FATAL,
				  "%s: Not allowed to manipulate '%s' due "
				  "to %s extension",
				  function, filename, COM_GetExtension(filename));
	}
}

void FS_Remove(const char* osPath) {
	FS_CheckFilenameIsMutable(osPath, __func__);

	remove(osPath);
}

void FS_HomeRemove(const char* homePath) {
	FS_CheckFilenameIsMutable(homePath, __func__);

	remove(FS_BuildOSPath(fs_homepath->string,
						  fs_gamedir, homePath));
}

qboolean FS_FileInPathExists(const char* testpath) {
	FILE* filep;

	filep = Sys_FOpen(testpath, "rb");

	if (filep) {
		fclose(filep);
		return qtrue;
	}

	return qfalse;
}

qboolean FS_FileExists(const char* file) {
	return FS_FileInPathExists(FS_BuildOSPath(fs_homepath->string, fs_gamedir, file));
}

qboolean FS_SV_FileExists(const char* file) {
	char* testpath;

	testpath = FS_BuildOSPath(fs_homepath->string, file, "");
	testpath[strlen(testpath) - 1] = '\0';

	return FS_FileInPathExists(testpath);
}

fileHandle_t FS_SV_FOpenFileWrite(const char* filename) {
	char* ospath;
	fileHandle_t f;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	ospath = FS_BuildOSPath(fs_homepath->string, filename, "");
	ospath[strlen(ospath) - 1] = '\0';

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	if (fs_debug->integer) {
		Com_Printf("FS_SV_FOpenFileWrite: %s\n", ospath);
	}

	FS_CheckFilenameIsMutable(ospath, __func__);

	if (FS_CreatePath(ospath)) {
		return 0;
	}

	Com_DPrintf("writing to: %s\n", ospath);
	fsh[f].handleFiles.file.o = Sys_FOpen(ospath, "wb");

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

long FS_SV_FOpenFileRead(const char* filename, fileHandle_t* fp) {
	char* ospath;
	fileHandle_t f = 0;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	S_ClearSoundBuffer();

	ospath = FS_BuildOSPath(fs_homepath->string, filename, "");
	ospath[strlen(ospath) - 1] = '\0';

	if (fs_debug->integer) {
		Com_Printf("FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath);
	}

	fsh[f].handleFiles.file.o = Sys_FOpen(ospath, "rb");
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		if (Q_stricmp(fs_homepath->string, fs_basepath->string)) {
			ospath = FS_BuildOSPath(fs_basepath->string, filename, "");
			ospath[strlen(ospath) - 1] = '\0';

			if (fs_debug->integer) {
				Com_Printf("FS_SV_FOpenFileRead (fs_basepath): %s\n", ospath);
			}

			fsh[f].handleFiles.file.o = Sys_FOpen(ospath, "rb");
			fsh[f].handleSync = qfalse;
		}

		if (!fsh[f].handleFiles.file.o && fs_steampath->string[0]) {
			ospath = FS_BuildOSPath(fs_steampath->string, filename, "");
			ospath[strlen(ospath) - 1] = '\0';

			if (fs_debug->integer) {
				Com_Printf("FS_SV_FOpenFileRead (fs_steampath): %s\n", ospath);
			}

			fsh[f].handleFiles.file.o = Sys_FOpen(ospath, "rb");
			fsh[f].handleSync = qfalse;
		}

		if (!fsh[f].handleFiles.file.o) {
			f = 0;
		}
	}

	*fp = f;
	if (f) {
		return FS_filelength(f);
	}

	return -1;
}

void FS_SV_Rename(const char* from, const char* to, qboolean safe) {
	char* from_ospath, * to_ospath;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	S_ClearSoundBuffer();

	from_ospath = FS_BuildOSPath(fs_homepath->string, from, "");
	to_ospath = FS_BuildOSPath(fs_homepath->string, to, "");
	from_ospath[strlen(from_ospath) - 1] = '\0';
	to_ospath[strlen(to_ospath) - 1] = '\0';

	if (fs_debug->integer) {
		Com_Printf("FS_SV_Rename: %s --> %s\n", from_ospath, to_ospath);
	}

	if (safe) {
		FS_CheckFilenameIsMutable(to_ospath, __func__);
	}

	rename(from_ospath, to_ospath);
}

const char* FS_FindSystemFile(const char* file) {
	FILE* f;
	char* testpath;

	testpath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, file);
	f = Sys_FOpen(testpath, "rb");
	if (f) {
		fclose(f);
		return testpath;
	}

	testpath = FS_BuildOSPath(fs_basepath->string, fs_gamedir, file);
	f = Sys_FOpen(testpath, "rb");
	if (f) {
		fclose(f);
		return testpath;
	}

	return file;
}

void FS_Rename(const char* from, const char* to) {
	char* from_ospath, * to_ospath;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	S_ClearSoundBuffer();

	from_ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, from);
	to_ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, to);

	if (fs_debug->integer) {
		Com_Printf("FS_Rename: %s --> %s\n", from_ospath, to_ospath);
	}

	FS_CheckFilenameIsMutable(to_ospath, __func__);

	rename(from_ospath, to_ospath);
}

void FS_FCloseFile(fileHandle_t f) {
	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (fsh[f].zipFile == qtrue) {
		unzCloseCurrentFile(fsh[f].handleFiles.file.z);
		if (fsh[f].handleFiles.unique) {
			unzClose(fsh[f].handleFiles.file.z);
		}
		Com_Memset(&fsh[f], 0, sizeof(fsh[f]));
		return;
	}

	if (fsh[f].handleFiles.file.o) {
		fclose(fsh[f].handleFiles.file.o);
	}
	Com_Memset(&fsh[f], 0, sizeof(fsh[f]));
}

fileHandle_t FS_FOpenFileWrite(const char* filename) {
	char* ospath;
	fileHandle_t f;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);

	if (fs_debug->integer) {
		Com_Printf("FS_FOpenFileWrite: %s\n", ospath);
	}

	if (FS_CreatePath(ospath)) {
		return 0;
	}

	fsh[f].handleFiles.file.o = Sys_FOpen(ospath, "wb");

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}

	return f;
}

static fileHandle_t FS_OpenAppend(const char* filename, qboolean quiet) {
	char* ospath;
	fileHandle_t f;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	if (!quiet) {
		S_ClearSoundBuffer();
	}

	ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);

	if (fs_debug->integer) {
		Com_Printf("FS_FOpenFileAppend: %s\n", ospath);
	}

	FS_CheckFilenameIsMutable(ospath, __func__);

	if (FS_CreatePath(ospath)) {
		return 0;
	}

	fsh[f].handleFiles.file.o = Sys_FOpen(ospath, "ab");
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

fileHandle_t FS_FOpenFileAppend(const char* filename) {
	return FS_OpenAppend(filename, qfalse);
}

fileHandle_t FS_FOpenFileAppendQuiet(const char* filename) {
	return FS_OpenAppend(filename, qtrue);
}

fileHandle_t FS_FCreateOpenPipeFile(const char* filename) {
	char* ospath;
	FILE* fifo;
	fileHandle_t f;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));

	S_ClearSoundBuffer();

	ospath = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);

	if (fs_debug->integer) {
		Com_Printf("FS_FCreateOpenPipeFile: %s\n", ospath);
	}

	FS_CheckFilenameIsMutable(ospath, __func__);

	fifo = Sys_Mkfifo(ospath);
	if (fifo) {
		fsh[f].handleFiles.file.o = fifo;
		fsh[f].handleSync = qfalse;
	} else {
		Com_Printf(S_COLOR_YELLOW
				   "WARNING: Could not create new com_pipefile at %s. "
				   "com_pipefile will not be used.\n",
				   ospath);
		f = 0;
	}

	return f;
}

qboolean FS_FilenameCompare(const char* s1, const char* s2) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if (c1 == '\\' || c1 == ':') {
			c1 = '/';
		}
		if (c2 == '\\' || c2 == ':') {
			c2 = '/';
		}

		if (c1 != c2) {
			return qtrue;
		}
	} while (c1);

	return qfalse;
}

qboolean FS_IsExt(const char* filename, const char* ext, int namelen) {
	int extlen;

	extlen = strlen(ext);

	if (extlen > namelen)
		return qfalse;

	filename += namelen - extlen;

	return !Q_stricmp(filename, ext);
}

qboolean FS_IsDemoExt(const char* filename, int namelen) {
	char* ext_test;
	int index, protocol;

	ext_test = strrchr(filename, '.');
	if (ext_test && !Q_stricmpn(ext_test + 1, DEMOEXT, ARRAY_LEN(DEMOEXT) - 1)) {
		protocol = atoi(ext_test + ARRAY_LEN(DEMOEXT));

		if (protocol == com_protocol->integer)
			return qtrue;

		for (index = 0; demo_protocols[index]; index++) {
			if (demo_protocols[index] == protocol)
				return qtrue;
		}
	}

	return qfalse;
}

extern qboolean com_fullyInitialized;

long FS_FOpenFileReadDir(const char* filename, searchpath_t* search, fileHandle_t* file, qboolean uniqueFILE, qboolean unpure) {
	long hash;
	pack_t* pak;
	fileInPack_t* pakFile;
	directory_t* dir;
	char* netpath;
	FILE* filep;
	int len;

	if (filename == NULL)
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed");

	if (filename[0] == '/' || filename[0] == '\\')
		filename++;

	if (strstr(filename, "..") || strstr(filename, "::")) {
		if (file == NULL)
			return qfalse;

		*file = 0;
		return -1;
	}

	if (file == NULL) {

		if (search->pack) {
			hash = FS_HashFileName(filename, search->pack->hashSize);

			if (search->pack->hashTable[hash]) {
				pak = search->pack;
				pakFile = pak->hashTable[hash];

				do {
					if (!FS_FilenameCompare(pakFile->name, filename)) {
						if (pakFile->len)
							return pakFile->len;
						else {
							return 1;
						}
					}

					pakFile = pakFile->next;
				} while (pakFile != NULL);
			}
		} else if (search->dir) {
			dir = search->dir;

			netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
			filep = Sys_FOpen(netpath, "rb");

			if (filep) {
				len = FS_fplength(filep);
				fclose(filep);

				if (len)
					return len;
				else
					return 1;
			}
		}

		return 0;
	}

	*file = FS_HandleForFile();
	fsh[*file].handleFiles.unique = uniqueFILE;

	if (search->pack) {
		hash = FS_HashFileName(filename, search->pack->hashSize);

		if (search->pack->hashTable[hash]) {
			if (!unpure && !FS_PakIsPure(search->pack)) {
				*file = 0;
				return -1;
			}

			pak = search->pack;
			pakFile = pak->hashTable[hash];

			do {
				if (!FS_FilenameCompare(pakFile->name, filename)) {

					len = strlen(filename);

					if (!(pak->referenced & FS_GENERAL_REF)) {
						if (!FS_IsExt(filename, ".shader", len) &&
							!FS_IsExt(filename, ".txt", len) &&
							!FS_IsExt(filename, ".cfg", len) &&
							!FS_IsExt(filename, ".config", len) &&
							!FS_IsExt(filename, ".bot", len) &&
							!FS_IsExt(filename, ".arena", len) &&
							!FS_IsExt(filename, ".menu", len) &&
							!strstr(filename, "levelshots")) {
							pak->referenced |= FS_GENERAL_REF;
						}
					}

					if (strstr(filename, "cgame" ARCH_STRING DLL_EXT))
						pak->referenced |= FS_CGAME_REF;
					if (strstr(filename, "ui" ARCH_STRING DLL_EXT))
						pak->referenced |= FS_UI_REF;
					if (strstr(filename, "qagame" ARCH_STRING DLL_EXT))
						pak->referenced |= FS_QAGAME_REF;

					if (uniqueFILE) {
						fsh[*file].handleFiles.file.z = unzOpen(pak->pakFilename);

						if (fsh[*file].handleFiles.file.z == NULL)
							Com_Error(ERR_FATAL, "Couldn't open %s", pak->pakFilename);
					} else
						fsh[*file].handleFiles.file.z = pak->handle;

					Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
					fsh[*file].zipFile = qtrue;

					unzSetOffset(fsh[*file].handleFiles.file.z, pakFile->pos);

					unzOpenCurrentFile(fsh[*file].handleFiles.file.z);
					fsh[*file].zipFilePos = pakFile->pos;
					fsh[*file].zipFileLen = pakFile->len;

					if (fs_debug->integer) {
						Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n",
								   filename, pak->pakFilename);
					}

					return pakFile->len;
				}

				pakFile = pakFile->next;
			} while (pakFile != NULL);
		}
	} else if (search->dir) {

		len = strlen(filename);
		if (!unpure && fs_numServerPaks) {
			if (!FS_IsExt(filename, ".cfg", len) &&
				!FS_IsExt(filename, ".menu", len) &&
				!FS_IsExt(filename, ".game", len) &&
				!FS_IsExt(filename, ".dat", len) &&
				!FS_IsExt(filename, ".txt", len) &&
				Q_stricmpn(filename, "webimages/", 10) &&
				!FS_IsDemoExt(filename, len))
			{
				*file = 0;
				return -1;
			}
		}

		dir = search->dir;

		netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
		filep = Sys_FOpen(netpath, "rb");

		if (filep == NULL) {
			*file = 0;
			return -1;
		}

		Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
		fsh[*file].zipFile = qfalse;

		if (fs_debug->integer) {
			Com_Printf("FS_FOpenFileRead: %s (found in '%s%c%s')\n", filename,
					   dir->path, PATH_SEP, dir->gamedir);
		}

		fsh[*file].handleFiles.file.o = filep;
		return FS_fplength(filep);
	}

	*file = 0;
	return -1;
}

long FS_FOpenFileRead(const char* filename, fileHandle_t* file, qboolean uniqueFILE) {
	searchpath_t* search;
	long len;
	qboolean isLocalConfig;

	if (!fs_searchpaths)
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");

	isLocalConfig = !strcmp(filename, "autoexec.cfg") || !strcmp(filename, QZCONFIG_CFG) ||
	                !strcmp(filename, "server.cfg");
	for (search = fs_searchpaths; search; search = search->next) {
		if (isLocalConfig && search->pack)
			continue;

		len = FS_FOpenFileReadDir(filename, search, file, uniqueFILE, qfalse);

		if (file == NULL) {
			if (len > 0)
				return len;
		} else {
			if (len >= 0 && *file)
				return len;
		}
	}

#ifdef FS_MISSING
	if (missingFiles)
		fprintf(missingFiles, "%s\n", filename);
#endif

	if (file) {
		*file = 0;
		return -1;
	} else {
		return 0;
	}
}

void FS_FindVM(void** startSearch, char* found, int foundlen, const char* name, int enableDll) {
	searchpath_t* search, * lastSearch;
	directory_t* dir;
	char dllName[MAX_OSPATH];
	char* netpath;

	if (!fs_searchpaths)
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");

	if (!enableDll)
		return;

	Com_sprintf(dllName, sizeof(dllName), "%s" ARCH_STRING DLL_EXT, name);

	lastSearch = *startSearch;
	if (*startSearch == NULL)
		search = fs_searchpaths;
	else
		search = lastSearch->next;

	while (search) {
		if (search->dir) {
			dir = search->dir;
			netpath = FS_BuildOSPath(dir->path, dir->gamedir, dllName);

			if (FS_FileInPathExists(netpath)) {
				Q_strncpyz(found, netpath, foundlen);
				*startSearch = search;
				return;
			}
		}

		search = search->next;
	}
}

int FS_Read(void* buffer, int len, fileHandle_t f) {
	int block, remaining;
	int read;
	byte* buf;
	int tries;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!f) {
		return 0;
	}

	buf = (byte*)buffer;
	fs_readCount += len;

	if (fsh[f].zipFile == qfalse) {
		remaining = len;
		tries = 0;
		while (remaining) {
			block = remaining;
			read = fread(buf, 1, block, fsh[f].handleFiles.file.o);
			if (read == 0) {
				if (!tries) {
					tries = 1;
				} else {
					return len - remaining;
				}
			}

			if (read == -1) {
				Com_Error(ERR_FATAL, "FS_Read: -1 bytes read");
			}

			remaining -= read;
			buf += read;
		}
		return len;
	} else {
		return unzReadCurrentFile(fsh[f].handleFiles.file.z, buffer, len);
	}
}

int FS_Write(const void* buffer, int len, fileHandle_t h) {
	int block, remaining;
	int written;
	byte* buf;
	int tries;
	FILE* f;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!h) {
		return 0;
	}

	f = FS_FileForHandle(h);
	buf = (byte*)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite(buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Printf("FS_Write: 0 bytes written\n");
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf("FS_Write: -1 bytes written\n");
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if (fsh[h].handleSync) {
		fflush(f);
	}
	return len;
}

void QDECL FS_Printf(fileHandle_t h, const char* fmt, ...) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	FS_Write(msg, strlen(msg), h);
}

#define PK3_SEEK_BUFFER_SIZE 65536

int FS_Seek(fileHandle_t f, long offset, int origin) {
	int _origin;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
		return -1;
	}

	if (fsh[f].zipFile == qtrue) {
		byte buffer[PK3_SEEK_BUFFER_SIZE];
		int remainder;
		int currentPosition = FS_FTell(f);

		if (offset < 0) {
			switch (origin) {
			case FS_SEEK_END:
				remainder = fsh[f].zipFileLen + offset;
				break;

			case FS_SEEK_CUR:
				remainder = currentPosition + offset;
				break;

			case FS_SEEK_SET:
			default:
				remainder = 0;
				break;
			}

			if (remainder < 0) {
				remainder = 0;
			}

			origin = FS_SEEK_SET;
		} else {
			if (origin == FS_SEEK_END) {
				remainder = fsh[f].zipFileLen - currentPosition + offset;
			} else {
				remainder = offset;
			}
		}

		switch (origin) {
		case FS_SEEK_SET:
			if (remainder == currentPosition) {
				return offset;
			}
			unzSetOffset(fsh[f].handleFiles.file.z, fsh[f].zipFilePos);
			unzOpenCurrentFile(fsh[f].handleFiles.file.z);

		case FS_SEEK_END:
		case FS_SEEK_CUR:
			while (remainder > PK3_SEEK_BUFFER_SIZE) {
				FS_Read(buffer, PK3_SEEK_BUFFER_SIZE, f);
				remainder -= PK3_SEEK_BUFFER_SIZE;
			}
			FS_Read(buffer, remainder, f);
			return offset;

		default:
			Com_Error(ERR_FATAL, "Bad origin in FS_Seek");
			return -1;
		}
	} else {
		FILE* file;
		file = FS_FileForHandle(f);
		switch (origin) {
		case FS_SEEK_CUR:
			_origin = SEEK_CUR;
			break;
		case FS_SEEK_END:
			_origin = SEEK_END;
			break;
		case FS_SEEK_SET:
			_origin = SEEK_SET;
			break;
		default:
			Com_Error(ERR_FATAL, "Bad origin in FS_Seek");
			break;
		}

		return fseek(file, offset, _origin);
	}
}

qboolean FS_FileIsInEnginePak(const char* filename) {
	searchpath_t* search;
	fileInPack_t* pakFile;
	long hash;

	for (search = fs_searchpaths; search; search = search->next) {
		if (!search->pack) {
			continue;
		}
		hash = FS_HashFileName(filename, search->pack->hashSize);
		for (pakFile = search->pack->hashTable[hash]; pakFile; pakFile = pakFile->next) {
			if (!FS_FilenameCompare(pakFile->name, filename)) {
				return FS_IsEnginePak(search->pack);
			}
		}
	}
	return qfalse;
}

int FS_FileIsInPAK(const char* filename, int* pChecksum) {
	searchpath_t* search;
	pack_t* pak;
	fileInPack_t* pakFile;
	long hash = 0;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!filename) {
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed");
	}

	if (filename[0] == '/' || filename[0] == '\\') {
		filename++;
	}

	if (strstr(filename, "..") || strstr(filename, "::")) {
		return -1;
	}

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack) {
			hash = FS_HashFileName(filename, search->pack->hashSize);
		}
		if (search->pack && search->pack->hashTable[hash]) {
			if (!FS_PakIsPure(search->pack)) {
				continue;
			}

			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				if (!FS_FilenameCompare(pakFile->name, filename)) {
					if (pChecksum) {
						*pChecksum = pak->pure_checksum;
					}
					return 1;
				}
				pakFile = pakFile->next;
			} while (pakFile != NULL);
		}
	}
	return -1;
}

#ifndef DEDICATED
static long FS_WebPak_ReadFile(const char* name, void** buffer);
#endif

long FS_ReadFileDir(const char* qpath, void* searchPath, qboolean unpure, void** buffer) {
	fileHandle_t h;
	searchpath_t* search;
	byte* buf;
	qboolean isConfig;
	long len;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!qpath || !qpath[0]) {
		Com_Error(ERR_FATAL, "FS_ReadFile with empty name");
	}

#ifndef DEDICATED
	if (!Q_stricmpn(qpath, "webui/", 6)) {
		return FS_WebPak_ReadFile(qpath + 6, buffer);
	}
#endif

	buf = NULL;

	if (strstr(qpath, ".cfg")) {
		isConfig = qtrue;
		if (com_journal && com_journal->integer == 2) {
			int r;

			Com_DPrintf("Loading %s from journal file.\n", qpath);
			r = FS_Read(&len, sizeof(len), com_journalDataFile);
			if (r != sizeof(len)) {
				if (buffer != NULL)
					*buffer = NULL;
				return -1;
			}
			if (!len) {
				if (buffer == NULL) {
					return 1;
				}
				*buffer = NULL;
				return -1;
			}
			if (buffer == NULL) {
				return len;
			}

			buf = Hunk_AllocateTempMemory(len + 1);
			*buffer = buf;

			r = FS_Read(buf, len, com_journalDataFile);
			if (r != len) {
				Com_Error(ERR_FATAL, "Read from journalDataFile failed");
			}

			fs_loadCount++;
			fs_loadStack++;

			buf[len] = 0;

			return len;
		}
	} else {
		isConfig = qfalse;
	}

	search = searchPath;

	if (search == NULL) {
		len = FS_FOpenFileRead(qpath, &h, qfalse);
	} else {
		len = FS_FOpenFileReadDir(qpath, search, &h, qfalse, unpure);
	}

	if (h == 0) {
		if (buffer) {
			*buffer = NULL;
		}
		if (isConfig && com_journal && com_journal->integer == 1) {
			Com_DPrintf("Writing zero for %s to journal file.\n", qpath);
			len = 0;
			FS_Write(&len, sizeof(len), com_journalDataFile);
			FS_Flush(com_journalDataFile);
		}
		return -1;
	}

	if (!buffer) {
		if (isConfig && com_journal && com_journal->integer == 1) {
			Com_DPrintf("Writing len for %s to journal file.\n", qpath);
			FS_Write(&len, sizeof(len), com_journalDataFile);
			FS_Flush(com_journalDataFile);
		}
		FS_FCloseFile(h);
		return len;
	}

	fs_loadCount++;
	fs_loadStack++;

	buf = Hunk_AllocateTempMemory(len + 1);
	*buffer = buf;

	FS_Read(buf, len, h);

	buf[len] = 0;
	FS_FCloseFile(h);

	if (isConfig && com_journal && com_journal->integer == 1) {
		Com_DPrintf("Writing %s to journal file.\n", qpath);
		FS_Write(&len, sizeof(len), com_journalDataFile);
		FS_Write(buf, len, com_journalDataFile);
		FS_Flush(com_journalDataFile);
	}
	return len;
}

#ifndef DEDICATED

typedef struct {
	char name[MAX_QPATH];
	int offset;
	int length;
} webPakFile_t;

static webPakFile_t* webPakFiles;
static int numWebPakFiles;
static char webPakPath[MAX_OSPATH];
static qboolean webPakTried;

static int FS_WebPak_Compare(const void* a, const void* b) {
	return Q_stricmp(((const webPakFile_t*)a)->name, ((const webPakFile_t*)b)->name);
}

static void FS_WebPak_Open(void) {
	const char* bases[3];
	FILE* f = NULL;
	unsigned char head[9];
	unsigned char* table = NULL;
	unsigned char* index = NULL;
	int count, i, indexOff = -1, indexLen = 0, pos;

	webPakTried = qtrue;

	bases[0] = fs_basepath ? fs_basepath->string : "";
	bases[1] = fs_homepath ? fs_homepath->string : "";
	bases[2] = fs_steampath ? fs_steampath->string : "";
	for (i = 0; i < 3 && !f; i++) {
		if (!bases[i][0]) {
			continue;
		}
		Com_sprintf(webPakPath, sizeof(webPakPath), "%s%cweb.pak", bases[i], PATH_SEP);
		f = Sys_FOpen(webPakPath, "rb");
	}
	if (!f) {
		webPakPath[0] = '\0';
		return;
	}

	if (fread(head, 1, sizeof(head), f) != sizeof(head) || LittleLong(*(int*)head) != 4) {
		goto fail;
	}
	count = LittleLong(*(int*)(head + 4));
	if (count <= 0 || count > 65536) {
		goto fail;
	}

	table = Z_Malloc((count + 1) * 6);
	if (fread(table, 6, count + 1, f) != (size_t)(count + 1)) {
		goto fail;
	}
	for (i = 0; i < count; i++) {
		if ((table[i * 6] | (table[i * 6 + 1] << 8)) == 0) {
			indexOff = LittleLong(*(int*)(table + i * 6 + 2));
			indexLen = LittleLong(*(int*)(table + (i + 1) * 6 + 2)) - indexOff;
		}
	}
	if (indexOff <= 0 || indexLen <= 16 || indexLen > 4 * 1024 * 1024) {
		goto fail;
	}

	index = Z_Malloc(indexLen);
	if (fseek(f, indexOff, SEEK_SET) || fread(index, 1, indexLen, f) != (size_t)indexLen) {
		goto fail;
	}

	webPakFiles = Z_Malloc(count * sizeof(*webPakFiles));
	numWebPakFiles = 0;
	for (pos = 16; pos + 4 <= indexLen && numWebPakFiles < count;) {
		int len = LittleLong(*(int*)(index + pos)), id, k, n;
		webPakFile_t* w = &webPakFiles[numWebPakFiles];

		pos += 4;
		if (len <= 0 || len >= MAX_QPATH || pos + len * 2 + 8 > indexLen) {
			break;
		}
		for (k = 0; k < len; k++) {
			w->name[k] = (char)index[pos + k * 2];
		}
		w->name[len] = '\0';
		pos = (pos + len * 2 + 3) & ~3;
		id = LittleLong(*(int*)(index + pos + 4));
		pos += 8;

		for (n = 0; n < count; n++) {
			if ((table[n * 6] | (table[n * 6 + 1] << 8)) == id) {
				w->offset = LittleLong(*(int*)(table + n * 6 + 2));
				w->length = LittleLong(*(int*)(table + (n + 1) * 6 + 2)) - w->offset;
				break;
			}
		}
		if (n < count && w->length > 0) {
			numWebPakFiles++;
		}
	}
	qsort(webPakFiles, numWebPakFiles, sizeof(*webPakFiles), FS_WebPak_Compare);

	Z_Free(table);
	Z_Free(index);
	fclose(f);
	Com_Printf("web.pak: %d files from %s\n", numWebPakFiles, webPakPath);
	return;

fail:
	if (table) {
		Z_Free(table);
	}
	if (index) {
		Z_Free(index);
	}
	if (webPakFiles) {
		Z_Free(webPakFiles);
		webPakFiles = NULL;
	}
	numWebPakFiles = 0;
	fclose(f);
	Com_Printf("web.pak at %s could not be read\n", webPakPath);
	webPakPath[0] = '\0';
}

static long FS_WebPak_ReadFile(const char* name, void** buffer) {
	webPakFile_t key;
	const webPakFile_t* w;
	FILE* f;
	byte* buf;

	if (buffer) {
		*buffer = NULL;
	}
	if (!webPakTried) {
		FS_WebPak_Open();
	}
	if (!numWebPakFiles) {
		return -1;
	}

	Q_strncpyz(key.name, name, sizeof(key.name));
	w = bsearch(&key, webPakFiles, numWebPakFiles, sizeof(*webPakFiles), FS_WebPak_Compare);
	if (!w) {
		return -1;
	}
	if (!buffer) {
		return w->length;
	}

	f = Sys_FOpen(webPakPath, "rb");
	if (!f) {
		return -1;
	}
	fs_loadCount++;
	fs_loadStack++;
	buf = Hunk_AllocateTempMemory(w->length + 1);
	if (fseek(f, w->offset, SEEK_SET) || fread(buf, 1, w->length, f) != (size_t)w->length) {
		Com_Memset(buf, 0, w->length);
	}
	buf[w->length] = 0;
	fclose(f);

	*buffer = buf;
	return w->length;
}

#endif

long FS_ReadFile(const char* qpath, void** buffer) {
	return FS_ReadFileDir(qpath, NULL, qfalse, buffer);
}

void FS_FreeFile(void* buffer) {
	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}
	if (!buffer) {
		Com_Error(ERR_FATAL, "FS_FreeFile( NULL )");
	}
	fs_loadStack--;

	Hunk_FreeTempMemory(buffer);

	if (fs_loadStack == 0) {
		Hunk_ClearTempMemory();
	}
}

static qboolean FS_CacheFilePath(const char* name, char* path, int size) {
	const char* base = Sys_DefaultCachePath();

	if (!base || !base[0] || !name || !name[0] || strstr(name, "..") || strstr(name, "::")) {
		return qfalse;
	}
	Com_sprintf(path, size, "%s%c%s", base, PATH_SEP, name);
	FS_ReplaceSeparators(path);
	return qtrue;
}

long FS_ReadCacheFile(const char* name, void** buffer) {
	char path[MAX_OSPATH];
	FILE* f;
	long len;
	byte* buf;

	*buffer = NULL;
	if (!FS_CacheFilePath(name, path, sizeof(path))) {
		return -1;
	}
	f = Sys_FOpen(path, "rb");
	if (!f) {
		return -1;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len <= 0) {
		fclose(f);
		return -1;
	}

	fs_loadStack++;
	buf = Hunk_AllocateTempMemory(len + 1);
	if (fread(buf, 1, len, f) != (size_t)len) {
		fclose(f);
		FS_FreeFile(buf);
		return -1;
	}
	fclose(f);
	buf[len] = 0;
	*buffer = buf;
	return len;
}

void FS_WriteCacheFile(const char* name, const void* buffer, int size) {
	char path[MAX_OSPATH];
	FILE* f;

	if (!FS_CacheFilePath(name, path, sizeof(path)) || FS_CreatePath(path)) {
		return;
	}
	f = Sys_FOpen(path, "wb");
	if (!f) {
		Com_DPrintf("FS_WriteCacheFile: couldn't write %s\n", path);
		return;
	}
	fwrite(buffer, 1, size, f);
	fclose(f);
}

#ifndef DEDICATED
static void FS_RemoveOldPrefetch(const char* gameName) {
	char dir[MAX_OSPATH];
	char** list;
	int count;
	int i;

	if (!fs_homepath || !fs_homepath->string[0]) {
		return;
	}
	Q_strncpyz(dir, FS_BuildOSPath(fs_homepath->string, gameName, "prefetch"), sizeof(dir));
	list = Sys_ListFiles(dir, ".txt", NULL, &count, qfalse);
	if (!list) {
		return;
	}
	for (i = 0; i < count; i++) {
		remove(va("%s%c%s", dir, PATH_SEP, list[i]));
	}
	Sys_FreeFileList(list);
	Sys_RemoveDir(dir);
}
#endif

void FS_WriteFile(const char* qpath, const void* buffer, int size) {
	fileHandle_t f;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!qpath || !buffer) {
		Com_Error(ERR_FATAL, "FS_WriteFile: NULL parameter");
	}

	f = FS_FOpenFileWrite(qpath);
	if (!f) {
		Com_Printf("Failed to open %s\n", qpath);
		return;
	}

	FS_Write(buffer, size, f);

	FS_FCloseFile(f);
}

static pack_t* FS_LoadZipFile(const char* zipfile, const char* basename) {
	fileInPack_t* buildBuffer;
	pack_t* pack;
	unzFile uf;
	int err;
	unz_global_info gi;
	char filename_inzip[MAX_ZPATH];
	unz_file_info file_info;
	int i, len;
	long hash;
	int fs_numHeaderLongs;
	int* fs_headerLongs;
	char* namePtr;

	fs_numHeaderLongs = 0;

	uf = unzOpen(zipfile);
	err = unzGetGlobalInfo(uf, &gi);

	if (err != UNZ_OK)
		return NULL;

	len = 0;
	unzGoToFirstFile(uf);
	for (i = 0; i < gi.number_entry; i++) {
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK) {
			break;
		}
		len += strlen(filename_inzip) + 1;
		unzGoToNextFile(uf);
	}

	buildBuffer = Z_Malloc((gi.number_entry * sizeof(fileInPack_t)) + len);
	namePtr = ((char*)buildBuffer) + gi.number_entry * sizeof(fileInPack_t);
	fs_headerLongs = Z_Malloc((gi.number_entry + 1) * sizeof(int));
	fs_headerLongs[fs_numHeaderLongs++] = LittleLong(fs_checksumFeed);

	for (i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1) {
		if (i > gi.number_entry) {
			break;
		}
	}

	pack = Z_Malloc(sizeof(pack_t) + i * sizeof(fileInPack_t*));
	pack->hashSize = i;
	pack->hashTable = (fileInPack_t**)(((char*)pack) + sizeof(pack_t));
	for (i = 0; i < pack->hashSize; i++) {
		pack->hashTable[i] = NULL;
	}

	Q_strncpyz(pack->pakFilename, zipfile, sizeof(pack->pakFilename));
	Q_strncpyz(pack->pakBasename, basename, sizeof(pack->pakBasename));

	if (strlen(pack->pakBasename) > 4 && !Q_stricmp(pack->pakBasename + strlen(pack->pakBasename) - 4, ".pk3")) {
		pack->pakBasename[strlen(pack->pakBasename) - 4] = 0;
	}

	pack->handle = uf;
	pack->numfiles = gi.number_entry;
	unzGoToFirstFile(uf);

	for (i = 0; i < gi.number_entry; i++) {
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK) {
			break;
		}
		if (file_info.uncompressed_size > 0) {
			fs_headerLongs[fs_numHeaderLongs++] = LittleLong(file_info.crc);
		}
		Q_strlwr(filename_inzip);
		hash = FS_HashFileName(filename_inzip, pack->hashSize);
		buildBuffer[i].name = namePtr;
		strcpy(buildBuffer[i].name, filename_inzip);
		namePtr += strlen(filename_inzip) + 1;
		buildBuffer[i].pos = unzGetOffset(uf);
		buildBuffer[i].len = file_info.uncompressed_size;
		buildBuffer[i].csize = file_info.compressed_size;
		if (unzGetCurrentFileOffset(uf, &buildBuffer[i].offset) != UNZ_OK) {
			buildBuffer[i].offset = 0;
		}
		buildBuffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &buildBuffer[i];
		unzGoToNextFile(uf);
	}

	pack->checksum = Com_BlockChecksum(&fs_headerLongs[1], sizeof(*fs_headerLongs) * (fs_numHeaderLongs - 1));
	pack->pure_checksum = Com_BlockChecksum(fs_headerLongs, sizeof(*fs_headerLongs) * fs_numHeaderLongs);
	pack->checksum = LittleLong(pack->checksum);
	pack->pure_checksum = LittleLong(pack->pure_checksum);

	Z_Free(fs_headerLongs);

	pack->buildBuffer = buildBuffer;
	return pack;
}

static void FS_FreePak(pack_t* thepak) {
	unzClose(thepak->handle);
	Z_Free(thepak->buildBuffer);
	Z_Free(thepak);
}

qboolean FS_CompareZipChecksum(const char* zipfile) {
	pack_t* thepak;
	int index, checksum;

	thepak = FS_LoadZipFile(zipfile, "");

	if (!thepak)
		return qfalse;

	checksum = thepak->checksum;
	FS_FreePak(thepak);

	for (index = 0; index < fs_numServerReferencedPaks; index++) {
		if (checksum == fs_serverReferencedPaks[index])
			return qtrue;
	}

	return qfalse;
}

#define MAX_FOUND_FILES 0x1000

static int FS_ReturnPath(const char* zname, char* zpath, int* depth) {
	int len, at, newdep;

	newdep = 0;
	zpath[0] = 0;
	len = 0;
	at = 0;

	while (zname[at] != 0) {
		if (zname[at] == '/' || zname[at] == '\\') {
			len = at;
			newdep++;
		}
		at++;
	}
	strcpy(zpath, zname);
	zpath[len] = 0;
	*depth = newdep;

	return len;
}

static int FS_AddFileToList(char* name, char* list[MAX_FOUND_FILES], int nfiles) {
	int i;

	if (nfiles == MAX_FOUND_FILES - 1) {
		return nfiles;
	}
	for (i = 0; i < nfiles; i++) {
		if (!Q_stricmp(name, list[i])) {
			return nfiles;
		}
	}
	list[nfiles] = Z_Malloc((int)strlen(name) + 1);
	strcpy(list[nfiles], name);
	nfiles++;

	return nfiles;
}

char** FS_ListFilteredFiles(const char* path, const char* extension, char* filter, int* numfiles, qboolean allowNonPureFilesOnDisk) {
	int nfiles;
	char** listCopy;
	char* list[MAX_FOUND_FILES];
	searchpath_t* search;
	int i;
	int pathLength;
	int extensionLength;
	int length, pathDepth, temp;
	pack_t* pak;
	fileInPack_t* buildBuffer;
	char zpath[MAX_ZPATH];

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!path) {
		*numfiles = 0;
		return NULL;
	}
	if (!extension) {
		extension = "";
	}

	pathLength = strlen(path);
	if (path[pathLength - 1] == '\\' || path[pathLength - 1] == '/') {
		pathLength--;
	}
	extensionLength = strlen(extension);
	nfiles = 0;
	FS_ReturnPath(path, zpath, &pathDepth);

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack) {
			if (!FS_PakIsPure(search->pack)) {
				continue;
			}

			pak = search->pack;
			buildBuffer = pak->buildBuffer;
			for (i = 0; i < pak->numfiles; i++) {
				char* name;
				int zpathLen, depth;

				name = buildBuffer[i].name;
				if (filter) {
					if (!Com_FilterPath(filter, name, qfalse))
						continue;
					nfiles = FS_AddFileToList(name, list, nfiles);
				} else {
					zpathLen = FS_ReturnPath(name, zpath, &depth);

					if ((depth - pathDepth) > 2 || pathLength > zpathLen || Q_stricmpn(name, path, pathLength)) {
						continue;
					}

					length = strlen(name);
					if (length < extensionLength) {
						continue;
					}

					if (Q_stricmp(name + length - extensionLength, extension)) {
						continue;
					}

					temp = pathLength;
					if (pathLength) {
						temp++;
					}
					nfiles = FS_AddFileToList(name + temp, list, nfiles);
				}
			}
		} else if (search->dir) {
			char* netpath;
			int numSysFiles;
			char** sysFiles;
			char* name;

			if (fs_numServerPaks && !allowNonPureFilesOnDisk) {
				continue;
			} else {
				netpath = FS_BuildOSPath(search->dir->path, search->dir->gamedir, path);
				sysFiles = Sys_ListFiles(netpath, extension, filter, &numSysFiles, qfalse);
				for (i = 0; i < numSysFiles; i++) {
					name = sysFiles[i];
					nfiles = FS_AddFileToList(name, list, nfiles);
				}
				Sys_FreeFileList(sysFiles);
			}
		}
	}

	*numfiles = nfiles;

	if (!nfiles) {
		return NULL;
	}

	listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
	for (i = 0; i < nfiles; i++) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

char** FS_ListFiles(const char* path, const char* extension, int* numfiles) {
	return FS_ListFilteredFiles(path, extension, NULL, numfiles, qfalse);
}

void FS_FreeFileList(char** list) {
	int i;

	if (!fs_searchpaths) {
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	}

	if (!list) {
		return;
	}

	for (i = 0; list[i]; i++) {
		Z_Free(list[i]);
	}

	Z_Free(list);
}

int FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize) {
	int nFiles, i, nTotal, nLen;
	char** pFiles = NULL;

	*listbuf = 0;
	nFiles = 0;
	nTotal = 0;

	if (Q_stricmp(path, "$modlist") == 0) {
		return FS_GetModList(listbuf, bufsize);
	}

	pFiles = FS_ListFiles(path, extension, &nFiles);

	for (i = 0; i < nFiles; i++) {
		nLen = strlen(pFiles[i]) + 1;
		if (nTotal + nLen + 1 < bufsize) {
			strcpy(listbuf, pFiles[i]);
			listbuf += nLen;
			nTotal += nLen;
		} else {
			nFiles = i;
			break;
		}
	}

	FS_FreeFileList(pFiles);

	return nFiles;
}

static unsigned int Sys_CountFileList(char** list) {
	int i = 0;

	if (list) {
		while (*list) {
			list++;
			i++;
		}
	}
	return i;
}

static char** Sys_ConcatenateFileLists(char** list0, char** list1) {
	int totalLength = 0;
	char** cat = NULL, ** dst, ** src;

	totalLength += Sys_CountFileList(list0);
	totalLength += Sys_CountFileList(list1);

	dst = cat = Z_Malloc((totalLength + 1) * sizeof(char*));

	if (list0) {
		for (src = list0; *src; src++, dst++)
			*dst = *src;
	}
	if (list1) {
		for (src = list1; *src; src++, dst++)
			*dst = *src;
	}

	*dst = NULL;

	if (list0)
		Z_Free(list0);
	if (list1)
		Z_Free(list1);

	return cat;
}

void FS_GetModDescription(const char* modDir, char* description, int descriptionLen) {
	fileHandle_t descHandle;
	char descPath[MAX_QPATH];
	int nDescLen;
	FILE* file;

	Com_sprintf(descPath, sizeof(descPath), "%s%cdescription.txt", modDir, PATH_SEP);
	nDescLen = FS_SV_FOpenFileRead(descPath, &descHandle);

	if (nDescLen > 0) {
		file = FS_FileForHandle(descHandle);
		Com_Memset(description, 0, descriptionLen);
		nDescLen = fread(description, 1, descriptionLen, file);
		if (nDescLen >= 0) {
			description[nDescLen] = '\0';
		}
	} else {
		Q_strncpyz(description, modDir, descriptionLen);
	}

	if (descHandle) {
		FS_FCloseFile(descHandle);
	}
}

int FS_GetModList(char* listbuf, int bufsize) {
	int nMods, i, j, k, nTotal, nLen, nPaks, nDirs, nPakDirs, nPotential, nDescLen;
	char** pFiles = NULL;
	char** pPaks = NULL;
	char** pDirs = NULL;
	char* name, * path;
	char description[MAX_OSPATH];

	int dummy;
	char** pFiles0 = NULL;
	qboolean bDrop = qfalse;

	const char* const paths[] = { fs_basepath->string, fs_homepath->string, fs_steampath->string };

	*listbuf = 0;
	nMods = nTotal = 0;

	for (i = 0; i < ARRAY_LEN(paths); i++) {
		pFiles0 = Sys_ListFiles(paths[i], NULL, NULL, &dummy, qtrue);
		pFiles = Sys_ConcatenateFileLists(pFiles, pFiles0);
	}

	nPotential = Sys_CountFileList(pFiles);

	for (i = 0; i < nPotential; i++) {
		name = pFiles[i];
		if (i != 0) {
			bDrop = qfalse;
			for (j = 0; j < i; j++) {
				if (Q_stricmp(pFiles[j], name) == 0) {
					bDrop = qtrue;
					break;
				}
			}
		}
		if (bDrop || Q_stricmp(name, com_basegame->string) == 0 || Q_stricmpn(name, ".", 1) == 0) {
			continue;
		}

		for (j = 0; j < ARRAY_LEN(paths); j++) {
			path = FS_BuildOSPath(paths[j], name, "");
			nPaks = nDirs = nPakDirs = 0;
			pPaks = Sys_ListFiles(path, ".pk3", NULL, &nPaks, qfalse);
			pDirs = Sys_ListFiles(path, "/", NULL, &nDirs, qfalse);
			for (k = 0; k < nDirs; k++) {
				if (FS_IsExt(pDirs[k], ".pk3dir", strlen(pDirs[k]))) {
					nPakDirs++;
				}
			}
			Sys_FreeFileList(pPaks);
			Sys_FreeFileList(pDirs);

			if (nPaks > 0 || nPakDirs > 0) {
				break;
			}
		}

		if (nPaks > 0 || nPakDirs > 0) {
			nLen = strlen(name) + 1;
			FS_GetModDescription(name, description, sizeof(description));
			nDescLen = strlen(description) + 1;

			if (nTotal + nLen + 1 + nDescLen + 1 < bufsize) {
				strcpy(listbuf, name);
				listbuf += nLen;
				strcpy(listbuf, description);
				listbuf += nDescLen;
				nTotal += nLen + nDescLen;
				nMods++;
			} else {
				break;
			}
		}
	}
	Sys_FreeFileList(pFiles);

	return nMods;
}

void FS_Dir_f(void) {
	char* path;
	char* extension;
	char** dirnames;
	int ndirs;
	int i;

	if (Cmd_Argc() < 2 || Cmd_Argc() > 3) {
		Com_Printf("usage: dir <directory> [extension]\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		path = Cmd_Argv(1);
		extension = "";
	} else {
		path = Cmd_Argv(1);
		extension = Cmd_Argv(2);
	}

	Com_Printf("Directory of %s %s\n", path, extension);
	Com_Printf("---------------\n");

	dirnames = FS_ListFiles(path, extension, &ndirs);

	for (i = 0; i < ndirs; i++) {
		Com_Printf("%s\n", dirnames[i]);
	}
	FS_FreeFileList(dirnames);
}

void FS_ConvertPath(char* s) {
	while (*s) {
		if (*s == '\\' || *s == ':') {
			*s = '/';
		}
		s++;
	}
}

int FS_PathCmp(const char* s1, const char* s2) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if (c1 == '\\' || c1 == ':') {
			c1 = '/';
		}
		if (c2 == '\\' || c2 == ':') {
			c2 = '/';
		}

		if (c1 < c2) {
			return -1;
		}
		if (c1 > c2) {
			return 1;
		}
	} while (c1);

	return 0;
}

void FS_SortFileList(char** filelist, int numfiles) {
	int i, j, k, numsortedfiles;
	char** sortedlist;

	sortedlist = Z_Malloc((numfiles + 1) * sizeof(*sortedlist));
	sortedlist[0] = NULL;
	numsortedfiles = 0;
	for (i = 0; i < numfiles; i++) {
		for (j = 0; j < numsortedfiles; j++) {
			if (FS_PathCmp(filelist[i], sortedlist[j]) < 0) {
				break;
			}
		}
		for (k = numsortedfiles; k > j; k--) {
			sortedlist[k] = sortedlist[k - 1];
		}
		sortedlist[j] = filelist[i];
		numsortedfiles++;
	}
	Com_Memcpy(filelist, sortedlist, numfiles * sizeof(*filelist));
	Z_Free(sortedlist);
}

void FS_NewDir_f(void) {
	char* filter;
	char** dirnames;
	int ndirs;
	int i;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: fdir <filter>\n");
		Com_Printf("example: fdir *q3dm*.bsp\n");
		return;
	}

	filter = Cmd_Argv(1);

	Com_Printf("---------------\n");

	dirnames = FS_ListFilteredFiles("", "", filter, &ndirs, qfalse);

	FS_SortFileList(dirnames, ndirs);

	for (i = 0; i < ndirs; i++) {
		FS_ConvertPath(dirnames[i]);
		Com_Printf("%s\n", dirnames[i]);
	}
	Com_Printf("%d files listed\n", ndirs);
	FS_FreeFileList(dirnames);
}

void FS_Path_f(void) {
	searchpath_t* s;

	Com_Printf("Current search path:\n");
	for (s = fs_searchpaths; s; s = s->next) {
		if (s->pack) {
			Com_Printf("%s (0x%08x - %i files)\n", s->pack->pakFilename, s->pack->checksum, s->pack->numfiles);
			if (fs_numServerPaks) {
				if (!FS_PakIsPure(s->pack)) {
					Com_Printf("    not on the pure list\n");
				} else {
					Com_Printf("    on the pure list\n");
				}
			}
		} else {
			Com_Printf("%s%c%s\n", s->dir->path, PATH_SEP, s->dir->gamedir);
		}
	}

}

void FS_TouchFile_f(void) {
	fileHandle_t f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: touchFile <file>\n");
		return;
	}

	FS_FOpenFileRead(Cmd_Argv(1), &f, qfalse);
	if (f) {
		FS_FCloseFile(f);
	}
}

qboolean FS_Which(const char* filename, void* searchPath) {
	searchpath_t* search = searchPath;

	if (FS_FOpenFileReadDir(filename, search, NULL, qfalse, qfalse) > 0) {
		if (search->pack) {
			Com_Printf("File \"%s\" found in \"%s\"\n", filename, search->pack->pakFilename);
			return qtrue;
		} else if (search->dir) {
			Com_Printf("File \"%s\" found at \"%s\"\n", filename, search->dir->fullpath);
			return qtrue;
		}
	}

	return qfalse;
}

void FS_Which_f(void) {
	searchpath_t* search;
	char* filename;

	filename = Cmd_Argv(1);

	if (!filename[0]) {
		Com_Printf("Usage: which <file>\n");
		return;
	}

	if (filename[0] == '/' || filename[0] == '\\') {
		filename++;
	}

	for (search = fs_searchpaths; search; search = search->next) {
		if (FS_Which(filename, search))
			return;
	}

	Com_Printf("File not found: \"%s\"\n", filename);
}

static int QDECL paksort(const void* a, const void* b) {
	char* aa, * bb;

	aa = *(char**)a;
	bb = *(char**)b;

	return FS_PathCmp(aa, bb);
}

void FS_AddGameDirectory(const char* path, const char* dir) {
	searchpath_t* sp;
	searchpath_t* search;
	pack_t* pak;
	char curpath[MAX_OSPATH + 1], * pakfile;
	int numfiles;
	char** pakfiles;
	int pakfilesi;
	char** pakfilestmp;
	int numdirs;
	char** pakdirs;
	int pakdirsi;
	char** pakdirstmp;

	int pakwhich;
	int len;

	for (sp = fs_searchpaths; sp; sp = sp->next) {
		if (sp->dir && !Q_stricmp(sp->dir->path, path) && !Q_stricmp(sp->dir->gamedir, dir)) {
			return;
		}
	}

	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	Q_strncpyz(curpath, FS_BuildOSPath(path, dir, ""), sizeof(curpath));
	curpath[strlen(curpath) - 1] = '\0';

	pakfiles = Sys_ListFiles(curpath, ".pk3", NULL, &numfiles, qfalse);

	if (pakfiles) {
		qsort(pakfiles, numfiles, sizeof(char*), paksort);
	}

	if (fs_numServerPaks) {
		numdirs = 0;
		pakdirs = NULL;
	} else {
		pakdirs = Sys_ListFiles(curpath, "/", NULL, &numdirs, qfalse);

		qsort(pakdirs, numdirs, sizeof(char*), paksort);
	}

	pakfilesi = 0;
	pakdirsi = 0;

	while ((pakfilesi < numfiles) || (pakdirsi < numdirs)) {
		if (pakfilesi >= numfiles) {
			pakwhich = 0;
		} else if (pakdirsi >= numdirs) {
			pakwhich = 1;
		} else {
			pakfilestmp = &pakfiles[pakfilesi];
			pakdirstmp = &pakdirs[pakdirsi];
			pakwhich = (paksort(pakfilestmp, pakdirstmp) < 0);
		}

		if (pakwhich) {
			if (!Q_stricmp(pakfiles[pakfilesi], IOBIN_FILENAME) &&
			    Q_stricmp(path, fs_basepath->string) != 0
#ifdef __APPLE__
			    && (!fs_apppath->string[0] || Q_stricmp(path, fs_apppath->string) != 0)
#endif
			    ) {
				Com_DPrintf("FS_AddGameDirectory: rejecting %s from non-canonical path '%s'\n",
				            pakfiles[pakfilesi], path);
				pakfilesi++;
				continue;
			}

			pakfile = FS_BuildOSPath(path, dir, pakfiles[pakfilesi]);
			if ((pak = FS_LoadZipFile(pakfile, pakfiles[pakfilesi])) == 0) {
				pakfilesi++;
				continue;
			}

			Q_strncpyz(pak->pakPathname, curpath, sizeof(pak->pakPathname));
			Q_strncpyz(pak->pakGamename, dir, sizeof(pak->pakGamename));

			fs_packFiles += pak->numfiles;

			search = Z_Malloc(sizeof(searchpath_t));
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;

			pakfilesi++;
		} else {
			len = strlen(pakdirs[pakdirsi]);
			if (!FS_IsExt(pakdirs[pakdirsi], ".pk3dir", len)) {
				pakdirsi++;
				continue;
			}

			pakfile = FS_BuildOSPath(path, dir, pakdirs[pakdirsi]);

			search = Z_Malloc(sizeof(searchpath_t));
			search->dir = Z_Malloc(sizeof(*search->dir));

			Q_strncpyz(search->dir->path, curpath, sizeof(search->dir->path));
			Q_strncpyz(search->dir->fullpath, pakfile, sizeof(search->dir->fullpath));
			Q_strncpyz(search->dir->gamedir, pakdirs[pakdirsi], sizeof(search->dir->gamedir));

			search->next = fs_searchpaths;
			fs_searchpaths = search;

			pakdirsi++;
		}
	}

	Sys_FreeFileList(pakfiles);
	Sys_FreeFileList(pakdirs);

	search = Z_Malloc(sizeof(searchpath_t));
	search->dir = Z_Malloc(sizeof(*search->dir));

	Q_strncpyz(search->dir->path, path, sizeof(search->dir->path));
	Q_strncpyz(search->dir->fullpath, curpath, sizeof(search->dir->fullpath));
	Q_strncpyz(search->dir->gamedir, dir, sizeof(search->dir->gamedir));

	search->next = fs_searchpaths;
	fs_searchpaths = search;
}

qboolean FS_CheckDirTraversal(const char* checkdir) {
	if (strstr(checkdir, "../") || strstr(checkdir, "..\\"))
		return qtrue;

	return qfalse;
}

qboolean FS_InvalidGameDir(const char* gamedir) {
	if (!strcmp(gamedir, ".") || !strcmp(gamedir, "..") || strchr(gamedir, '/') || strchr(gamedir, '\\')) {
		return qtrue;
	}

	return qfalse;
}

qboolean FS_ComparePaks(char* neededpaks, int len, qboolean dlstring) {
	searchpath_t* sp;
	qboolean havepak;
	char* origpos = neededpaks;
	int i;

	if (!fs_numServerReferencedPaks)
		return qfalse;

	*neededpaks = 0;

	for (i = 0; i < fs_numServerReferencedPaks; i++) {
		havepak = qfalse;

		if (FS_CheckDirTraversal(fs_serverReferencedPakNames[i])) {
			Com_Printf("WARNING: Invalid download name %s\n", fs_serverReferencedPakNames[i]);
			continue;
		}

		for (sp = fs_searchpaths; sp; sp = sp->next) {
			if (sp->pack && sp->pack->checksum == fs_serverReferencedPaks[i]) {
				havepak = qtrue;
				break;
			}
		}

		if (!havepak && fs_serverReferencedPakNames[i] && *fs_serverReferencedPakNames[i]) {

			if (dlstring) {

				origpos += strlen(origpos);

				Q_strcat(neededpaks, len, "@");
				Q_strcat(neededpaks, len, fs_serverReferencedPakNames[i]);
				Q_strcat(neededpaks, len, ".pk3");

				Q_strcat(neededpaks, len, "@");
				if (FS_SV_FileExists(va("%s.pk3", fs_serverReferencedPakNames[i]))) {
					char st[MAX_ZPATH];
					Com_sprintf(st, sizeof(st), "%s.%08x.pk3", fs_serverReferencedPakNames[i], fs_serverReferencedPaks[i]);
					Q_strcat(neededpaks, len, st);
				} else {
					Q_strcat(neededpaks, len, fs_serverReferencedPakNames[i]);
					Q_strcat(neededpaks, len, ".pk3");
				}

				if (strlen(origpos) + (origpos - neededpaks) >= len - 1) {
					*origpos = '\0';
					break;
				}
			} else {
				Q_strcat(neededpaks, len, fs_serverReferencedPakNames[i]);
				Q_strcat(neededpaks, len, ".pk3");
				if (FS_SV_FileExists(va("%s.pk3", fs_serverReferencedPakNames[i]))) {
					Q_strcat(neededpaks, len, " (local file exists with wrong checksum)");
				}
				Q_strcat(neededpaks, len, "\n");
			}
		}
	}

	if (*neededpaks) {
		return qtrue;
	}

	return qfalse;
}

void FS_Shutdown(qboolean closemfp) {
	searchpath_t* p, * next;
	int i;

	for (i = 0; i < MAX_FILE_HANDLES; i++) {
		if (fsh[i].fileSize) {
			FS_FCloseFile(i);
		}
	}

	for (p = fs_searchpaths; p; p = next) {
		next = p->next;

		if (p->pack)
			FS_FreePak(p->pack);
		if (p->dir)
			Z_Free(p->dir);

		Z_Free(p);
	}

	fs_searchpaths = NULL;

	Cmd_RemoveCommand("path");
	Cmd_RemoveCommand("dir");
	Cmd_RemoveCommand("fdir");
	Cmd_RemoveCommand("touchFile");

#ifdef FS_MISSING
	if (closemfp) {
		fclose(missingFiles);
	}
#endif
}

#define FS_MAX_STOCK_SHADERS 16384
#define FS_MAX_CHECKED_PAKS 512

static unsigned int fsStockShaders[FS_MAX_STOCK_SHADERS];
static int fsNumStockShaders = -1;
typedef struct {
	int checksum;
	int same;
	int redefined;
	char first[MAX_QPATH];
} fsPakCheck_t;

static fsPakCheck_t fsCheckedPaks[FS_MAX_CHECKED_PAKS];
static int fsNumCheckedPaks;

static unsigned int FS_ShaderNameHash(const char* name) {
	unsigned int h = 2166136261u;

	for (; *name; name++) {
		h = (h ^ (unsigned char)tolower((unsigned char)*name)) * 16777619u;
	}
	return h;
}

static int FS_CompareShaderHashes(const void* a, const void* b) {
	unsigned int x = *(const unsigned int*)a;
	unsigned int y = *(const unsigned int*)b;

	return (x > y) - (x < y);
}

static int FS_ScanShaderFile(const char* name, searchpath_t* search, qboolean stock, char* first, int firstSize) {
	char shader[MAX_QPATH];
	char* buf;
	char* p;
	char* token;
	int count = 0;

	FS_ReadFileDir(name, search, qtrue, (void**)&buf);
	if (!buf) {
		return 0;
	}
	p = buf;
	COM_BeginParseSession(name);
	while (p) {
		token = COM_ParseExt(&p, qtrue);
		if (!token[0]) {
			break;
		}
		Q_strncpyz(shader, token, sizeof(shader));
		token = COM_ParseExt(&p, qtrue);
		if (token[0] != '{' || token[1]) {
			if (!p) {
				break;
			}
			SkipRestOfLine(&p);
			continue;
		}
		if (!SkipBracedSection(&p, 1)) {
			break;
		}
		if (stock) {
			if (fsNumStockShaders < FS_MAX_STOCK_SHADERS) {
				fsStockShaders[fsNumStockShaders++] = FS_ShaderNameHash(shader);
			}
		} else {
			unsigned int h = FS_ShaderNameHash(shader);

			if (bsearch(&h, fsStockShaders, fsNumStockShaders, sizeof(h), FS_CompareShaderHashes)) {
				if (!count) {
					Q_strncpyz(first, shader, firstSize);
				}
				count++;
			}
		}
	}
	FS_FreeFile(buf);
	return count;
}

static qboolean FS_IsShaderScript(const char* name) {
	return (!Q_stricmpn(name, "scripts/", 8) && !Q_stricmp(COM_GetExtension(name), "shader")) ? qtrue : qfalse;
}

static void FS_CheckWorkshopPak(searchpath_t* search, searchpath_t* stock, fsPakCheck_t* check) {
	pack_t* pak = search->pack;
	int j;

	if (fsNumStockShaders < 0) {
		fsNumStockShaders = 0;
		for (j = 0; j < stock->pack->numfiles; j++) {
			if (FS_IsShaderScript(stock->pack->buildBuffer[j].name)) {
				FS_ScanShaderFile(stock->pack->buildBuffer[j].name, stock, qtrue, NULL, 0);
			}
		}
		qsort(fsStockShaders, fsNumStockShaders, sizeof(fsStockShaders[0]), FS_CompareShaderHashes);
	}

	for (j = 0; j < pak->numfiles; j++) {
		const char* name = pak->buildBuffer[j].name;
		char shader[MAX_QPATH];
		int n;

		if (name[0] && name[strlen(name) - 1] == '/') {
			continue;
		}
		if (FS_FOpenFileReadDir(name, stock, NULL, qfalse, qtrue) > 0) {
			check->same++;
		} else if (FS_IsShaderScript(name)) {
			n = FS_ScanShaderFile(name, search, qfalse, shader, sizeof(shader));
			if (n && !check->redefined) {
				Q_strncpyz(check->first, shader, sizeof(check->first));
			}
			check->redefined += n;
		}
	}
}

static void FS_CheckWorkshopPaks(void) {
	searchpath_t* stock = NULL;
	searchpath_t* search;
	int i;

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack && !search->pack->workshopId && !Q_stricmp(search->pack->pakBasename, "pak00")) {
			stock = search;
			break;
		}
	}
	if (!stock) {
		return;
	}

	for (search = fs_searchpaths; search; search = search->next) {
		pack_t* pak = search->pack;
		fsPakCheck_t* check;

		if (!pak || !pak->workshopId) {
			continue;
		}
		for (i = 0; i < fsNumCheckedPaks && fsCheckedPaks[i].checksum != pak->checksum; i++) {
		}
		check = &fsCheckedPaks[i];
		if (i == fsNumCheckedPaks) {
			if (fsNumCheckedPaks == FS_MAX_CHECKED_PAKS) {
				continue;
			}
			fsNumCheckedPaks++;
			Com_Memset(check, 0, sizeof(*check));
			check->checksum = pak->checksum;
			FS_CheckWorkshopPak(search, stock, check);
		}

		if (check->same) {
			Com_Printf("%s.pk3 (Workshop %llu): %i files that pak00 also has, and pak00's are the ones used\n",
			           pak->pakBasename, (unsigned long long)pak->workshopId, check->same);
		}
		if (check->redefined) {
			Com_Printf(S_COLOR_YELLOW "%s.pk3 (Workshop %llu) defines %i of pak00's shaders again (%s%s): every map "
			           "uses its version\n", pak->pakBasename, (unsigned long long)pak->workshopId, check->redefined,
			           check->first, check->redefined > 1 ? ", ..." : "");
		}
	}
}

static void FS_ReorderPurePaks(void) {
	searchpath_t* s;
	int i;
	searchpath_t** p_insert_index,
		** p_previous;

	fs_reordered = qfalse;

	if (!fs_numServerPaks)
		return;

	p_insert_index = &fs_searchpaths;

	while (*p_insert_index && (*p_insert_index)->dir) {
		p_insert_index = &(*p_insert_index)->next;
	}

	for (i = 0; i < fs_numServerPaks; i++) {
		p_previous = p_insert_index;
		for (s = *p_insert_index; s; s = s->next) {
			if (s->pack && fs_serverPaks[i] == s->pack->checksum) {
				fs_reordered = qtrue;
				*p_previous = s->next;
				s->next = *p_insert_index;
				*p_insert_index = s;
				p_insert_index = &s->next;
				break;
			}
			p_previous = &s->next;
		}
	}
}

static void FS_AddWorkshop(void);

static void FS_Startup(const char* gameName) {
	const char* homePath;

	Com_Printf("----- FS_Startup -----\n");

	fs_packFiles = 0;

	fs_debug = Cvar_Get("fs_debug", "0", 0);
	fs_basepath = Cvar_Get("fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT | CVAR_PROTECTED);
	fs_basegame = Cvar_Get("fs_basegame", "", CVAR_INIT);
	Cvar_Get("fs_skipWorkshop", "0", CVAR_INIT);
	homePath = Cvar_VariableString("com_homepath")[0] ? Sys_DefaultHomePath() : NULL;
	if (!homePath || !homePath[0]) {
		homePath = fs_basepath->string;
	}
	fs_homepath = Cvar_Get("fs_homepath", homePath, CVAR_INIT | CVAR_PROTECTED);
	fs_gamedirvar = Cvar_Get("fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO);
#ifndef DEDICATED
	FS_RemoveOldPrefetch(gameName);
#endif

	if (!gameName[0]) {
		Cvar_ForceReset("com_basegame");
	}

	if (!FS_FilenameCompare(fs_gamedirvar->string, gameName)) {
		Cvar_ForceReset("fs_game");
	}

	if (FS_InvalidGameDir(gameName)) {
		Com_Error(ERR_DROP, "Invalid com_basegame '%s'", gameName);
	}
	if (FS_InvalidGameDir(fs_basegame->string)) {
		Com_Error(ERR_DROP, "Invalid fs_basegame '%s'", fs_basegame->string);
	}
	if (FS_InvalidGameDir(fs_gamedirvar->string)) {
		Com_Error(ERR_DROP, "Invalid fs_game '%s'", fs_gamedirvar->string);
	}

	fs_steampath = Cvar_Get("fs_steampath", Sys_SteamPath(), CVAR_INIT | CVAR_PROTECTED);

	FS_AddWorkshop();
	if (fs_steampath->string[0]) {
		FS_AddGameDirectory(fs_steampath->string, gameName);
	}
	if (fs_basepath->string[0]) {
		FS_AddGameDirectory(fs_basepath->string, gameName);
	}

#ifdef __APPLE__
	fs_apppath = Cvar_Get("fs_apppath", Sys_DefaultAppPath(), CVAR_INIT | CVAR_PROTECTED);
	if (fs_apppath->string[0])
		FS_AddGameDirectory(fs_apppath->string, gameName);
#endif

	if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string)) {
		FS_CreatePath(fs_homepath->string);
		FS_AddGameDirectory(fs_homepath->string, gameName);
	}

	if (fs_basegame->string[0] && Q_stricmp(fs_basegame->string, gameName)) {
		if (fs_steampath->string[0]) {
			FS_AddGameDirectory(fs_steampath->string, fs_basegame->string);
		}
		if (fs_basepath->string[0]) {
			FS_AddGameDirectory(fs_basepath->string, fs_basegame->string);
		}
		if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string)) {
			FS_AddGameDirectory(fs_homepath->string, fs_basegame->string);
		}
	}

	if (fs_gamedirvar->string[0] && Q_stricmp(fs_gamedirvar->string, gameName)) {
		if (fs_steampath->string[0]) {
			FS_AddGameDirectory(fs_steampath->string, fs_gamedirvar->string);
		}
		if (fs_basepath->string[0]) {
			FS_AddGameDirectory(fs_basepath->string, fs_gamedirvar->string);
		}
		if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string)) {
			FS_AddGameDirectory(fs_homepath->string, fs_gamedirvar->string);
		}
	}

	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("dir", FS_Which_f);
	Cmd_AddCommand("fdir", FS_NewDir_f);
	Cmd_AddCommand("touchFile", FS_TouchFile_f);

	FS_ReorderPurePaks();

	FS_Path_f();

	FS_CheckWorkshopPaks();

	fs_gamedirvar->modified = qfalse;

	Com_Printf("----------------------\n");

#ifdef FS_MISSING
	if (missingFiles == NULL) {
		missingFiles = Sys_FOpen("\\missing.txt", "ab");
	}
#endif
	Com_Printf("%d files in pk3 files\n", fs_packFiles);
}

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#define QL_PAK00_SIZE   962052238ULL
#define QL_PAK00_MD5    "75aaae7c836b9ebdb1d4cfd53ba1c958"

static qboolean FS_FileExistsOS(const char *path) {
	DWORD attr = GetFileAttributesA(path);
	return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static qboolean FS_DirExistsOS(const char *path) {
	DWORD attr = GetFileAttributesA(path);
	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

static qboolean FS_FindSteamQLPath(char *qlPath, int pathSize) {
	char *steamPath;
	char testPath[MAX_OSPATH];
	HKEY steamRegKey;
	char steamInstall[MAX_OSPATH];
	DWORD pathLen;
	FILE *vdf;
	char line[1024];

	steamPath = Sys_SteamPath();
	if (steamPath[0]) {
		Com_sprintf(testPath, sizeof(testPath), "%s\\baseq3\\pak00.pk3", steamPath);
		if (FS_FileExistsOS(testPath)) {
			Q_strncpyz(qlPath, steamPath, pathSize);
			return qtrue;
		}
	}

	steamInstall[0] = '\0';
	if (!RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0,
	                  KEY_QUERY_VALUE, &steamRegKey)) {
		pathLen = sizeof(steamInstall);
		if (RegQueryValueEx(steamRegKey, "SteamPath", NULL, NULL,
		                    (LPBYTE)steamInstall, &pathLen)) {
			if (RegQueryValueEx(steamRegKey, "InstallPath", NULL, NULL,
			                    (LPBYTE)steamInstall, &pathLen)) {
				steamInstall[0] = '\0';
			}
		}
		RegCloseKey(steamRegKey);
	}

	if (!steamInstall[0])
		return qfalse;

	{
		char *s;
		for (s = steamInstall; *s; s++) {
			if (*s == '/') *s = '\\';
		}
	}

	Com_sprintf(testPath, sizeof(testPath), "%s\\steamapps\\libraryfolders.vdf", steamInstall);
	vdf = fopen(testPath, "r");
	if (!vdf)
		return qfalse;

	{
		char currentPath[MAX_OSPATH];
		qboolean hasPath = qfalse;

		currentPath[0] = '\0';
		while (fgets(line, sizeof(line), vdf)) {
			char *p, *val;
			for (p = line; *p == ' ' || *p == '\t'; p++);

			if (!Q_stricmpn(p, "\"path\"", 6)) {
				val = strchr(p + 6, '\"');
				if (val) {
					char *end;
					val++;
					end = strchr(val, '\"');
					if (end) {
						*end = '\0';
						Q_strncpyz(currentPath, val, sizeof(currentPath));
						hasPath = qtrue;
					}
				}
			}
			else if (hasPath && strstr(p, "\"282440\"")) {
				char qlDir[MAX_OSPATH];
				fclose(vdf);
				Com_sprintf(qlDir, sizeof(qlDir), "%s\\steamapps\\common\\Quake Live",
				            currentPath);
				{
					char *s;
					for (s = qlDir; *s; s++) {
						if (*s == '/') *s = '\\';
					}
				}
				Com_sprintf(testPath, sizeof(testPath), "%s\\baseq3\\pak00.pk3", qlDir);
				if (FS_FileExistsOS(testPath)) {
					Q_strncpyz(qlPath, qlDir, pathSize);
					return qtrue;
				}
				return qfalse;
			}
			else if (*p == '}') {
				hasPath = qfalse;
				currentPath[0] = '\0';
			}
		}
		fclose(vdf);
	}
	return qfalse;
}

static qboolean FS_Pak00SizeOk(const char *path) {
	WIN32_FILE_ATTRIBUTE_DATA info;

	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info))
		return qfalse;
	return (((ULONGLONG)info.nFileSizeHigh << 32) | info.nFileSizeLow) == QL_PAK00_SIZE ? qtrue : qfalse;
}

static qboolean FS_CopyFileWithProgress(const char *src, const char *dst,
                                         const char *displayName) {
	char from[MAX_OSPATH + 2];
	char part[MAX_OSPATH + 8];
	SHFILEOPSTRUCTA op;
	int result;

	Com_sprintf(part, sizeof(part), "%s.part", dst);
	DeleteFileA(part);

	Com_Memset(from, 0, sizeof(from));
	Q_strncpyz(from, src, sizeof(from) - 1);
	part[strlen(part) + 1] = '\0';

	Com_Memset(&op, 0, sizeof(op));
	op.wFunc = FO_COPY;
	op.pFrom = from;
	op.pTo = part;
	op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI;
	op.lpszProgressTitle = "ioquakelive";

	Com_Printf("Copying %s...\n", displayName);
	result = SHFileOperationA(&op);
	if (result != 0 || op.fAnyOperationsAborted || !FS_Pak00SizeOk(part)) {
		Com_Printf("Copying %s failed (%d)\n", displayName, result);
		DeleteFileA(part);
		return qfalse;
	}

	if (!MoveFileExA(part, dst, MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(part);
		return qfalse;
	}
	return qtrue;
}

static void FS_CopyFromSteam(void) {
	char qlPath[MAX_OSPATH];
	char pak00Dest[MAX_OSPATH];
	char pak00Src[MAX_OSPATH];
	qboolean needPak;
	char message[2048];

	Com_sprintf(pak00Dest, sizeof(pak00Dest), "%s\\%s\\pak00.pk3",
	            fs_basepath->string, BASEGAME_DIR);
	needPak = !FS_Pak00SizeOk(pak00Dest);

	if (!needPak)
		return;

	Com_Printf("\npak00.pk3 missing or incomplete in %s\\%s\\\n", fs_basepath->string, BASEGAME_DIR);
	Com_Printf("Searching for Quake Live Steam installation...\n");

	if (!FS_FindSteamQLPath(qlPath, sizeof(qlPath))) {
		Com_Printf("Quake Live Steam installation not found.\n");
		return;
	}

	Com_sprintf(pak00Src, sizeof(pak00Src), "%s\\baseq3\\pak00.pk3", qlPath);
	Com_Printf("Found Quake Live at: %s\n", qlPath);

	Com_sprintf(message, sizeof(message),
		"Quake Live is installed at:\n  %s\n\n"
		"The following files will be copied:\n\n"
		"  pak00.pk3 (~917 MB)\n"
		"    From: %s\\baseq3\\\n"
		"    To:   %s\\%s\\\n",
		qlPath, qlPath, fs_basepath->string, BASEGAME_DIR);

	Q_strcat(message, sizeof(message), "\nProceed with copy?");

	if (Sys_Dialog(DT_YES_NO, message, "ioquakelive - First Time Setup") != DR_YES) {
		Com_Printf("Copy cancelled by user.\n");
		return;
	}

	{
		char dir[MAX_OSPATH];
		Com_sprintf(dir, sizeof(dir), "%s\\%s", fs_basepath->string, BASEGAME_DIR);
		Sys_Mkdir(dir);
	}

	if (!FS_CopyFileWithProgress(pak00Src, pak00Dest, "pak00.pk3")) {
		Com_Printf("ERROR: Failed to copy pak00.pk3\n");
		Sys_Dialog(DT_ERROR,
			"Failed to copy pak00.pk3.\n\n"
			"Check that the source file is readable and the\n"
			"destination has enough free disk space (~917 MB).",
			"Copy Failed");
		return;
	}

	Com_Printf("pak00.pk3 copied successfully.\n");

	Com_Printf("\nQuake Live data setup complete.\n\n");
}
#endif

static void FS_AddWorkshopDir(const char* folder, qboolean skipPaks, uint64_t item) {
	char savedGamedir[MAX_OSPATH];
	char curpath[MAX_OSPATH + 1];
	searchpath_t* before = fs_searchpaths;
	searchpath_t* sp;

	if (!folder[0]) {
		return;
	}
	if (skipPaks) {
		for (sp = fs_searchpaths; sp; sp = sp->next) {
			if (sp->dir && !Q_stricmp(sp->dir->path, folder) && !sp->dir->gamedir[0]) {
				return;
			}
		}
		Q_strncpyz(curpath, FS_BuildOSPath(folder, "", ""), sizeof(curpath));
		curpath[strlen(curpath) - 1] = '\0';
		sp = Z_Malloc(sizeof(searchpath_t));
		sp->dir = Z_Malloc(sizeof(*sp->dir));
		Q_strncpyz(sp->dir->path, folder, sizeof(sp->dir->path));
		Q_strncpyz(sp->dir->fullpath, curpath, sizeof(sp->dir->fullpath));
		sp->next = fs_searchpaths;
		fs_searchpaths = sp;
		return;
	}
	Q_strncpyz(savedGamedir, fs_gamedir, sizeof(savedGamedir));
	FS_AddGameDirectory(folder, "");
	Q_strncpyz(fs_gamedir, savedGamedir, sizeof(fs_gamedir));
	for (sp = fs_searchpaths; sp && sp != before; sp = sp->next) {
		if (sp->pack) {
			sp->pack->workshopId = item;
		}
	}
}

static qboolean FS_HavePak00(void) {
	FILE* f = Sys_FOpen(FS_BuildOSPath(fs_basepath->string, BASEGAME_DIR, "pak00.pk3"), "rb");

	if (!f && fs_steampath && fs_steampath->string[0]) {
		f = Sys_FOpen(FS_BuildOSPath(fs_steampath->string, BASEGAME_DIR, "pak00.pk3"), "rb");
	}
	if (!f) {
		return qfalse;
	}
	fclose(f);
	return qtrue;
}

static void FS_AddWorkshop(void) {
	uint64_t items[256];
	char folder[MAX_OSPATH];
	const char* path;
	uint64_t item;
	qboolean skipPaks;
	int count, i;

	skipPaks = (qboolean)!FS_HavePak00();
	if (skipPaks) {
		Com_Printf("WARNING: Skipping workshop PK3s since pak00 doesn't exist.\n");
	}
	if (Cvar_VariableIntegerValue("fs_skipWorkshop")) {
		Com_Printf("Skipping workshop since fs_skipWorkshop is set.\n");
		return;
	}
	if (Cvar_VariableIntegerValue("com_build")) {
		Com_Printf("Skipping workshop since running in build mode.\n");
		return;
	}
	if (!Steam_WorkshopUGC()) {
		Com_Printf("WARNING: Skipping workshop, ISteamUGC is NULL.\n");
		return;
	}
	count = Steam_WorkshopSubscribed(items, ARRAY_LEN(items));
	for (i = 0; i < count; i++) {
		if (Steam_WorkshopPath(items[i], folder, sizeof(folder))) {
			FS_AddWorkshopDir(folder, skipPaks, items[i]);
		}
	}
	for (i = 0; (path = Steam_WorkshopFolder(i, &item)) != NULL; i++) {
		FS_AddWorkshopDir(path, skipPaks, item);
	}
}

void FS_WorkshopRestart(void) {
	if ((com_sv_running && com_sv_running->integer) || CL_Connected()) {
		return;
	}
	FS_Restart(fs_checksumFeed);
}

typedef struct {
	unsigned long offset;
	unsigned long size;
} fsWarmRange_t;

typedef struct {
	char pak[MAX_OSPATH];
	int count;
	fsWarmRange_t ranges[1];
} fsWarm_t;

static int FS_WarmCompare(const void* a, const void* b) {
	unsigned long x = ((const fsWarmRange_t*)a)->offset;
	unsigned long y = ((const fsWarmRange_t*)b)->offset;

	return x < y ? -1 : x > y;
}

static void FS_WarmJob(void* data) {
	fsWarm_t* w = data;
	char discard[32768];
	unsigned long left;
	size_t got;
	FILE* f;
	int i;

	f = Sys_FOpen(w->pak, "rb");
	if (f) {
		for (i = 0; i < w->count; i++) {
			if (fseek(f, (long)w->ranges[i].offset, SEEK_SET) != 0) {
				continue;
			}
			for (left = w->ranges[i].size + 512; left > 0; left -= (unsigned long)got) {
				got = fread(discard, 1, left < sizeof(discard) ? left : sizeof(discard), f);
				if (!got) {
					break;
				}
			}
		}
		fclose(f);
	}
	free(w);
}

void FS_WarmPrefix(const char* prefix) {
	searchpath_t* search;
	int seen[64];
	int numSeen = 0;
	int len = (int)strlen(prefix);
	int i, j, n;

	if (!fs_searchpaths || !Com_JobWorkers()) {
		return;
	}
	for (search = fs_searchpaths; search; search = search->next) {
		pack_t* pak = search->pack;
		fsWarm_t* w;

		if (!pak) {
			continue;
		}
		for (j = 0; j < numSeen && seen[j] != pak->checksum; j++) {
		}
		if (j < numSeen) {
			continue;
		}
		if (numSeen < ARRAY_LEN(seen)) {
			seen[numSeen++] = pak->checksum;
		}
		for (i = n = 0; i < pak->numfiles; i++) {
			if (pak->buildBuffer[i].offset && !Q_stricmpn(pak->buildBuffer[i].name, prefix, len)) {
				n++;
			}
		}
		if (!n) {
			continue;
		}
		w = malloc(sizeof(*w) + (n - 1) * sizeof(w->ranges[0]));
		if (!w) {
			return;
		}
		Q_strncpyz(w->pak, pak->pakFilename, sizeof(w->pak));
		for (i = w->count = 0; i < pak->numfiles; i++) {
			if (pak->buildBuffer[i].offset && !Q_stricmpn(pak->buildBuffer[i].name, prefix, len)) {
				w->ranges[w->count].offset = pak->buildBuffer[i].offset;
				w->ranges[w->count].size = pak->buildBuffer[i].csize;
				w->count++;
			}
		}
		qsort(w->ranges, w->count, sizeof(w->ranges[0]), FS_WarmCompare);
		Com_DPrintf("FS_WarmPrefix: %s, %i files from %s\n", prefix, w->count, pak->pakBasename);
		Com_JobsSubmit(FS_WarmJob, w);
	}
}

const char* FS_LoadedPakChecksums(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;

	info[0] = 0;

	for (search = fs_searchpaths; search; search = search->next) {
		if (!search->pack || FS_IsEnginePak(search->pack)) {
			continue;
		}

		Q_strcat(info, sizeof(info), va("%i ", search->pack->checksum));
	}

	return info;
}

const char* FS_LoadedPakNames(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;

	info[0] = 0;

	for (search = fs_searchpaths; search; search = search->next) {
		if (!search->pack || FS_IsEnginePak(search->pack)) {
			continue;
		}

		if (*info) {
			Q_strcat(info, sizeof(info), " ");
		}
		Q_strcat(info, sizeof(info), search->pack->pakBasename);
	}

	return info;
}

const char* FS_LoadedPakPureChecksums(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;

	info[0] = 0;

	for (search = fs_searchpaths; search; search = search->next) {
		if (!search->pack || FS_IsEnginePak(search->pack)) {
			continue;
		}

		Q_strcat(info, sizeof(info), va("%i ", search->pack->pure_checksum));
	}

	return info;
}

const char* FS_ReferencedPakChecksums(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;

	info[0] = 0;

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack && search->pack->referenced && !FS_IsEnginePak(search->pack)) {
			Q_strcat(info, sizeof(info), va("%i ", search->pack->checksum));
		}
	}

	return info;
}

static int FS_ModulePakPureChecksum(const pack_t* pack) {
	searchpath_t* search;

	if (!FS_IsGamecodePak(pack->pakBasename)) {
		return pack->pure_checksum;
	}
	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack && !Q_stricmp(search->pack->pakBasename, "bin")) {
			return search->pack->pure_checksum;
		}
	}
	return pack->pure_checksum;
}

const char* FS_ReferencedPakPureChecksums(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;
	int numPaks, checksum;

	info[0] = 0;

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack && (search->pack->referenced & (FS_QAGAME_REF | FS_CGAME_REF | FS_UI_REF))) {
			Q_strcat(info, sizeof(info), va("%i ", FS_ModulePakPureChecksum(search->pack)));
			break;
		}
	}

	Q_strcat(info, sizeof(info), "@ ");

	checksum = fs_checksumFeed;
	numPaks = 0;
	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack && (search->pack->referenced & FS_GENERAL_REF) && !FS_IsEnginePak(search->pack)) {
			Q_strcat(info, sizeof(info), va("%i ", search->pack->pure_checksum));
			checksum ^= search->pack->pure_checksum;
			numPaks++;
		}
	}
	checksum ^= numPaks;
	Q_strcat(info, sizeof(info), va("%i ", checksum));

	return info;
}

const char* FS_ReferencedPakNames(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;

	info[0] = 0;

	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack && search->pack->referenced && !FS_IsEnginePak(search->pack)) {
			if (*info) {
				Q_strcat(info, sizeof(info), " ");
			}
			Q_strcat(info, sizeof(info), search->pack->pakGamename);
			Q_strcat(info, sizeof(info), "/");
			Q_strcat(info, sizeof(info), search->pack->pakBasename);
		}
	}

	return info;
}

const char* FS_ReferencedSteamworksItems(void) {
	static char info[BIG_INFO_STRING];
	searchpath_t* search;

	info[0] = 0;
	for (search = fs_searchpaths; search; search = search->next) {
		char id[32];

		if (!search->pack || !search->pack->referenced || !search->pack->workshopId) {
			continue;
		}
		Com_sprintf(id, sizeof(id), "%llu", search->pack->workshopId);

		if (strstr(info, id)) {
			continue;
		}
		Q_strcat(info, sizeof(info), va("%s ", id));
	}
	return info;
}

void FS_ClearPakReferences(int flags) {
	searchpath_t* search;

	if (!flags) {
		flags = -1;
	}
	for (search = fs_searchpaths; search; search = search->next) {
		if (search->pack) {
			search->pack->referenced &= ~flags;
		}
	}
}

void FS_PureServerSetLoadedPaks(const char* pakSums, const char* pakNames) {
	int i, c, d;

	Cmd_TokenizeString(pakSums);

	c = Cmd_Argc();
	if (c > MAX_SEARCH_PATHS) {
		c = MAX_SEARCH_PATHS;
	}

	fs_numServerPaks = c;

	for (i = 0; i < c; i++) {
		fs_serverPaks[i] = atoi(Cmd_Argv(i));
	}

	if (fs_numServerPaks) {
		Com_DPrintf("Connected to a pure server.\n");
	} else {
		if (fs_reordered) {
			Com_DPrintf("FS search reorder is required\n");
			FS_Restart(fs_checksumFeed);
			return;
		}
	}

	for (i = 0; i < c; i++) {
		if (fs_serverPakNames[i]) {
			Z_Free(fs_serverPakNames[i]);
		}
		fs_serverPakNames[i] = NULL;
	}
	if (pakNames && *pakNames) {
		Cmd_TokenizeString(pakNames);

		d = Cmd_Argc();
		if (d > MAX_SEARCH_PATHS) {
			d = MAX_SEARCH_PATHS;
		}

		for (i = 0; i < d; i++) {
			fs_serverPakNames[i] = CopyString(Cmd_Argv(i));
		}
	}
}

void FS_PureServerSetReferencedPaks(const char* pakSums, const char* pakNames) {
	int i, c, d = 0;

	Cmd_TokenizeString(pakSums);

	c = Cmd_Argc();
	if (c > MAX_SEARCH_PATHS) {
		c = MAX_SEARCH_PATHS;
	}

	for (i = 0; i < c; i++) {
		fs_serverReferencedPaks[i] = atoi(Cmd_Argv(i));
	}

	for (i = 0; i < ARRAY_LEN(fs_serverReferencedPakNames); i++) {
		if (fs_serverReferencedPakNames[i])
			Z_Free(fs_serverReferencedPakNames[i]);

		fs_serverReferencedPakNames[i] = NULL;
	}

	if (pakNames && *pakNames) {
		Cmd_TokenizeString(pakNames);

		d = Cmd_Argc();

		if (d > c)
			d = c;

		for (i = 0; i < d; i++) {
			fs_serverReferencedPakNames[i] = CopyString(Cmd_Argv(i));
		}
	}

	if (d < c)
		c = d;

	fs_numServerReferencedPaks = c;
}

qboolean FS_VerifyPureGamecode(void) {
	if (fs_numServerPaks == 0) {
		return qtrue;
	}

	if (fs_uiBinChecksum == 0) {
		Com_DPrintf("no pak for ui module\n");
		return qfalse;
	}

	if (fs_cgameBinChecksum == 0) {
		Com_DPrintf("no pak for cgame module\n");
		return qfalse;
	}

	if (fs_qagameBinChecksum == 0) {
		Com_DPrintf("no pak for qagame module\n");
		return qfalse;
	}

	if (fs_uiBinChecksum != fs_cgameBinChecksum ||
	    fs_cgameBinChecksum != fs_qagameBinChecksum) {
		Com_DPrintf("different paks for ui/cgame/qagame modules\n");
		return qfalse;
	}

	for (searchpath_t* search = fs_searchpaths; search != NULL; search = search->next) {
		pack_t* pak = search->pack;

		if (pak && (pak->referenced & FS_UI_REF) && fs_numServerPaks > 0) {
			int checksum = pak->checksum;

			if (FS_IsEnginePak(pak) && fs_uiBinChecksum == checksum) {
				return qtrue;
			}
			if (checksum != fs_serverPaks[0]) {
				qboolean found = qfalse;
				for (int i = 0; i < fs_numServerPaks; i++) {
					if (fs_serverPaks[i] == checksum) {
						found = qtrue;
						break;
					}
				}

				if (!found) {
					continue;
				}
			}

			if (fs_uiBinChecksum == checksum) {
				return qtrue;
			}
		}
	}

	Com_DPrintf("no pak found matching the game modules we have\n");
	return qfalse;
}

qboolean FS_ExtractGamecode(const char* module, char* outOSPath) {
	char filename[MAX_QPATH];
	searchpath_t* search;
	pack_t* pak;
	fileInPack_t* fileEntry;
	int handleIn, handleOut;
	int checksum;
	int hash;
	void* buffer;
	int fileLen;

	Com_sprintf(filename, sizeof(filename), "%s" ARCH_STRING DLL_EXT, module);

	for (search = fs_searchpaths; search != NULL; search = search->next) {
		pak = search->pack;

		if (!pak)
			continue;

		checksum = pak->checksum;
		if (fs_numServerPaks > 0 && !FS_IsEnginePak(pak)) {
			qboolean matched = qfalse;

			for (int i = 0; i < fs_numServerPaks; i++) {
				if (fs_serverPaks[i] == checksum) {
					matched = qtrue;
					break;
				}
			}
			if (!matched)
				continue;
		}

		int refFlag = 0;
		if (Q_stricmp(module, "ui") == 0) {
			refFlag = FS_UI_REF;
		} else if (Q_stricmp(module, "cgame") == 0) {
			refFlag = FS_CGAME_REF;
		} else if (Q_stricmp(module, "qagame") == 0) {
			refFlag = FS_QAGAME_REF;
		} else {
			Com_Error(ERR_FATAL, "FS_ExtractGamecode: invalid module name '%s'.", module);
		}

		if (FS_IsGamecodePak(pak->pakBasename)) {
			pak->referenced |= FS_UI_REF | FS_CGAME_REF | FS_QAGAME_REF;
		}

		if (!(pak->referenced & refFlag)) {
			continue;
		}

		hash = FS_HashFileName(filename, pak->hashSize);
		for (fileEntry = pak->hashTable[hash]; fileEntry; fileEntry = fileEntry->next) {
			if (FS_FilenameCompare(fileEntry->name, filename) != 0)
				continue;

			handleIn = FS_HandleForFile();
			fileHandleData_t* fhdIn = &fsh[handleIn];
			memset(fhdIn, 0, sizeof(fileHandleData_t));
			fhdIn->zipFile = qtrue;
			fhdIn->handleFiles.file.z = pak->handle;
			Q_strncpyz(fhdIn->name, filename, sizeof(fhdIn->name));

			unzSetCurrentFileInfoPosition(pak->handle, fileEntry->pos);
			unzOpenCurrentFile(pak->handle);
			fhdIn->zipFilePos = fileEntry->pos;

			if (FS_FileExists(filename)) {
				char* path = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);
				remove(path);
			}

			handleOut = FS_FOpenFileWrite(filename);
			char* path = FS_BuildOSPath(fs_homepath->string, fs_gamedir, filename);
			Q_strncpyz(outOSPath, path, MAX_OSPATH);

			if (!handleOut) {
				Com_Error(ERR_FATAL, "FS_ExtractGamecode: failed to open %s for writing", filename);
			}

			fileLen = fileEntry->len;
			buffer = Z_Malloc(fileLen);

			if (FS_Read(buffer, fileLen, handleIn) != fileLen) {
				Com_Error(ERR_FATAL, "FS_ExtractGamecode: short read");
			}
			if (FS_Write(buffer, fileLen, handleOut) != fileLen) {
				Com_Error(ERR_FATAL, "FS_ExtractGamecode: short write");
			}

			Com_DPrintf("Extracted '%s' (%d bytes) from '%s/%s.pk3' to '%s'\n", filename, fileLen, pak->pakGamename, pak->pakBasename, path);

			Z_Free(buffer);
			FS_FCloseFile(handleIn);
			FS_FCloseFile(handleOut);

			if (Q_stricmp(module, "ui") == 0) {
				fs_uiBinChecksum = checksum;
			} else if (Q_stricmp(module, "cgame") == 0) {
				fs_cgameBinChecksum = checksum;
			} else if (Q_stricmp(module, "qagame") == 0) {
				fs_qagameBinChecksum = checksum;
			}

			return qtrue;
		}
	}

	return qfalse;
}

void FS_InitFilesystem(void) {
	Com_StartupVariable("fs_basepath");
	Com_StartupVariable("fs_homepath");
	Com_StartupVariable("fs_game");

	if (!FS_FilenameCompare(Cvar_VariableString("fs_game"), com_basegame->string))
		Cvar_Set("fs_game", "");

	fs_uiBinChecksum = 0;
	fs_cgameBinChecksum = 0;

	FS_Startup(com_basegame->string);

#ifndef DEDICATED
#ifdef _WIN32
	{
		char pak00Path[MAX_OSPATH];
		Com_sprintf(pak00Path, sizeof(pak00Path), "%s%c%s%cpak00.pk3",
		            fs_basepath->string, PATH_SEP, BASEGAME_DIR, PATH_SEP);
		if (!FS_Pak00SizeOk(pak00Path)) {
			FS_CopyFromSteam();
			if (FS_Pak00SizeOk(pak00Path)) {
				FS_Shutdown(qfalse);
				FS_Startup(com_basegame->string);
			}
		}
	}

#elif defined(__APPLE__)
	{
		char pak00Path[MAX_OSPATH];
		qboolean found = qfalse;

		Com_sprintf(pak00Path, sizeof(pak00Path), "%s%c%s%cpak00.pk3",
		            fs_homepath->string, PATH_SEP, BASEGAME_DIR, PATH_SEP);
		if (FS_FileInPathExists(pak00Path))
			found = qtrue;

		if (!found) {
			Com_sprintf(pak00Path, sizeof(pak00Path), "%s%c%s%cpak00.pk3",
			            fs_basepath->string, PATH_SEP, BASEGAME_DIR, PATH_SEP);
			if (FS_FileInPathExists(pak00Path))
				found = qtrue;
		}

		if (!found) {
			if (Sys_LocatePak0(fs_homepath->string)) {
				FS_Shutdown(qfalse);
				FS_Startup(com_basegame->string);
			}
		}
	}

#endif
#else
	{
		char pak00Base[MAX_OSPATH];
		char pak00Home[MAX_OSPATH];

		Com_sprintf(pak00Base, sizeof(pak00Base), "%s%c%s%cpak00.pk3",
		            fs_basepath->string, PATH_SEP, BASEGAME_DIR, PATH_SEP);
		Com_sprintf(pak00Home, sizeof(pak00Home), "%s%c%s%cpak00.pk3",
		            fs_homepath->string, PATH_SEP, BASEGAME_DIR, PATH_SEP);

		if (!FS_FileInPathExists(pak00Base) && !FS_FileInPathExists(pak00Home)) {
			Com_Printf(
			    "\n"
			    "********************************************************\n"
			    "WARNING: pak00.pk3 not found.\n"
			    "\n"
			    "The dedicated server requires pak00.pk3 to load game data.\n"
			    "Place the file in one of the following locations and\n"
			    "restart the server:\n"
			    "\n"
			    "  %s\n"
			    "  %s\n"
			    "\n"
			    "The server will fail to load default.cfg without it.\n"
			    "********************************************************\n"
			    "\n",
			    pak00Base, pak00Home);
		}
	}
#endif

	if (FS_ReadFile("default.cfg", NULL) <= 0) {
		Com_Error(ERR_FATAL, "Couldn't load default.cfg");
	}

	Q_strncpyz(lastValidBase, fs_basepath->string, sizeof(lastValidBase));
	Q_strncpyz(lastValidComBaseGame, com_basegame->string, sizeof(lastValidComBaseGame));
	Q_strncpyz(lastValidFsBaseGame, fs_basegame->string, sizeof(lastValidFsBaseGame));
	Q_strncpyz(lastValidGame, fs_gamedirvar->string, sizeof(lastValidGame));
}

void FS_Restart(int checksumFeed) {
	const char* lastGameDir;

	FS_Shutdown(qfalse);

	fs_checksumFeed = checksumFeed;

	FS_ClearPakReferences(0);

	FS_Startup(com_basegame->string);

	if (FS_ReadFile("default.cfg", NULL) <= 0) {
		if (lastValidBase[0]) {
			FS_PureServerSetLoadedPaks("", "");
			Cvar_Set("fs_basepath", lastValidBase);
			Cvar_Set("com_basegame", lastValidComBaseGame);
			Cvar_Set("fs_basegame", lastValidFsBaseGame);
			Cvar_Set("fs_game", lastValidGame);
			lastValidBase[0] = '\0';
			lastValidComBaseGame[0] = '\0';
			lastValidFsBaseGame[0] = '\0';
			lastValidGame[0] = '\0';
			FS_Restart(checksumFeed);
			Com_Error(ERR_DROP, "Invalid game folder");
			return;
		}
		Com_Error(ERR_FATAL, "Couldn't load default.cfg");
	}

	lastGameDir = (lastValidGame[0]) ? lastValidGame : lastValidComBaseGame;

	if (Q_stricmp(FS_GetCurrentGameDir(), lastGameDir)) {
#ifdef DEDICATED
		Sys_RemovePIDFile(lastGameDir);
		Sys_WritePIDFile(FS_GetCurrentGameDir());
#endif

		if (!Com_SafeMode()) {
			Cbuf_AddText("exec " QZCONFIG_CFG "\n");
		}
	}

	Q_strncpyz(lastValidBase, fs_basepath->string, sizeof(lastValidBase));
	Q_strncpyz(lastValidComBaseGame, com_basegame->string, sizeof(lastValidComBaseGame));
	Q_strncpyz(lastValidFsBaseGame, fs_basegame->string, sizeof(lastValidFsBaseGame));
	Q_strncpyz(lastValidGame, fs_gamedirvar->string, sizeof(lastValidGame));
}

qboolean FS_ConditionalRestart(int checksumFeed, qboolean disconnect) {
	if (fs_gamedirvar->modified) {
		if (FS_FilenameCompare(lastValidGame, fs_gamedirvar->string) &&
			(*lastValidGame || FS_FilenameCompare(fs_gamedirvar->string, com_basegame->string)) &&
			(*fs_gamedirvar->string || FS_FilenameCompare(lastValidGame, com_basegame->string))) {
			Com_GameRestart(checksumFeed, disconnect);
			return qtrue;
		} else
			fs_gamedirvar->modified = qfalse;
	}

	if (checksumFeed != fs_checksumFeed)
		FS_Restart(checksumFeed);
	else if (fs_numServerPaks && !fs_reordered)
		FS_ReorderPurePaks();

	return qfalse;
}

int FS_FOpenFileByMode(const char* qpath, fileHandle_t* f, fsMode_t mode) {
	int r;
	qboolean sync;

	sync = qfalse;

	switch (mode) {
	case FS_READ:
		r = FS_FOpenFileRead(qpath, f, qtrue);
		break;
	case FS_WRITE:
		*f = FS_FOpenFileWrite(qpath);
		r = 0;
		if (*f == 0) {
			r = -1;
		}
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
		*f = FS_FOpenFileAppend(qpath);
		r = 0;
		if (*f == 0) {
			r = -1;
		}
		break;
	default:
		Com_Error(ERR_FATAL, "FS_FOpenFileByMode: bad mode");
		return -1;
	}

	if (!f) {
		return r;
	}

	if (*f) {
		fsh[*f].fileSize = r;
	}
	fsh[*f].handleSync = sync;

	return r;
}

int FS_FTell(fileHandle_t f) {
	int pos;
	if (fsh[f].zipFile == qtrue) {
		pos = unztell(fsh[f].handleFiles.file.z);
	} else {
		pos = ftell(fsh[f].handleFiles.file.o);
	}
	return pos;
}

void FS_Flush(fileHandle_t f) {
	fflush(fsh[f].handleFiles.file.o);
}

void FS_FilenameCompletion(const char* dir, const char* ext, qboolean stripExt, void (*callback)(const char* s), qboolean allowNonPureFilesOnDisk) {
	char** filenames;
	int nfiles;
	int i;
	char filename[MAX_STRING_CHARS];

	filenames = FS_ListFilteredFiles(dir, ext, NULL, &nfiles, allowNonPureFilesOnDisk);

	FS_SortFileList(filenames, nfiles);

	for (i = 0; i < nfiles; i++) {
		FS_ConvertPath(filenames[i]);
		Q_strncpyz(filename, filenames[i], MAX_STRING_CHARS);

		if (stripExt) {
			COM_StripExtension(filename, filename, sizeof(filename));
		}

		callback(filename);
	}
	FS_FreeFileList(filenames);
}

const char* FS_GetCurrentGameDir(void) {
	if (fs_gamedirvar->string[0])
		return fs_gamedirvar->string;

	return com_basegame->string;
}
