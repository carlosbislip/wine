/*
 * Unit tests for shell32 SHGet{Special}Folder{Path|Location} functions.
 *
 * Copyright 2004 Juan Lang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 * This is a test program for the SHGet{Special}Folder{Path|Location} functions
 * of shell32, that get either a filesystem path or a LPITEMIDLIST (shell
 * namespace) path for a given folder (CSIDL value).
 */

#define COBJMACROS

#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "shlguid.h"
#include "shlobj.h"
#include "shlwapi.h"
#include "initguid.h"
#include "knownfolders.h"
#include "wine/test.h"

/* CSIDL_MYDOCUMENTS is now the same as CSIDL_PERSONAL, but what we want
 * here is its original value.
 */
#define OLD_CSIDL_MYDOCUMENTS  0x000c

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) ( sizeof(x) / sizeof((x)[0]) )
#endif

/* from pidl.h, not included here: */
#ifndef PT_CPL             /* Guess, Win7 uses this for CSIDL_CONTROLS */
#define PT_CPL        0x01 /* no path */
#endif
#ifndef PT_GUID
#define PT_GUID       0x1f /* no path */
#endif
#ifndef PT_DRIVE 
#define PT_DRIVE      0x23 /* has path */
#endif
#ifndef PT_DRIVE2
#define PT_DRIVE2     0x25 /* has path */
#endif
#ifndef PT_SHELLEXT
#define PT_SHELLEXT   0x2e /* no path */
#endif
#ifndef PT_FOLDER
#define PT_FOLDER     0x31 /* has path */
#endif
#ifndef PT_FOLDERW
#define PT_FOLDERW    0x35 /* has path */
#endif
#ifndef PT_WORKGRP
#define PT_WORKGRP    0x41 /* no path */
#endif
#ifndef PT_YAGUID
#define PT_YAGUID     0x70 /* no path */
#endif
/* FIXME: this is used for history/favorites folders; what's a better name? */
#ifndef PT_IESPECIAL2
#define PT_IESPECIAL2 0xb1 /* has path */
#endif

static GUID CLSID_CommonDocuments = { 0x0000000c, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x1a } };

struct shellExpectedValues {
    int folder;
    int numTypes;
    const BYTE *types;
};

static HRESULT (WINAPI *pDllGetVersion)(DLLVERSIONINFO *);
static HRESULT (WINAPI *pSHGetFolderPathA)(HWND, int, HANDLE, DWORD, LPSTR);
static HRESULT (WINAPI *pSHGetFolderLocation)(HWND, int, HANDLE, DWORD,
 LPITEMIDLIST *);
static BOOL    (WINAPI *pSHGetSpecialFolderPathA)(HWND, LPSTR, int, BOOL);
static HRESULT (WINAPI *pSHGetSpecialFolderLocation)(HWND, int, LPITEMIDLIST *);
static LPITEMIDLIST (WINAPI *pILFindLastID)(LPCITEMIDLIST);
static int (WINAPI *pSHFileOperationA)(LPSHFILEOPSTRUCTA);
static HRESULT (WINAPI *pSHGetMalloc)(LPMALLOC *);
static UINT (WINAPI *pGetSystemWow64DirectoryA)(LPSTR,UINT);
static HRESULT (WINAPI *pSHGetKnownFolderPath)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR *);
static HRESULT (WINAPI *pSHSetKnownFolderPath)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR);
static HRESULT (WINAPI *pSHGetFolderPathEx)(REFKNOWNFOLDERID, DWORD, HANDLE, LPWSTR, DWORD);

static DLLVERSIONINFO shellVersion = { 0 };
static LPMALLOC pMalloc;
static const BYTE guidType[] = { PT_GUID };
static const BYTE controlPanelType[] = { PT_SHELLEXT, PT_GUID, PT_CPL };
static const BYTE folderType[] = { PT_FOLDER, PT_FOLDERW };
static const BYTE favoritesType[] = { PT_FOLDER, PT_FOLDERW, 0, PT_IESPECIAL2 /* Win98 */ };
static const BYTE folderOrSpecialType[] = { PT_FOLDER, PT_IESPECIAL2 };
static const BYTE personalType[] = { PT_FOLDER, PT_GUID, PT_DRIVE, 0xff /* Win9x */,
 PT_IESPECIAL2 /* Win98 */, 0 /* Vista */ };
/* FIXME: don't know the type of 0x71 returned by Vista/2008 for printers */
static const BYTE printersType[] = { PT_YAGUID, PT_SHELLEXT, 0x71 };
static const BYTE ieSpecialType[] = { PT_IESPECIAL2 };
static const BYTE shellExtType[] = { PT_SHELLEXT };
static const BYTE workgroupType[] = { PT_WORKGRP };
#define DECLARE_TYPE(x, y) { x, sizeof(y) / sizeof(y[0]), y }
static const struct shellExpectedValues requiredShellValues[] = {
 DECLARE_TYPE(CSIDL_BITBUCKET, guidType),
 DECLARE_TYPE(CSIDL_CONTROLS, controlPanelType),
 DECLARE_TYPE(CSIDL_COOKIES, folderType),
 DECLARE_TYPE(CSIDL_DESKTOPDIRECTORY, folderType),
 DECLARE_TYPE(CSIDL_DRIVES, guidType),
 DECLARE_TYPE(CSIDL_FAVORITES, favoritesType),
 DECLARE_TYPE(CSIDL_FONTS, folderOrSpecialType),
/* FIXME: the following fails in Wine, returns type PT_FOLDER
 DECLARE_TYPE(CSIDL_HISTORY, ieSpecialType),
 */
 DECLARE_TYPE(CSIDL_INTERNET, guidType),
 DECLARE_TYPE(CSIDL_NETHOOD, folderType),
 DECLARE_TYPE(CSIDL_NETWORK, guidType),
 DECLARE_TYPE(CSIDL_PERSONAL, personalType),
 DECLARE_TYPE(CSIDL_PRINTERS, printersType),
 DECLARE_TYPE(CSIDL_PRINTHOOD, folderType),
 DECLARE_TYPE(CSIDL_PROGRAMS, folderType),
 DECLARE_TYPE(CSIDL_RECENT, folderOrSpecialType),
 DECLARE_TYPE(CSIDL_SENDTO, folderType),
 DECLARE_TYPE(CSIDL_STARTMENU, folderType),
 DECLARE_TYPE(CSIDL_STARTUP, folderType),
 DECLARE_TYPE(CSIDL_TEMPLATES, folderType),
};
static const struct shellExpectedValues optionalShellValues[] = {
/* FIXME: the following only semi-succeed; they return NULL PIDLs on XP.. hmm.
 DECLARE_TYPE(CSIDL_ALTSTARTUP, folderType),
 DECLARE_TYPE(CSIDL_COMMON_ALTSTARTUP, folderType),
 DECLARE_TYPE(CSIDL_COMMON_OEM_LINKS, folderType),
 */
/* Windows NT-only: */
 DECLARE_TYPE(CSIDL_COMMON_DESKTOPDIRECTORY, folderType),
 DECLARE_TYPE(CSIDL_COMMON_DOCUMENTS, shellExtType),
 DECLARE_TYPE(CSIDL_COMMON_FAVORITES, folderType),
 DECLARE_TYPE(CSIDL_COMMON_PROGRAMS, folderType),
 DECLARE_TYPE(CSIDL_COMMON_STARTMENU, folderType),
 DECLARE_TYPE(CSIDL_COMMON_STARTUP, folderType),
 DECLARE_TYPE(CSIDL_COMMON_TEMPLATES, folderType),
/* first appearing in shell32 version 4.71: */
 DECLARE_TYPE(CSIDL_APPDATA, folderType),
/* first appearing in shell32 version 4.72: */
 DECLARE_TYPE(CSIDL_INTERNET_CACHE, ieSpecialType),
/* first appearing in shell32 version 5.0: */
 DECLARE_TYPE(CSIDL_ADMINTOOLS, folderType),
 DECLARE_TYPE(CSIDL_COMMON_APPDATA, folderType),
 DECLARE_TYPE(CSIDL_LOCAL_APPDATA, folderType),
 DECLARE_TYPE(OLD_CSIDL_MYDOCUMENTS, folderType),
 DECLARE_TYPE(CSIDL_MYMUSIC, folderType),
 DECLARE_TYPE(CSIDL_MYPICTURES, folderType),
 DECLARE_TYPE(CSIDL_MYVIDEO, folderType),
 DECLARE_TYPE(CSIDL_PROFILE, folderType),
 DECLARE_TYPE(CSIDL_PROGRAM_FILES, folderType),
 DECLARE_TYPE(CSIDL_PROGRAM_FILESX86, folderType),
 DECLARE_TYPE(CSIDL_PROGRAM_FILES_COMMON, folderType),
 DECLARE_TYPE(CSIDL_PROGRAM_FILES_COMMONX86, folderType),
 DECLARE_TYPE(CSIDL_SYSTEM, folderType),
 DECLARE_TYPE(CSIDL_WINDOWS, folderType),
/* first appearing in shell32 6.0: */
 DECLARE_TYPE(CSIDL_CDBURN_AREA, folderType),
 DECLARE_TYPE(CSIDL_COMMON_MUSIC, folderType),
 DECLARE_TYPE(CSIDL_COMMON_PICTURES, folderType),
 DECLARE_TYPE(CSIDL_COMMON_VIDEO, folderType),
 DECLARE_TYPE(CSIDL_COMPUTERSNEARME, workgroupType),
 DECLARE_TYPE(CSIDL_RESOURCES, folderType),
 DECLARE_TYPE(CSIDL_RESOURCES_LOCALIZED, folderType),
};
#undef DECLARE_TYPE

static void loadShell32(void)
{
    HMODULE hShell32 = GetModuleHandleA("shell32");

#define GET_PROC(func) \
    p ## func = (void*)GetProcAddress(hShell32, #func); \
    if(!p ## func) \
      trace("GetProcAddress(%s) failed\n", #func);

    GET_PROC(DllGetVersion)
    GET_PROC(SHGetFolderPathA)
    GET_PROC(SHGetFolderPathEx)
    GET_PROC(SHGetFolderLocation)
    GET_PROC(SHGetKnownFolderPath)
    GET_PROC(SHSetKnownFolderPath)
    GET_PROC(SHGetSpecialFolderPathA)
    GET_PROC(SHGetSpecialFolderLocation)
    GET_PROC(ILFindLastID)
    if (!pILFindLastID)
        pILFindLastID = (void *)GetProcAddress(hShell32, (LPCSTR)16);
    GET_PROC(SHFileOperationA)
    GET_PROC(SHGetMalloc)

    ok(pSHGetMalloc != NULL, "shell32 is missing SHGetMalloc\n");
    if (pSHGetMalloc)
    {
        HRESULT hr = pSHGetMalloc(&pMalloc);

        ok(hr == S_OK, "SHGetMalloc failed: 0x%08x\n", hr);
        ok(pMalloc != NULL, "SHGetMalloc returned a NULL IMalloc\n");
    }

    if (pDllGetVersion)
    {
        shellVersion.cbSize = sizeof(shellVersion);
        pDllGetVersion(&shellVersion);
        trace("shell32 version is %d.%d\n",
              shellVersion.dwMajorVersion, shellVersion.dwMinorVersion);
    }
#undef GET_PROC
}

#ifndef CSIDL_PROFILES
#define CSIDL_PROFILES		0x003e
#endif

/* A couple utility printing functions */
static const char *getFolderName(int folder)
{
    static char unknown[32];

#define CSIDL_TO_STR(x) case x: return#x;
    switch (folder)
    {
    CSIDL_TO_STR(CSIDL_DESKTOP);
    CSIDL_TO_STR(CSIDL_INTERNET);
    CSIDL_TO_STR(CSIDL_PROGRAMS);
    CSIDL_TO_STR(CSIDL_CONTROLS);
    CSIDL_TO_STR(CSIDL_PRINTERS);
    CSIDL_TO_STR(CSIDL_PERSONAL);
    CSIDL_TO_STR(CSIDL_FAVORITES);
    CSIDL_TO_STR(CSIDL_STARTUP);
    CSIDL_TO_STR(CSIDL_RECENT);
    CSIDL_TO_STR(CSIDL_SENDTO);
    CSIDL_TO_STR(CSIDL_BITBUCKET);
    CSIDL_TO_STR(CSIDL_STARTMENU);
    CSIDL_TO_STR(OLD_CSIDL_MYDOCUMENTS);
    CSIDL_TO_STR(CSIDL_MYMUSIC);
    CSIDL_TO_STR(CSIDL_MYVIDEO);
    CSIDL_TO_STR(CSIDL_DESKTOPDIRECTORY);
    CSIDL_TO_STR(CSIDL_DRIVES);
    CSIDL_TO_STR(CSIDL_NETWORK);
    CSIDL_TO_STR(CSIDL_NETHOOD);
    CSIDL_TO_STR(CSIDL_FONTS);
    CSIDL_TO_STR(CSIDL_TEMPLATES);
    CSIDL_TO_STR(CSIDL_COMMON_STARTMENU);
    CSIDL_TO_STR(CSIDL_COMMON_PROGRAMS);
    CSIDL_TO_STR(CSIDL_COMMON_STARTUP);
    CSIDL_TO_STR(CSIDL_COMMON_DESKTOPDIRECTORY);
    CSIDL_TO_STR(CSIDL_APPDATA);
    CSIDL_TO_STR(CSIDL_PRINTHOOD);
    CSIDL_TO_STR(CSIDL_LOCAL_APPDATA);
    CSIDL_TO_STR(CSIDL_ALTSTARTUP);
    CSIDL_TO_STR(CSIDL_COMMON_ALTSTARTUP);
    CSIDL_TO_STR(CSIDL_COMMON_FAVORITES);
    CSIDL_TO_STR(CSIDL_INTERNET_CACHE);
    CSIDL_TO_STR(CSIDL_COOKIES);
    CSIDL_TO_STR(CSIDL_HISTORY);
    CSIDL_TO_STR(CSIDL_COMMON_APPDATA);
    CSIDL_TO_STR(CSIDL_WINDOWS);
    CSIDL_TO_STR(CSIDL_SYSTEM);
    CSIDL_TO_STR(CSIDL_PROGRAM_FILES);
    CSIDL_TO_STR(CSIDL_MYPICTURES);
    CSIDL_TO_STR(CSIDL_PROFILE);
    CSIDL_TO_STR(CSIDL_SYSTEMX86);
    CSIDL_TO_STR(CSIDL_PROGRAM_FILESX86);
    CSIDL_TO_STR(CSIDL_PROGRAM_FILES_COMMON);
    CSIDL_TO_STR(CSIDL_PROGRAM_FILES_COMMONX86);
    CSIDL_TO_STR(CSIDL_COMMON_TEMPLATES);
    CSIDL_TO_STR(CSIDL_COMMON_DOCUMENTS);
    CSIDL_TO_STR(CSIDL_COMMON_ADMINTOOLS);
    CSIDL_TO_STR(CSIDL_ADMINTOOLS);
    CSIDL_TO_STR(CSIDL_CONNECTIONS);
    CSIDL_TO_STR(CSIDL_PROFILES);
    CSIDL_TO_STR(CSIDL_COMMON_MUSIC);
    CSIDL_TO_STR(CSIDL_COMMON_PICTURES);
    CSIDL_TO_STR(CSIDL_COMMON_VIDEO);
    CSIDL_TO_STR(CSIDL_RESOURCES);
    CSIDL_TO_STR(CSIDL_RESOURCES_LOCALIZED);
    CSIDL_TO_STR(CSIDL_COMMON_OEM_LINKS);
    CSIDL_TO_STR(CSIDL_CDBURN_AREA);
    CSIDL_TO_STR(CSIDL_COMPUTERSNEARME);
#undef CSIDL_TO_STR
    default:
        sprintf(unknown, "unknown (0x%04x)", folder);
        return unknown;
    }
}

static const char *printGUID(const GUID *guid, char * guidSTR)
{
    if (!guid) return NULL;

    sprintf(guidSTR, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
     guid->Data1, guid->Data2, guid->Data3,
     guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
     guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
    return guidSTR;
}

static void test_parameters(void)
{
    LPITEMIDLIST pidl = NULL;
    char path[MAX_PATH];
    HRESULT hr;

    if (pSHGetFolderLocation)
    {
        /* check a bogus CSIDL: */
        pidl = NULL;
        hr = pSHGetFolderLocation(NULL, 0xeeee, NULL, 0, &pidl);
        ok(hr == E_INVALIDARG, "got 0x%08x, expected E_INVALIDARG\n", hr);
        if (hr == S_OK) IMalloc_Free(pMalloc, pidl);

        /* check a bogus user token: */
        pidl = NULL;
        hr = pSHGetFolderLocation(NULL, CSIDL_FAVORITES, (HANDLE)2, 0, &pidl);
        ok(hr == E_FAIL || hr == E_HANDLE, "got 0x%08x, expected E_FAIL or E_HANDLE\n", hr);
        if (hr == S_OK) IMalloc_Free(pMalloc, pidl);

        /* a NULL pidl pointer crashes, so don't test it */
    }

    if (pSHGetSpecialFolderLocation)
    {
        if (0)
            /* crashes */
            SHGetSpecialFolderLocation(NULL, 0, NULL);

        hr = pSHGetSpecialFolderLocation(NULL, 0xeeee, &pidl);
        ok(hr == E_INVALIDARG, "got returned 0x%08x\n", hr);
    }

    if (pSHGetFolderPathA)
    {
        /* expect 2's a bogus handle, especially since we didn't open it */
        hr = pSHGetFolderPathA(NULL, CSIDL_DESKTOP, (HANDLE)2, SHGFP_TYPE_DEFAULT, path);
        ok(hr == E_FAIL || hr == E_HANDLE || /* Vista and 2k8 */
           broken(hr == S_OK), /* W2k and Me */ "got 0x%08x, expected E_FAIL\n", hr);

        hr = pSHGetFolderPathA(NULL, 0xeeee, NULL, SHGFP_TYPE_DEFAULT, path);
        ok(hr == E_INVALIDARG, "got 0x%08x, expected E_INVALIDARG\n", hr);
    }

    if (pSHGetSpecialFolderPathA)
    {
        BOOL ret;

        if (0)
           pSHGetSpecialFolderPathA(NULL, NULL, CSIDL_BITBUCKET, FALSE);

        /* odd but true: calling with a NULL path still succeeds if it's a real
         * dir (on some windows platform).  on winME it generates exception.
         */
        ret = pSHGetSpecialFolderPathA(NULL, path, CSIDL_PROGRAMS, FALSE);
        ok(ret, "got %d\n", ret);

        ret = pSHGetSpecialFolderPathA(NULL, path, 0xeeee, FALSE);
        ok(!ret, "got %d\n", ret);
    }
}

/* Returns the folder's PIDL type, or 0xff if one can't be found. */
static BYTE testSHGetFolderLocation(int folder)
{
    LPITEMIDLIST pidl;
    HRESULT hr;
    BYTE ret = 0xff;

    /* treat absence of function as success */
    if (!pSHGetFolderLocation) return TRUE;

    pidl = NULL;
    hr = pSHGetFolderLocation(NULL, folder, NULL, 0, &pidl);
    if (hr == S_OK)
    {
        if (pidl)
        {
            LPITEMIDLIST pidlLast = pILFindLastID(pidl);

            ok(pidlLast != NULL, "%s: ILFindLastID failed\n",
             getFolderName(folder));
            if (pidlLast)
                ret = pidlLast->mkid.abID[0];
            IMalloc_Free(pMalloc, pidl);
        }
    }
    return ret;
}

/* Returns the folder's PIDL type, or 0xff if one can't be found. */
static BYTE testSHGetSpecialFolderLocation(int folder)
{
    LPITEMIDLIST pidl;
    HRESULT hr;
    BYTE ret = 0xff;

    /* treat absence of function as success */
    if (!pSHGetSpecialFolderLocation) return TRUE;

    pidl = NULL;
    hr = pSHGetSpecialFolderLocation(NULL, folder, &pidl);
    if (hr == S_OK)
    {
        if (pidl)
        {
            LPITEMIDLIST pidlLast = pILFindLastID(pidl);

            ok(pidlLast != NULL,
                "%s: ILFindLastID failed\n", getFolderName(folder));
            if (pidlLast)
                ret = pidlLast->mkid.abID[0];
            IMalloc_Free(pMalloc, pidl);
        }
    }
    return ret;
}

static void test_SHGetFolderPath(BOOL optional, int folder)
{
    char path[MAX_PATH];
    HRESULT hr;

    if (!pSHGetFolderPathA) return;

    hr = pSHGetFolderPathA(NULL, folder, NULL, SHGFP_TYPE_CURRENT, path);
    ok(hr == S_OK || optional,
     "SHGetFolderPathA(NULL, %s, NULL, SHGFP_TYPE_CURRENT, path) failed: 0x%08x\n", getFolderName(folder), hr);
}

static void test_SHGetSpecialFolderPath(BOOL optional, int folder)
{
    char path[MAX_PATH];
    BOOL ret;

    if (!pSHGetSpecialFolderPathA) return;

    ret = pSHGetSpecialFolderPathA(NULL, path, folder, FALSE);
    if (ret && winetest_interactive)
        printf("%s: %s\n", getFolderName(folder), path);
    ok(ret || optional,
     "SHGetSpecialFolderPathA(NULL, path, %s, FALSE) failed\n",
     getFolderName(folder));
}

static void test_ShellValues(const struct shellExpectedValues testEntries[],
 int numEntries, BOOL optional)
{
    int i;

    for (i = 0; i < numEntries; i++)
    {
        BYTE type;
        int j;
        BOOL foundTypeMatch = FALSE;

        if (pSHGetFolderLocation)
        {
            type = testSHGetFolderLocation(testEntries[i].folder);
            for (j = 0; !foundTypeMatch && j < testEntries[i].numTypes; j++)
                if (testEntries[i].types[j] == type)
                    foundTypeMatch = TRUE;
            ok(foundTypeMatch || optional || broken(type == 0xff) /* Win9x */,
             "%s has unexpected type %d (0x%02x)\n",
             getFolderName(testEntries[i].folder), type, type);
        }
        type = testSHGetSpecialFolderLocation(testEntries[i].folder);
        for (j = 0, foundTypeMatch = FALSE; !foundTypeMatch &&
         j < testEntries[i].numTypes; j++)
            if (testEntries[i].types[j] == type)
                foundTypeMatch = TRUE;
        ok(foundTypeMatch || optional || broken(type == 0xff) /* Win9x */,
         "%s has unexpected type %d (0x%02x)\n",
         getFolderName(testEntries[i].folder), type, type);
        switch (type)
        {
            case PT_FOLDER:
            case PT_DRIVE:
            case PT_DRIVE2:
            case PT_IESPECIAL2:
                test_SHGetFolderPath(optional, testEntries[i].folder);
                test_SHGetSpecialFolderPath(optional, testEntries[i].folder);
                break;
        }
    }
}

/* Attempts to verify that the folder path corresponding to the folder CSIDL
 * value has the same value as the environment variable with name envVar.
 * Doesn't mind if SHGetSpecialFolderPath fails for folder or if envVar isn't
 * set in this environment; different OS and shell version behave differently.
 * However, if both are present, fails if envVar's value is not the same
 * (byte-for-byte) as what SHGetSpecialFolderPath returns.
 */
static void matchSpecialFolderPathToEnv(int folder, const char *envVar)
{
    char path[MAX_PATH];

    if (!pSHGetSpecialFolderPathA) return;

    if (pSHGetSpecialFolderPathA(NULL, path, folder, FALSE))
    {
        char *envVal = getenv(envVar);

        ok(!envVal || !lstrcmpiA(envVal, path),
         "%%%s%% does not match SHGetSpecialFolderPath:\n"
         "%%%s%% is %s\nSHGetSpecialFolderPath returns %s\n",
         envVar, envVar, envVal, path);
    }
}

/* Attempts to match the GUID returned by SHGetFolderLocation for folder with
 * GUID.  Assumes the type of the returned PIDL is in fact a GUID, but doesn't
 * fail if it isn't--that check should already have been done.
 * Fails if the returned PIDL is a GUID whose value does not match guid.
 */
static void matchGUID(int folder, const GUID *guid, const GUID *guid_alt)
{
    LPITEMIDLIST pidl;
    HRESULT hr;

    if (!pSHGetFolderLocation) return;
    if (!guid) return;

    pidl = NULL;
    hr = pSHGetFolderLocation(NULL, folder, NULL, 0, &pidl);
    if (hr == S_OK)
    {
        LPITEMIDLIST pidlLast = pILFindLastID(pidl);

        if (pidlLast && (pidlLast->mkid.abID[0] == PT_SHELLEXT ||
         pidlLast->mkid.abID[0] == PT_GUID))
        {
            GUID *shellGuid = (GUID *)(pidlLast->mkid.abID + 2);
            char shellGuidStr[39], guidStr[39], guid_altStr[39];

            if (!guid_alt)
             ok(IsEqualIID(shellGuid, guid),
              "%s: got GUID %s, expected %s\n", getFolderName(folder),
              printGUID(shellGuid, shellGuidStr), printGUID(guid, guidStr));
            else
             ok(IsEqualIID(shellGuid, guid) ||
              IsEqualIID(shellGuid, guid_alt),
              "%s: got GUID %s, expected %s or %s\n", getFolderName(folder),
              printGUID(shellGuid, shellGuidStr), printGUID(guid, guidStr),
              printGUID(guid_alt, guid_altStr));
        }
        IMalloc_Free(pMalloc, pidl);
    }
}

/* Checks the PIDL type of all the known values. */
static void test_PidlTypes(void)
{
    /* Desktop */
    test_SHGetFolderPath(FALSE, CSIDL_DESKTOP);
    test_SHGetSpecialFolderPath(FALSE, CSIDL_DESKTOP);

    test_ShellValues(requiredShellValues, ARRAY_SIZE(requiredShellValues), FALSE);
    test_ShellValues(optionalShellValues, ARRAY_SIZE(optionalShellValues), TRUE);
}

/* FIXME: Should be in shobjidl.idl */
DEFINE_GUID(CLSID_NetworkExplorerFolder, 0xF02C1A0D, 0xBE21, 0x4350, 0x88, 0xB0, 0x73, 0x67, 0xFC, 0x96, 0xEF, 0x3C);

/* Verifies various shell virtual folders have the correct well-known GUIDs. */
static void test_GUIDs(void)
{
    matchGUID(CSIDL_BITBUCKET, &CLSID_RecycleBin, NULL);
    matchGUID(CSIDL_CONTROLS, &CLSID_ControlPanel, NULL);
    matchGUID(CSIDL_DRIVES, &CLSID_MyComputer, NULL);
    matchGUID(CSIDL_INTERNET, &CLSID_Internet, NULL);
    matchGUID(CSIDL_NETWORK, &CLSID_NetworkPlaces, &CLSID_NetworkExplorerFolder); /* Vista and higher */
    matchGUID(CSIDL_PERSONAL, &CLSID_MyDocuments, NULL);
    matchGUID(CSIDL_COMMON_DOCUMENTS, &CLSID_CommonDocuments, NULL);
    matchGUID(CSIDL_PRINTERS, &CLSID_Printers, NULL);
}

/* Verifies various shell paths match the environment variables to which they
 * correspond.
 */
static void test_EnvVars(void)
{
    matchSpecialFolderPathToEnv(CSIDL_PROGRAM_FILES, "ProgramFiles");
    matchSpecialFolderPathToEnv(CSIDL_APPDATA, "APPDATA");
    matchSpecialFolderPathToEnv(CSIDL_PROFILE, "USERPROFILE");
    matchSpecialFolderPathToEnv(CSIDL_WINDOWS, "SystemRoot");
    matchSpecialFolderPathToEnv(CSIDL_WINDOWS, "windir");
    matchSpecialFolderPathToEnv(CSIDL_PROGRAM_FILES_COMMON, "CommonProgramFiles");
    /* this is only set on Wine, but can't hurt to verify it: */
    matchSpecialFolderPathToEnv(CSIDL_SYSTEM, "winsysdir");
}

/* Loosely based on PathRemoveBackslashA from dlls/shlwapi/path.c */
static BOOL myPathIsRootA(LPCSTR lpszPath)
{
  if (lpszPath && *lpszPath &&
      lpszPath[1] == ':' && lpszPath[2] == '\\' && lpszPath[3] == '\0')
      return TRUE; /* X:\ */
  return FALSE;
}
static LPSTR myPathRemoveBackslashA( LPSTR lpszPath )
{
  LPSTR szTemp = NULL;

  if(lpszPath)
  {
    szTemp = CharPrevA(lpszPath, lpszPath + strlen(lpszPath));
    if (!myPathIsRootA(lpszPath) && *szTemp == '\\')
      *szTemp = '\0';
  }
  return szTemp;
}

/* Verifies the shell path for CSIDL_WINDOWS matches the return from
 * GetWindowsDirectory.  If SHGetSpecialFolderPath fails, no harm, no foul--not
 * every shell32 version supports CSIDL_WINDOWS.
 */
static void testWinDir(void)
{
    char windowsShellPath[MAX_PATH], windowsDir[MAX_PATH] = { 0 };

    if (!pSHGetSpecialFolderPathA) return;

    if (pSHGetSpecialFolderPathA(NULL, windowsShellPath, CSIDL_WINDOWS, FALSE))
    {
        myPathRemoveBackslashA(windowsShellPath);
        GetWindowsDirectoryA(windowsDir, sizeof(windowsDir));
        myPathRemoveBackslashA(windowsDir);
        ok(!lstrcmpiA(windowsDir, windowsShellPath),
         "GetWindowsDirectory returns %s SHGetSpecialFolderPath returns %s\n",
         windowsDir, windowsShellPath);
    }
}

/* Verifies the shell path for CSIDL_SYSTEM matches the return from
 * GetSystemDirectory.  If SHGetSpecialFolderPath fails, no harm,
 * no foul--not every shell32 version supports CSIDL_SYSTEM.
 */
static void testSystemDir(void)
{
    char systemShellPath[MAX_PATH], systemDir[MAX_PATH], systemDirx86[MAX_PATH];

    if (!pSHGetSpecialFolderPathA) return;

    GetSystemDirectoryA(systemDir, sizeof(systemDir));
    myPathRemoveBackslashA(systemDir);
    if (pSHGetSpecialFolderPathA(NULL, systemShellPath, CSIDL_SYSTEM, FALSE))
    {
        myPathRemoveBackslashA(systemShellPath);
        ok(!lstrcmpiA(systemDir, systemShellPath),
         "GetSystemDirectory returns %s SHGetSpecialFolderPath returns %s\n",
         systemDir, systemShellPath);
    }

    if (!pGetSystemWow64DirectoryA || !pGetSystemWow64DirectoryA(systemDirx86, sizeof(systemDirx86)))
        GetSystemDirectoryA(systemDirx86, sizeof(systemDirx86));
    myPathRemoveBackslashA(systemDirx86);
    if (pSHGetSpecialFolderPathA(NULL, systemShellPath, CSIDL_SYSTEMX86, FALSE))
    {
        myPathRemoveBackslashA(systemShellPath);
        ok(!lstrcmpiA(systemDirx86, systemShellPath) || broken(!lstrcmpiA(systemDir, systemShellPath)),
         "GetSystemDirectory returns %s SHGetSpecialFolderPath returns %s\n",
         systemDir, systemShellPath);
    }
}

/* Globals used by subprocesses */
static int    myARGC;
static char **myARGV;
static char   base[MAX_PATH];
static char   selfname[MAX_PATH];

static int init(void)
{
    myARGC = winetest_get_mainargs(&myARGV);
    if (!GetCurrentDirectoryA(sizeof(base), base)) return 0;
    strcpy(selfname, myARGV[0]);
    return 1;
}

static void doChild(const char *arg)
{
    char path[MAX_PATH];
    HRESULT hr;

    if (arg[0] == '1')
    {
        LPITEMIDLIST pidl;
        char *p;

        /* test what happens when CSIDL_FAVORITES is set to a nonexistent directory */

        /* test some failure cases first: */
        hr = pSHGetFolderPathA(NULL, CSIDL_FAVORITES, NULL, SHGFP_TYPE_CURRENT, path);
        ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            "SHGetFolderPath returned 0x%08x, expected 0x80070002\n", hr);

        pidl = NULL;
        hr = pSHGetFolderLocation(NULL, CSIDL_FAVORITES, NULL, 0, &pidl);
        ok(hr == E_FAIL || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            "SHGetFolderLocation returned 0x%08x\n", hr);
        if (hr == S_OK && pidl) IMalloc_Free(pMalloc, pidl);

        ok(!pSHGetSpecialFolderPathA(NULL, path, CSIDL_FAVORITES, FALSE),
            "SHGetSpecialFolderPath succeeded, expected failure\n");

        pidl = NULL;
        hr = pSHGetSpecialFolderLocation(NULL, CSIDL_FAVORITES, &pidl);
        ok(hr == E_FAIL || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            "SHGetFolderLocation returned 0x%08x\n", hr);

        if (hr == S_OK && pidl) IMalloc_Free(pMalloc, pidl);

        /* now test success: */
        hr = pSHGetFolderPathA(NULL, CSIDL_FAVORITES | CSIDL_FLAG_CREATE, NULL,
                               SHGFP_TYPE_CURRENT, path);
        ok (hr == S_OK, "got 0x%08x\n", hr);
        if (hr == S_OK)
        {
            BOOL ret;

            trace("CSIDL_FAVORITES was changed to %s\n", path);
            ret = CreateDirectoryA(path, NULL);
            ok(!ret, "expected failure with ERROR_ALREADY_EXISTS\n");
            if (!ret)
                ok(GetLastError() == ERROR_ALREADY_EXISTS,
                  "got %d, expected ERROR_ALREADY_EXISTS\n", GetLastError());

            p = path + strlen(path);
            strcpy(p, "\\desktop.ini");
            DeleteFileA(path);
            *p = 0;
            SetFileAttributesA( path, FILE_ATTRIBUTE_NORMAL );
            ret = RemoveDirectoryA(path);
            ok( ret, "failed to remove %s error %u\n", path, GetLastError() );
        }
    }
    else if (arg[0] == '2')
    {
        /* make sure SHGetFolderPath still succeeds when the
           original value of CSIDL_FAVORITES is restored. */
        hr = pSHGetFolderPathA(NULL, CSIDL_FAVORITES | CSIDL_FLAG_CREATE, NULL,
            SHGFP_TYPE_CURRENT, path);
        ok(hr == S_OK, "SHGetFolderPath failed: 0x%08x\n", hr);
    }
}

/* Tests the return values from the various shell functions both with and
 * without the use of the CSIDL_FLAG_CREATE flag.  This flag only appeared in
 * version 5 of the shell, so don't test unless it's at least version 5.
 * The test reads a value from the registry, modifies it, calls
 * SHGetFolderPath once with the CSIDL_FLAG_CREATE flag, and immediately
 * afterward without it.  Then it restores the registry and deletes the folder
 * that was created.
 * One oddity with respect to restoration: shell32 caches somehow, so it needs
 * to be reloaded in order to see the correct (restored) value.
 * Some APIs unrelated to the ones under test may fail, but I expect they're
 * covered by other unit tests; I just print out something about failure to
 * help trace what's going on.
 */
static void test_NonExistentPath(void)
{
    static const char userShellFolders[] = 
     "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders";
    char originalPath[MAX_PATH], modifiedPath[MAX_PATH];
    HKEY key;

    if (!pSHGetFolderPathA) return;
    if (!pSHGetFolderLocation) return;
    if (!pSHGetSpecialFolderPathA) return;
    if (!pSHGetSpecialFolderLocation) return;
    if (!pSHFileOperationA) return;
    if (shellVersion.dwMajorVersion < 5) return;

    if (!RegOpenKeyExA(HKEY_CURRENT_USER, userShellFolders, 0, KEY_ALL_ACCESS,
     &key))
    {
        DWORD len, type;

        len = sizeof(originalPath);
        if (!RegQueryValueExA(key, "Favorites", NULL, &type,
         (LPBYTE)&originalPath, &len))
        {
            size_t len = strlen(originalPath);

            memcpy(modifiedPath, originalPath, len);
            modifiedPath[len++] = '2';
            modifiedPath[len++] = '\0';
            trace("Changing CSIDL_FAVORITES to %s\n", modifiedPath);
            if (!RegSetValueExA(key, "Favorites", 0, type,
             (LPBYTE)modifiedPath, len))
            {
                char buffer[MAX_PATH+20];
                STARTUPINFOA startup;
                PROCESS_INFORMATION info;

                sprintf(buffer, "%s tests/shellpath.c 1", selfname);
                memset(&startup, 0, sizeof(startup));
                startup.cb = sizeof(startup);
                startup.dwFlags = STARTF_USESHOWWINDOW;
                startup.dwFlags = SW_SHOWNORMAL;
                CreateProcessA(NULL, buffer, NULL, NULL, FALSE, 0L, NULL, NULL,
                 &startup, &info);
                winetest_wait_child_process( info.hProcess );

                /* restore original values: */
                trace("Restoring CSIDL_FAVORITES to %s\n", originalPath);
                RegSetValueExA(key, "Favorites", 0, type, (LPBYTE) originalPath,
                 strlen(originalPath) + 1);
                RegFlushKey(key);

                sprintf(buffer, "%s tests/shellpath.c 2", selfname);
                memset(&startup, 0, sizeof(startup));
                startup.cb = sizeof(startup);
                startup.dwFlags = STARTF_USESHOWWINDOW;
                startup.dwFlags = SW_SHOWNORMAL;
                CreateProcessA(NULL, buffer, NULL, NULL, FALSE, 0L, NULL, NULL,
                 &startup, &info);
                ok(WaitForSingleObject(info.hProcess, 30000) == WAIT_OBJECT_0,
                 "child process termination\n");
            }
        }
        else skip("RegQueryValueExA(key, Favorites, ...) failed\n");
        if (key)
            RegCloseKey(key);
    }
    else skip("RegOpenKeyExA(HKEY_CURRENT_USER, %s, ...) failed\n", userShellFolders);
}

static void test_SHGetFolderPathEx(void)
{
    HRESULT hr;
    WCHAR buffer[MAX_PATH], *path;
    DWORD len;

    if (!pSHGetKnownFolderPath || !pSHGetFolderPathEx)
    {
        win_skip("SHGetKnownFolderPath or SHGetFolderPathEx not available\n");
        return;
    }

if (0) { /* crashes */
    hr = pSHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, NULL);
    ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got 0x%08x\n", hr);
}
    path = NULL;
    hr = pSHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &path);
    ok(hr == S_OK, "expected S_OK, got 0x%08x\n", hr);
    ok(path != NULL, "expected path != NULL\n");

    hr = pSHGetFolderPathEx(&FOLDERID_Desktop, 0, NULL, buffer, MAX_PATH);
    ok(hr == S_OK, "expected S_OK, got 0x%08x\n", hr);
    ok(!lstrcmpiW(path, buffer), "expected equal paths\n");
    len = lstrlenW(buffer);
    CoTaskMemFree(path);

    hr = pSHGetFolderPathEx(&FOLDERID_Desktop, 0, NULL, buffer, 0);
    ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got 0x%08x\n", hr);

if (0) { /* crashes */
    hr = pSHGetFolderPathEx(&FOLDERID_Desktop, 0, NULL, NULL, len + 1);
    ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got 0x%08x\n", hr);

    hr = pSHGetFolderPathEx(NULL, 0, NULL, buffer, MAX_PATH);
    ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got 0x%08x\n", hr);
}
    hr = pSHGetFolderPathEx(&FOLDERID_Desktop, 0, NULL, buffer, len);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "expected 0x8007007a, got 0x%08x\n", hr);

    hr = pSHGetFolderPathEx(&FOLDERID_Desktop, 0, NULL, buffer, len + 1);
    ok(hr == S_OK, "expected S_OK, got 0x%08x\n", hr);
}

/* Standard CSIDL values (and their flags) uses only two less-significant bytes */
#define NO_CSIDL 0x10000
#define CSIDL_TODO_WINE 0x20000
#define KNOWN_FOLDER(id, csidl) \
    { &id, # id, csidl, # csidl, __LINE__ }

struct knownFolderDef {
    const KNOWNFOLDERID *folderId;
    const char *sFolderId;
    const int csidl;
    const char *sCsidl;
    const int line;
};

static const struct knownFolderDef known_folders[] = {
    KNOWN_FOLDER(FOLDERID_AddNewPrograms,         NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_AddNewPrograms,         NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_AdminTools,             CSIDL_ADMINTOOLS),
    KNOWN_FOLDER(FOLDERID_AppUpdates,             NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_CDBurning,              CSIDL_CDBURN_AREA),
    KNOWN_FOLDER(FOLDERID_ChangeRemovePrograms,   NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_CommonAdminTools,       CSIDL_COMMON_ADMINTOOLS),
    KNOWN_FOLDER(FOLDERID_CommonOEMLinks,         CSIDL_COMMON_OEM_LINKS),
    KNOWN_FOLDER(FOLDERID_CommonPrograms,         CSIDL_COMMON_PROGRAMS),
    KNOWN_FOLDER(FOLDERID_CommonStartMenu,        CSIDL_COMMON_STARTMENU),
    KNOWN_FOLDER(FOLDERID_CommonStartup,          CSIDL_COMMON_STARTUP),
    KNOWN_FOLDER(FOLDERID_CommonTemplates,        CSIDL_COMMON_TEMPLATES),
    KNOWN_FOLDER(FOLDERID_ComputerFolder,         CSIDL_DRIVES),
    KNOWN_FOLDER(FOLDERID_ConflictFolder,         NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_ConnectionsFolder,      CSIDL_CONNECTIONS),
    KNOWN_FOLDER(FOLDERID_Contacts,               NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_ControlPanelFolder,     CSIDL_CONTROLS),
    KNOWN_FOLDER(FOLDERID_Cookies,                CSIDL_COOKIES),
    KNOWN_FOLDER(FOLDERID_Desktop,                CSIDL_DESKTOP),
    KNOWN_FOLDER(FOLDERID_DeviceMetadataStore,    NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Documents,              CSIDL_MYDOCUMENTS | CSIDL_TODO_WINE),
    KNOWN_FOLDER(FOLDERID_DocumentsLibrary,       NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Downloads,              NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Favorites,              CSIDL_FAVORITES),
    KNOWN_FOLDER(FOLDERID_Fonts,                  CSIDL_FONTS),
    KNOWN_FOLDER(FOLDERID_Games,                  NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_GameTasks,              NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_History,                CSIDL_HISTORY),
    KNOWN_FOLDER(FOLDERID_HomeGroup,              NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_ImplicitAppShortcuts,   NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_InternetCache,          CSIDL_INTERNET_CACHE),
    KNOWN_FOLDER(FOLDERID_InternetFolder,         CSIDL_INTERNET),
    KNOWN_FOLDER(FOLDERID_Libraries,              NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Links,                  NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_LocalAppData,           CSIDL_LOCAL_APPDATA),
    KNOWN_FOLDER(FOLDERID_LocalAppDataLow,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_LocalizedResourcesDir,  CSIDL_RESOURCES_LOCALIZED),
    KNOWN_FOLDER(FOLDERID_Music,                  CSIDL_MYMUSIC),
    KNOWN_FOLDER(FOLDERID_MusicLibrary,           NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_NetHood,                CSIDL_NETHOOD),
    KNOWN_FOLDER(FOLDERID_NetworkFolder,          CSIDL_NETWORK),
    KNOWN_FOLDER(FOLDERID_OriginalImages,         NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PhotoAlbums,            NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Pictures,               CSIDL_MYPICTURES),
    KNOWN_FOLDER(FOLDERID_PicturesLibrary,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Playlists,              NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PrintersFolder,         CSIDL_PRINTERS),
    KNOWN_FOLDER(FOLDERID_PrintHood,              CSIDL_PRINTHOOD),
    KNOWN_FOLDER(FOLDERID_Profile,                CSIDL_PROFILE),
    KNOWN_FOLDER(FOLDERID_ProgramData,            CSIDL_COMMON_APPDATA),
    KNOWN_FOLDER(FOLDERID_ProgramFiles,           CSIDL_PROGRAM_FILES),
    KNOWN_FOLDER(FOLDERID_ProgramFilesCommon,     CSIDL_PROGRAM_FILES_COMMON),
    KNOWN_FOLDER(FOLDERID_ProgramFilesCommonX86,  NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_ProgramFilesX86,        CSIDL_PROGRAM_FILESX86),
    KNOWN_FOLDER(FOLDERID_Programs,               CSIDL_PROGRAMS),
    KNOWN_FOLDER(FOLDERID_Public,                 NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PublicDesktop,          CSIDL_COMMON_DESKTOPDIRECTORY),
    KNOWN_FOLDER(FOLDERID_PublicDocuments,        CSIDL_COMMON_DOCUMENTS),
    KNOWN_FOLDER(FOLDERID_PublicDownloads,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PublicGameTasks,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PublicLibraries,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PublicMusic,            CSIDL_COMMON_MUSIC),
    KNOWN_FOLDER(FOLDERID_PublicPictures,         CSIDL_COMMON_PICTURES),
    KNOWN_FOLDER(FOLDERID_PublicRingtones,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_PublicVideos,           CSIDL_COMMON_VIDEO),
    KNOWN_FOLDER(FOLDERID_QuickLaunch,            NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Recent,                 CSIDL_RECENT),
    KNOWN_FOLDER(FOLDERID_RecordedTVLibrary,      NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_RecycleBinFolder,       CSIDL_BITBUCKET),
    KNOWN_FOLDER(FOLDERID_ResourceDir,            CSIDL_RESOURCES),
    KNOWN_FOLDER(FOLDERID_Ringtones,              NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_RoamingAppData,         CSIDL_APPDATA),
    KNOWN_FOLDER(FOLDERID_SampleMusic,            NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SamplePictures,         NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SamplePlaylists,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SampleVideos,           NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SavedGames,             NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SavedSearches,          NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SEARCH_CSC,             NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SearchHome,             NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SEARCH_MAPI,            NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SendTo,                 CSIDL_SENDTO),
    KNOWN_FOLDER(FOLDERID_SidebarDefaultParts,    NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SidebarParts,           NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_StartMenu,              CSIDL_STARTMENU),
    KNOWN_FOLDER(FOLDERID_Startup,                CSIDL_STARTUP),
    KNOWN_FOLDER(FOLDERID_SyncManagerFolder,      NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SyncResultsFolder,      NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_SyncSetupFolder,        NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_System,                 CSIDL_SYSTEM),
    KNOWN_FOLDER(FOLDERID_SystemX86,              CSIDL_SYSTEMX86),
    KNOWN_FOLDER(FOLDERID_Templates,              CSIDL_TEMPLATES),
    KNOWN_FOLDER(FOLDERID_UserPinned,             NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_UserProfiles,           NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_UserProgramFiles,       NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_UserProgramFilesCommon, NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_UsersFiles,             NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_UsersLibraries,         NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Videos,                 CSIDL_MYVIDEO),
    KNOWN_FOLDER(FOLDERID_VideosLibrary,          NO_CSIDL),
    KNOWN_FOLDER(FOLDERID_Windows,                CSIDL_WINDOWS),
    { NULL, NULL, 0, NULL }
};
#undef KNOWN_FOLDER

static void check_known_folder(IKnownFolderManager *mgr, KNOWNFOLDERID *folderId)
{
    HRESULT hr;
    const struct knownFolderDef *known_folder = &known_folders[0];
    int csidl, expectedCsidl;

    while(known_folder->folderId != NULL)
    {
        if(IsEqualGUID(known_folder->folderId, folderId))
        {
            /* verify CSIDL */
            if(known_folder->csidl != NO_CSIDL)
            {
                expectedCsidl = known_folder->csidl & (~CSIDL_TODO_WINE);

                hr = IKnownFolderManager_FolderIdToCsidl(mgr, folderId, &csidl);
                ok_(__FILE__, known_folder->line)(hr == S_OK, "cannot retrieve CSIDL for folder %s\n", known_folder->sFolderId);

                if(known_folder->csidl & CSIDL_TODO_WINE)
                    todo_wine ok_(__FILE__, known_folder->line)(csidl == expectedCsidl, "invalid CSIDL retrieved for folder %s. %d (%s) expected, but %d found\n", known_folder->sFolderId, expectedCsidl, known_folder->sCsidl, csidl);
                else
                    ok_(__FILE__, known_folder->line)(csidl == expectedCsidl, "invalid CSIDL retrieved for folder %s. %d (%s) expected, but %d found\n", known_folder->sFolderId, expectedCsidl, known_folder->sCsidl, csidl);
            }

            break;
        }
        known_folder++;
    }
}
#undef NO_CSIDL
#undef CSIDL_TODO_WINE

static void test_knownFolders(void)
{
    static const WCHAR sWindows[] = {'W','i','n','d','o','w','s',0};
    static const WCHAR sExample[] = {'E','x','a','m','p','l','e',0};
    static const WCHAR sExample2[] = {'E','x','a','m','p','l','e','2',0};
    static const WCHAR sSubFolder[] = {'S','u','b','F','o','l','d','e','r',0};
    static const WCHAR sBackslash[] = {'\\',0};
    static const KNOWNFOLDERID newFolderId = {0x01234567, 0x89AB, 0xCDEF, {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x01} };
    static const KNOWNFOLDERID subFolderId = {0xFEDCBA98, 0x7654, 0x3210, {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF} };
    HRESULT hr;
    IKnownFolderManager *mgr = NULL;
    IKnownFolder *folder = NULL, *subFolder = NULL;
    KNOWNFOLDERID folderId, *folders;
    KF_CATEGORY cat = 0;
    KNOWNFOLDER_DEFINITION kfDefinition, kfSubDefinition;
    int csidl, i;
    UINT nCount = 0;
    LPWSTR folderPath, errorMsg;
    KF_REDIRECTION_CAPABILITIES redirectionCapabilities = 1;
    WCHAR sWinDir[MAX_PATH], sExamplePath[MAX_PATH], sExample2Path[MAX_PATH], sSubFolderPath[MAX_PATH], sSubFolder2Path[MAX_PATH];
    BOOL bRes;
    DWORD dwAttributes;

    GetWindowsDirectoryW( sWinDir, MAX_PATH );

    GetTempPathW(sizeof(sExamplePath)/sizeof(sExamplePath[0]), sExamplePath);
    lstrcatW(sExamplePath, sExample);

    GetTempPathW(sizeof(sExample2Path)/sizeof(sExample2Path[0]), sExample2Path);
    lstrcatW(sExample2Path, sExample2);

    lstrcpyW(sSubFolderPath, sExamplePath);
    lstrcatW(sSubFolderPath, sBackslash);
    lstrcatW(sSubFolderPath, sSubFolder);

    lstrcpyW(sSubFolder2Path, sExample2Path);
    lstrcatW(sSubFolder2Path, sBackslash);
    lstrcatW(sSubFolder2Path, sSubFolder);

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_KnownFolderManager, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IKnownFolderManager, (LPVOID*)&mgr);
    if(hr == REGDB_E_CLASSNOTREG)
        win_skip("IKnownFolderManager unavailable\n");
    else
    {
        ok(hr == S_OK, "failed to create KnownFolderManager instance: 0x%08x\n", hr);

        hr = IKnownFolderManager_FolderIdFromCsidl(mgr, CSIDL_WINDOWS, &folderId);
        ok(hr == S_OK, "failed to convert CSIDL to KNOWNFOLDERID: 0x%08x\n", hr);
        ok(IsEqualGUID(&folderId, &FOLDERID_Windows)==TRUE, "invalid KNOWNFOLDERID returned\n");

        hr = IKnownFolderManager_FolderIdToCsidl(mgr, &FOLDERID_Windows, &csidl);
        ok(hr == S_OK, "failed to convert CSIDL to KNOWNFOLDERID: 0x%08x\n", hr);
        ok(csidl == CSIDL_WINDOWS, "invalid CSIDL returned\n");

        hr = IKnownFolderManager_GetFolder(mgr, &FOLDERID_Windows, &folder);
        ok(hr == S_OK, "failed to get known folder: 0x%08x\n", hr);
        if(SUCCEEDED(hr))
        {
            hr = IKnownFolder_GetCategory(folder, &cat);
            todo_wine
            ok(hr == S_OK, "failed to get folder category: 0x%08x\n", hr);
            todo_wine
            ok(cat==KF_CATEGORY_FIXED, "invalid folder category: %d\n", cat);

            hr = IKnownFolder_GetId(folder, &folderId);
            ok(hr == S_OK, "failed to get folder id: 0x%08x\n", hr);
            ok(IsEqualGUID(&folderId, &FOLDERID_Windows)==TRUE, "invalid KNOWNFOLDERID returned\n");

            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
            ok(lstrcmpiW(sWinDir, folderPath)==0, "invalid path returned: \"%s\", expected: \"%s\"\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sWinDir));
            CoTaskMemFree(folderPath);

            hr = IKnownFolder_GetRedirectionCapabilities(folder, &redirectionCapabilities);
            todo_wine
            ok(hr == S_OK, "failed to get redirection capabilities: 0x%08x\n", hr);
            todo_wine
            ok(redirectionCapabilities==0, "invalid redirection capabilities returned: %d\n", redirectionCapabilities);

            hr = IKnownFolder_SetPath(folder, 0, sWinDir);
            todo_wine
            ok(hr == E_INVALIDARG, "unexpected value from SetPath: 0x%08x\n", hr);

            hr = IKnownFolder_GetFolderDefinition(folder, &kfDefinition);
            todo_wine
            ok(hr == S_OK, "failed to get folder definition: 0x%08x\n", hr);
            if(SUCCEEDED(hr))
            {
                todo_wine
                ok(kfDefinition.category==KF_CATEGORY_FIXED, "invalid folder category: 0x%08x\n", kfDefinition.category);
                todo_wine
                ok(lstrcmpW(kfDefinition.pszName, sWindows)==0, "invalid folder name: %s\n", wine_dbgstr_w(kfDefinition.pszName));
                todo_wine
                ok(kfDefinition.dwAttributes==0, "invalid folder attributes: %d\n", kfDefinition.dwAttributes);
                FreeKnownFolderDefinitionFields(&kfDefinition);
            }

            hr = IKnownFolder_Release(folder);
            ok(hr == S_OK, "failed to release KnownFolder instance: 0x%08x\n", hr);
        }

        hr = IKnownFolderManager_GetFolderByName(mgr, sWindows, &folder);
        todo_wine
        ok(hr == S_OK, "failed to get known folder: 0x%08x\n", hr);
        if(SUCCEEDED(hr))
        {
            hr = IKnownFolder_GetId(folder, &folderId);
            ok(hr == S_OK, "failed to get folder id: 0x%08x\n", hr);
            ok(IsEqualGUID(&folderId, &FOLDERID_Windows)==TRUE, "invalid KNOWNFOLDERID returned\n");

            hr = IKnownFolder_Release(folder);
            ok(hr == S_OK, "failed to release KnownFolder instance: 0x%08x\n", hr);
        }

        hr = IKnownFolderManager_GetFolderIds(mgr, &folders, &nCount);
        ok(hr == S_OK, "failed to get known folders: 0x%08x\n", hr);
        for(i=0;i<nCount;++i)
            check_known_folder(mgr, &folders[i]);

        CoTaskMemFree(folders);

        /* test of registering new known folders */
        bRes = CreateDirectoryW(sExamplePath, NULL);
        ok(bRes, "cannot create example directory: %s\n", wine_dbgstr_w(sExamplePath));
        bRes = CreateDirectoryW(sExample2Path, NULL);
        ok(bRes, "cannot create example directory: %s\n", wine_dbgstr_w(sExample2Path));
        bRes = CreateDirectoryW(sSubFolderPath, NULL);
        ok(bRes, "cannot create example directory: %s\n", wine_dbgstr_w(sSubFolderPath));

        ZeroMemory(&kfDefinition, sizeof(kfDefinition));
        kfDefinition.category = KF_CATEGORY_PERUSER;
        kfDefinition.pszName = CoTaskMemAlloc(sizeof(sExample));
        lstrcpyW(kfDefinition.pszName, sExample);
        kfDefinition.pszDescription = CoTaskMemAlloc(sizeof(sExample));
        lstrcpyW(kfDefinition.pszDescription, sExample);
        kfDefinition.pszRelativePath = CoTaskMemAlloc(sizeof(sExamplePath));
        lstrcpyW(kfDefinition.pszRelativePath, sExamplePath);

        hr = IKnownFolderManager_RegisterFolder(mgr, &newFolderId, &kfDefinition);
        if(hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED))
            win_skip("No permissions required to register custom known folder\n");
        else
        {
            ok(hr == S_OK, "failed to register known folder: 0x%08x\n", hr);
            if(SUCCEEDED(hr))
            {
                hr = IKnownFolderManager_GetFolder(mgr, &newFolderId, &folder);
                ok(hr == S_OK, "failed to get known folder: 0x%08x\n", hr);
                if(SUCCEEDED(hr))
                {
                    hr = IKnownFolder_GetCategory(folder, &cat);
                    ok(hr == S_OK, "failed to get folder category: hr=0x%0x\n", hr);
                    ok(cat == KF_CATEGORY_PERUSER, "invalid category returned: %d, while %d (KF_CATEGORY_PERUSER) expected\n", cat, KF_CATEGORY_PERUSER);

                    hr = IKnownFolder_GetId(folder, &folderId);
                    ok(hr == S_OK, "failed to get folder id: 0x%08x\n", hr);
                    ok(IsEqualGUID(&folderId, &newFolderId)==TRUE, "invalid KNOWNFOLDERID returned\n");

                    /* current path should be Temp\Example */
                    hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                    ok(hr == S_OK, "failed to get path from known folder: 0x%08x\n", hr);
                    ok(lstrcmpiW(folderPath, sExamplePath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExamplePath));
                    CoTaskMemFree(folderPath);

                    /* register sub-folder and mark it as child of Example folder */
                    ZeroMemory(&kfSubDefinition, sizeof(kfSubDefinition));
                    kfSubDefinition.category = KF_CATEGORY_PERUSER;
                    kfSubDefinition.pszName = CoTaskMemAlloc(sizeof(sSubFolder));
                    lstrcpyW(kfSubDefinition.pszName, sSubFolder);
                    kfSubDefinition.pszDescription = CoTaskMemAlloc(sizeof(sSubFolder));
                    lstrcpyW(kfSubDefinition.pszDescription, sSubFolder);
                    kfSubDefinition.pszRelativePath = CoTaskMemAlloc(sizeof(sSubFolder));
                    lstrcpyW(kfSubDefinition.pszRelativePath, sSubFolder);
                    kfSubDefinition.fidParent = newFolderId;

                    hr = IKnownFolderManager_RegisterFolder(mgr, &subFolderId, &kfSubDefinition);
                    ok(hr == S_OK, "failed to register known folder: 0x%08x\n", hr);
                    if(SUCCEEDED(hr))
                    {

                        hr = IKnownFolderManager_GetFolder(mgr, &subFolderId, &subFolder);
                        ok(hr == S_OK, "failed to get known folder: 0x%08x\n", hr);
                        if(SUCCEEDED(hr))
                        {
                            /* check sub folder path */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolderPath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolderPath));
                            CoTaskMemFree(folderPath);


                            /* try to redirect Example to Temp\Example2  */
                            hr = IKnownFolderManager_Redirect(mgr, &newFolderId, NULL, 0, sExample2Path, 0, NULL, &errorMsg);
                            ok(hr == S_OK, "failed to redirect known folder: 0x%08x, errorMsg: %s\n", hr, wine_dbgstr_w(errorMsg));

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sExample2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExample2Path));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder - it should fail now, as we redirected it's parent folder, but we have no sub folder in new location */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            todo_wine
                            ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "unexpected value from GetPath(): 0x%08x\n", hr);
                            todo_wine
                            ok(folderPath==NULL, "invalid known folder path retrieved: \"%s\" when NULL pointer was expected\n", wine_dbgstr_w(folderPath));
                            CoTaskMemFree(folderPath);


                            /* set Example path to original. Using SetPath() is valid here, as it also uses redirection internally */
                            hr = IKnownFolder_SetPath(folder, 0, sExamplePath);
                            ok(hr == S_OK, "SetPath() failed: 0x%08x\n", hr);

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sExamplePath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExamplePath));
                            CoTaskMemFree(folderPath);


                            /* create sub folder in Temp\Example2 */
                            bRes = CreateDirectoryW(sSubFolder2Path, NULL);
                            ok(bRes, "cannot create example directory: %s\n", wine_dbgstr_w(sSubFolder2Path));

                            /* again perform that same redirection */
                            hr = IKnownFolderManager_Redirect(mgr, &newFolderId, NULL, 0, sExample2Path, 0, NULL, &errorMsg);
                            ok(hr == S_OK, "failed to redirect known folder: 0x%08x, errorMsg: %s\n", hr, wine_dbgstr_w(errorMsg));

                            /* verify sub folder. It should succeed now, as the required sub folder exists */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolder2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolder2Path));
                            CoTaskMemFree(folderPath);

                            /* remove newly created directory */
                            RemoveDirectoryW(sSubFolder2Path);

                            /* verify sub folder. It still succeedes, so Windows does not check folder presence each time */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolder2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolder2Path));
                            CoTaskMemFree(folderPath);


                            /* set Example path to original */
                            hr = IKnownFolder_SetPath(folder, 0, sExamplePath);
                            ok(hr == S_OK, "SetPath() failed: 0x%08x\n", hr);

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sExamplePath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExamplePath));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sSubFolderPath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolderPath));
                            CoTaskMemFree(folderPath);


                            /* create sub folder in Temp\Example2 */
                            bRes = CreateDirectoryW(sSubFolder2Path, NULL);
                            ok(bRes, "cannot create example directory: %s\n", wine_dbgstr_w(sSubFolder2Path));

                            /* do that same redirection, but try to exclude sub-folder */
                            hr = IKnownFolderManager_Redirect(mgr, &newFolderId, NULL, 0, sExample2Path, 1, &subFolderId, &errorMsg);
                            ok(hr == S_OK, "failed to redirect known folder: 0x%08x, errorMsg: %s\n", hr, wine_dbgstr_w(errorMsg));

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sExample2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExample2Path));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder. Unexpectedly, this path was also changed. So, exclusion seems to be ignored (Windows bug)? This test however will let us know, if this behavior is changed */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolder2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolder2Path));
                            CoTaskMemFree(folderPath);

                            /* remove newly created directory */
                            RemoveDirectoryW(sSubFolder2Path);


                            /* set Example path to original */
                            hr = IKnownFolder_SetPath(folder, 0, sExamplePath);
                            ok(hr == S_OK, "SetPath() failed: 0x%08x\n", hr);

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sExamplePath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExamplePath));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sSubFolderPath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolderPath));
                            CoTaskMemFree(folderPath);


                            /* do that same redirection again, but set it to copy content. It should also copy the sub folder, so checking it would succeed now */
                            hr = IKnownFolderManager_Redirect(mgr, &newFolderId, NULL, KF_REDIRECT_COPY_CONTENTS, sExample2Path, 0, NULL, &errorMsg);
                            ok(hr == S_OK, "failed to redirect known folder: 0x%08x, errorMsg: %s\n", hr, wine_dbgstr_w(errorMsg));

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sExample2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExample2Path));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolder2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolder2Path));
                            CoTaskMemFree(folderPath);

                            /* remove copied directory */
                            RemoveDirectoryW(sSubFolder2Path);


                            /* set Example path to original */
                            hr = IKnownFolder_SetPath(folder, 0, sExamplePath);
                            ok(hr == S_OK, "SetPath() failed: 0x%08x\n", hr);

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sExamplePath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExamplePath));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            todo_wine
                            ok(lstrcmpiW(folderPath, sSubFolderPath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolderPath));
                            CoTaskMemFree(folderPath);


                            /* redirect again, set it to copy content and remove originals */
                            hr = IKnownFolderManager_Redirect(mgr, &newFolderId, NULL, KF_REDIRECT_COPY_CONTENTS | KF_REDIRECT_DEL_SOURCE_CONTENTS, sExample2Path, 0, NULL, &errorMsg);
                            ok(hr == S_OK, "failed to redirect known folder: 0x%08x, errorMsg: %s\n", hr, wine_dbgstr_w(errorMsg));

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sExample2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExample2Path));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolder2Path)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolder2Path));
                            CoTaskMemFree(folderPath);

                            /* check if original directory was really removed */
                            dwAttributes = GetFileAttributesW(sExamplePath);
                            todo_wine
                            ok(dwAttributes==INVALID_FILE_ATTRIBUTES, "directory should not exist, but has attributes: 0x%08x\n", dwAttributes );


                            /* redirect (with copy) to original path */
                            hr = IKnownFolderManager_Redirect(mgr, &newFolderId, NULL, KF_REDIRECT_COPY_CONTENTS,  sExamplePath, 0, NULL, &errorMsg);
                            ok(hr == S_OK, "failed to redirect known folder: 0x%08x, errorMsg: %s\n", hr, wine_dbgstr_w(errorMsg));

                            /* verify */
                            hr = IKnownFolder_GetPath(folder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sExamplePath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sExamplePath));
                            CoTaskMemFree(folderPath);

                            /* verify sub folder */
                            hr = IKnownFolder_GetPath(subFolder, 0, &folderPath);
                            ok(hr == S_OK, "failed to get known folder path: 0x%08x\n", hr);
                            ok(lstrcmpiW(folderPath, sSubFolderPath)==0, "invalid known folder path retrieved: \"%s\" when \"%s\" was expected\n", wine_dbgstr_w(folderPath), wine_dbgstr_w(sSubFolderPath));
                            CoTaskMemFree(folderPath);

                            /* check shell utility functions */
                            if(!pSHGetKnownFolderPath || !pSHSetKnownFolderPath)
                                todo_wine
                                win_skip("cannot get SHGet/SetKnownFolderPath routines\n");
                            else
                            {
                                /* try to get current known folder path */
                                hr = pSHGetKnownFolderPath(&newFolderId, 0, NULL, &folderPath);
                                todo_wine
                                ok(hr==S_OK, "cannot get known folder path: hr=0x%0x\n", hr);
                                todo_wine
                                ok(lstrcmpW(folderPath, sExamplePath)==0, "invalid path returned: %s\n", wine_dbgstr_w(folderPath));

                                /* set it to new value */
                                hr = pSHSetKnownFolderPath(&newFolderId, 0, NULL, sExample2Path);
                                todo_wine
                                ok(hr==S_OK, "cannot set known folder path: hr=0x%0x\n", hr);

                                /* check if it changed */
                                hr = pSHGetKnownFolderPath(&newFolderId, 0, NULL, &folderPath);
                                todo_wine
                                ok(hr==S_OK, "cannot get known folder path: hr=0x%0x\n", hr);
                                todo_wine
                                ok(lstrcmpW(folderPath, sExample2Path)==0, "invalid path returned: %s\n", wine_dbgstr_w(folderPath));

                                /* set it back */
                                hr = pSHSetKnownFolderPath(&newFolderId, 0, NULL, sExamplePath);
                                todo_wine
                                ok(hr==S_OK, "cannot set known folder path: hr=0x%0x\n", hr);
                            }

                            IKnownFolder_Release(subFolder);
                        }

                        hr = IKnownFolderManager_UnregisterFolder(mgr, &subFolderId);
                        ok(hr == S_OK, "failed to unregister folder: 0x%08x\n", hr);
                    }

                    FreeKnownFolderDefinitionFields(&kfSubDefinition);

                    hr = IKnownFolder_Release(folder);
                    ok(hr == S_OK, "failed to release KnownFolder instance: 0x%08x\n", hr);
                }

                hr = IKnownFolderManager_UnregisterFolder(mgr, &newFolderId);
                ok(hr == S_OK, "failed to unregister folder: 0x%08x\n", hr);
            }
        }
        FreeKnownFolderDefinitionFields(&kfDefinition);

        RemoveDirectoryW(sSubFolder2Path);
        RemoveDirectoryW(sSubFolderPath);
        RemoveDirectoryW(sExamplePath);
        RemoveDirectoryW(sExample2Path);

        hr = IKnownFolderManager_Release(mgr);
        ok(hr == S_OK, "failed to release KnownFolderManager instance: 0x%08x\n", hr);
    }
    CoUninitialize();
}

START_TEST(shellpath)
{
    if (!init()) return;

    loadShell32();
    pGetSystemWow64DirectoryA = (void *)GetProcAddress( GetModuleHandleA("kernel32.dll"),
                                                        "GetSystemWow64DirectoryA" );
    if (myARGC >= 3)
        doChild(myARGV[2]);
    else
    {
        /* Report missing functions once */
        if (!pSHGetFolderLocation)
            win_skip("SHGetFolderLocation is not available\n");

        /* first test various combinations of parameters: */
        test_parameters();

        /* check known values: */
        test_PidlTypes();
        test_GUIDs();
        test_EnvVars();
        testWinDir();
        testSystemDir();
        test_NonExistentPath();
        test_SHGetFolderPathEx();
        test_knownFolders();
    }
}
