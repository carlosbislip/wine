/*		DirectInput
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998,1999 Lionel Ulmer
 * Copyright 2000-2002 TransGaming Technologies Inc.
 * Copyright 2007 Vitaliy Margolen
 *
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
 */
/* Status:
 *
 * - Tomb Raider 2 Demo:
 *   Playable using keyboard only.
 * - WingCommander Prophecy Demo:
 *   Doesn't get Input Focus.
 *
 * - Fallout : works great in X and DGA mode
 */

#include "config.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>

#define COBJMACROS
#define NONAMELESSUNION

#include "wine/debug.h"
#include "wine/unicode.h"
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "objbase.h"
#include "rpcproxy.h"
#include "dinput_private.h"
#include "device_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

static const IDirectInput7AVtbl ddi7avt;
static const IDirectInput7WVtbl ddi7wvt;
static const IDirectInput8AVtbl ddi8avt;
static const IDirectInput8WVtbl ddi8wvt;

static inline IDirectInputImpl *impl_from_IDirectInput7A( IDirectInput7A *iface )
{
    return CONTAINING_RECORD( iface, IDirectInputImpl, IDirectInput7A_iface );
}

static inline IDirectInputImpl *impl_from_IDirectInput7W( IDirectInput7W *iface )
{
    return CONTAINING_RECORD( iface, IDirectInputImpl, IDirectInput7W_iface );
}

static inline IDirectInputImpl *impl_from_IDirectInput8A( IDirectInput8A *iface )
{
    return CONTAINING_RECORD( iface, IDirectInputImpl, IDirectInput8A_iface );
}

static inline IDirectInputImpl *impl_from_IDirectInput8W( IDirectInput8W *iface )
{
    return CONTAINING_RECORD( iface, IDirectInputImpl, IDirectInput8W_iface );
}

static inline IDirectInputDeviceImpl *impl_from_IDirectInputDevice8A(IDirectInputDevice8A *iface)
{
    return CONTAINING_RECORD(iface, IDirectInputDeviceImpl, IDirectInputDevice8A_iface);
}
static inline IDirectInputDeviceImpl *impl_from_IDirectInputDevice8W(IDirectInputDevice8W *iface)
{
    return CONTAINING_RECORD(iface, IDirectInputDeviceImpl, IDirectInputDevice8W_iface);
}

static const struct dinput_device *dinput_devices[] =
{
    &mouse_device,
    &keyboard_device,
    &joystick_linuxinput_device,
    &joystick_linux_device,
    &joystick_osx_device
};
#define NB_DINPUT_DEVICES (sizeof(dinput_devices)/sizeof(dinput_devices[0]))

static HINSTANCE DINPUT_instance = NULL;

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserv)
{
    switch(reason)
    {
      case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(inst);
        DINPUT_instance = inst;
        break;
      case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

static BOOL check_hook_thread(void);
static CRITICAL_SECTION dinput_hook_crit;
static struct list direct_input_list = LIST_INIT( direct_input_list );

/******************************************************************************
 *	DirectInputCreateEx (DINPUT.@)
 */
HRESULT WINAPI DirectInputCreateEx(
	HINSTANCE hinst, DWORD dwVersion, REFIID riid, LPVOID *ppDI,
	LPUNKNOWN punkOuter) 
{
    IDirectInputImpl* This;

    TRACE("(%p,%04x,%s,%p,%p)\n", hinst, dwVersion, debugstr_guid(riid), ppDI, punkOuter);

    if (IsEqualGUID( &IID_IUnknown,       riid ) ||
        IsEqualGUID( &IID_IDirectInputA,  riid ) ||
        IsEqualGUID( &IID_IDirectInput2A, riid ) ||
        IsEqualGUID( &IID_IDirectInput7A, riid ) ||
        IsEqualGUID( &IID_IDirectInputW,  riid ) ||
        IsEqualGUID( &IID_IDirectInput2W, riid ) ||
        IsEqualGUID( &IID_IDirectInput7W, riid ) ||
        IsEqualGUID( &IID_IDirectInput8A, riid ) ||
        IsEqualGUID( &IID_IDirectInput8W, riid ))
    {
        if (!(This = HeapAlloc( GetProcessHeap(), 0, sizeof(IDirectInputImpl) )))
            return DIERR_OUTOFMEMORY;
    }
    else
        return DIERR_OLDDIRECTINPUTVERSION;

    This->IDirectInput7A_iface.lpVtbl = &ddi7avt;
    This->IDirectInput7W_iface.lpVtbl = &ddi7wvt;
    This->IDirectInput8A_iface.lpVtbl = &ddi8avt;
    This->IDirectInput8W_iface.lpVtbl = &ddi8wvt;
    This->ref         = 0;
    This->dwVersion   = dwVersion;
    This->evsequence  = 1;

    InitializeCriticalSection(&This->crit);
    This->crit.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": IDirectInputImpl*->crit");

    list_init( &This->devices_list );

    /* Add self to the list of the IDirectInputs */
    EnterCriticalSection( &dinput_hook_crit );
    list_add_head( &direct_input_list, &This->entry );
    LeaveCriticalSection( &dinput_hook_crit );

    if (!check_hook_thread())
    {
        IUnknown_Release( &This->IDirectInput7A_iface );
        return DIERR_GENERIC;
    }

    IDirectInput_QueryInterface( &This->IDirectInput7A_iface, riid, ppDI );
    return DI_OK;
}

/******************************************************************************
 *	DirectInputCreateA (DINPUT.@)
 */
HRESULT WINAPI DECLSPEC_HOTPATCH DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter)
{
    return DirectInputCreateEx(hinst, dwVersion, &IID_IDirectInput7A, (LPVOID *)ppDI, punkOuter);
}

/******************************************************************************
 *	DirectInputCreateW (DINPUT.@)
 */
HRESULT WINAPI DECLSPEC_HOTPATCH DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW *ppDI, LPUNKNOWN punkOuter)
{
    return DirectInputCreateEx(hinst, dwVersion, &IID_IDirectInput7W, (LPVOID *)ppDI, punkOuter);
}

static const char *_dump_DIDEVTYPE_value(DWORD dwDevType) {
    switch (dwDevType) {
        case 0: return "All devices";
	case DIDEVTYPE_MOUSE: return "DIDEVTYPE_MOUSE";
	case DIDEVTYPE_KEYBOARD: return "DIDEVTYPE_KEYBOARD";
	case DIDEVTYPE_JOYSTICK: return "DIDEVTYPE_JOYSTICK";
	case DIDEVTYPE_DEVICE: return "DIDEVTYPE_DEVICE";
	default: return "Unknown";
    }
}

static void _dump_EnumDevices_dwFlags(DWORD dwFlags) {
    if (TRACE_ON(dinput)) {
	unsigned int   i;
	static const struct {
	    DWORD       mask;
	    const char  *name;
	} flags[] = {
#define FE(x) { x, #x}
	    FE(DIEDFL_ALLDEVICES),
	    FE(DIEDFL_ATTACHEDONLY),
	    FE(DIEDFL_FORCEFEEDBACK),
	    FE(DIEDFL_INCLUDEALIASES),
	    FE(DIEDFL_INCLUDEPHANTOMS)
#undef FE
	};
	TRACE(" flags: ");
	if (dwFlags == 0) {
	    TRACE("DIEDFL_ALLDEVICES\n");
	    return;
	}
	for (i = 0; i < (sizeof(flags) / sizeof(flags[0])); i++)
	    if (flags[i].mask & dwFlags)
		TRACE("%s ",flags[i].name);
    }
    TRACE("\n");
}

void _dump_diactionformatA(LPDIACTIONFORMATA lpdiActionFormat) {
    unsigned int i;

    FIXME("diaf.dwSize = %d\n", lpdiActionFormat->dwSize);
    FIXME("diaf.dwActionSize = %d\n", lpdiActionFormat->dwActionSize);
    FIXME("diaf.dwDataSize = %d\n", lpdiActionFormat->dwDataSize);
    FIXME("diaf.dwNumActions = %d\n", lpdiActionFormat->dwNumActions);
    FIXME("diaf.rgoAction = %p\n", lpdiActionFormat->rgoAction);
    FIXME("diaf.guidActionMap = %s\n", debugstr_guid(&lpdiActionFormat->guidActionMap));
    FIXME("diaf.dwGenre = 0x%08x\n", lpdiActionFormat->dwGenre);
    FIXME("diaf.dwBufferSize = %d\n", lpdiActionFormat->dwBufferSize);
    FIXME("diaf.lAxisMin = %d\n", lpdiActionFormat->lAxisMin);
    FIXME("diaf.lAxisMax = %d\n", lpdiActionFormat->lAxisMax);
    FIXME("diaf.hInstString = %p\n", lpdiActionFormat->hInstString);
    FIXME("diaf.ftTimeStamp ...\n");
    FIXME("diaf.dwCRC = 0x%x\n", lpdiActionFormat->dwCRC);
    FIXME("diaf.tszActionMap = %s\n", debugstr_a(lpdiActionFormat->tszActionMap));
    for (i = 0; i < lpdiActionFormat->dwNumActions; i++)
    {
        FIXME("diaf.rgoAction[%u]:\n", i);
        FIXME("\tuAppData=0x%lx\n", lpdiActionFormat->rgoAction[i].uAppData);
        FIXME("\tdwSemantic=0x%08x\n", lpdiActionFormat->rgoAction[i].dwSemantic);
        FIXME("\tdwFlags=0x%x\n", lpdiActionFormat->rgoAction[i].dwFlags);
        FIXME("\tszActionName=%s\n", debugstr_a(lpdiActionFormat->rgoAction[i].u.lptszActionName));
        FIXME("\tguidInstance=%s\n", debugstr_guid(&lpdiActionFormat->rgoAction[i].guidInstance));
        FIXME("\tdwObjID=0x%x\n", lpdiActionFormat->rgoAction[i].dwObjID);
        FIXME("\tdwHow=0x%x\n", lpdiActionFormat->rgoAction[i].dwHow);
    }
}

void _copy_diactionformatAtoW(LPDIACTIONFORMATW to, LPDIACTIONFORMATA from)
{
    int i;

    to->dwSize = sizeof(DIACTIONFORMATW);
    to->dwActionSize = sizeof(DIACTIONW);
    to->dwDataSize = from->dwDataSize;
    to->dwNumActions = from->dwNumActions;
    to->guidActionMap = from->guidActionMap;
    to->dwGenre = from->dwGenre;
    to->dwBufferSize = from->dwBufferSize;
    to->lAxisMin = from->lAxisMin;
    to->lAxisMax = from->lAxisMax;
    to->dwCRC = from->dwCRC;
    to->ftTimeStamp = from->ftTimeStamp;

    for (i=0; i < to->dwNumActions; i++)
    {
        to->rgoAction[i].uAppData = from->rgoAction[i].uAppData;
        to->rgoAction[i].dwSemantic = from->rgoAction[i].dwSemantic;
        to->rgoAction[i].dwFlags = from->rgoAction[i].dwFlags;
        to->rgoAction[i].guidInstance = from->rgoAction[i].guidInstance;
        to->rgoAction[i].dwObjID = from->rgoAction[i].dwObjID;
        to->rgoAction[i].dwHow = from->rgoAction[i].dwHow;
    }
}

void _copy_diactionformatWtoA(LPDIACTIONFORMATA to, LPDIACTIONFORMATW from)
{
    int i;

    to->dwSize = sizeof(DIACTIONFORMATA);
    to->dwActionSize = sizeof(DIACTIONA);
    to->dwDataSize = from->dwDataSize;
    to->dwNumActions = from->dwNumActions;
    to->guidActionMap = from->guidActionMap;
    to->dwGenre = from->dwGenre;
    to->dwBufferSize = from->dwBufferSize;
    to->lAxisMin = from->lAxisMin;
    to->lAxisMax = from->lAxisMax;
    to->dwCRC = from->dwCRC;
    to->ftTimeStamp = from->ftTimeStamp;

    for (i=0; i < to->dwNumActions; i++)
    {
        to->rgoAction[i].uAppData = from->rgoAction[i].uAppData;
        to->rgoAction[i].dwSemantic = from->rgoAction[i].dwSemantic;
        to->rgoAction[i].dwFlags = from->rgoAction[i].dwFlags;
        to->rgoAction[i].guidInstance = from->rgoAction[i].guidInstance;
        to->rgoAction[i].dwObjID = from->rgoAction[i].dwObjID;
        to->rgoAction[i].dwHow = from->rgoAction[i].dwHow;
    }
}

/******************************************************************************
 *	IDirectInputA_EnumDevices
 */
static HRESULT WINAPI IDirectInputAImpl_EnumDevices(
	LPDIRECTINPUT7A iface, DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback,
	LPVOID pvRef, DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A(iface);
    DIDEVICEINSTANCEA devInstance;
    unsigned int i;
    int j, r;

    TRACE("(this=%p,0x%04x '%s',%p,%p,%04x)\n",
	  This, dwDevType, _dump_DIDEVTYPE_value(dwDevType),
	  lpCallback, pvRef, dwFlags);
    _dump_EnumDevices_dwFlags(dwFlags);

    for (i = 0; i < NB_DINPUT_DEVICES; i++) {
        if (!dinput_devices[i]->enum_deviceA) continue;
        for (j = 0, r = -1; r != 0; j++) {
	    devInstance.dwSize = sizeof(devInstance);
	    TRACE("  - checking device %u ('%s')\n", i, dinput_devices[i]->name);
	    if ((r = dinput_devices[i]->enum_deviceA(dwDevType, dwFlags, &devInstance, This->dwVersion, j))) {
	        if (lpCallback(&devInstance,pvRef) == DIENUM_STOP)
		    return 0;
	    }
	}
    }
    
    return 0;
}
/******************************************************************************
 *	IDirectInputW_EnumDevices
 */
static HRESULT WINAPI IDirectInputWImpl_EnumDevices(
	LPDIRECTINPUT7W iface, DWORD dwDevType, LPDIENUMDEVICESCALLBACKW lpCallback,
	LPVOID pvRef, DWORD dwFlags) 
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    DIDEVICEINSTANCEW devInstance;
    unsigned int i;
    int j, r;

    TRACE("(this=%p,0x%04x '%s',%p,%p,%04x)\n",
	  This, dwDevType, _dump_DIDEVTYPE_value(dwDevType),
	  lpCallback, pvRef, dwFlags);
    _dump_EnumDevices_dwFlags(dwFlags);

    for (i = 0; i < NB_DINPUT_DEVICES; i++) {
        if (!dinput_devices[i]->enum_deviceW) continue;
        for (j = 0, r = -1; r != 0; j++) {
	    devInstance.dwSize = sizeof(devInstance);
	    TRACE("  - checking device %u ('%s')\n", i, dinput_devices[i]->name);
	    if ((r = dinput_devices[i]->enum_deviceW(dwDevType, dwFlags, &devInstance, This->dwVersion, j))) {
	        if (lpCallback(&devInstance,pvRef) == DIENUM_STOP)
		    return 0;
	    }
	}
    }
    
    return 0;
}

static ULONG WINAPI IDirectInputAImpl_AddRef(LPDIRECTINPUT7A iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A( iface );
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE( "(%p) incrementing from %d\n", This, ref - 1);
    return ref;
}

static ULONG WINAPI IDirectInputWImpl_AddRef(LPDIRECTINPUT7W iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    return IDirectInputAImpl_AddRef( &This->IDirectInput7A_iface );
}

static ULONG WINAPI IDirectInputAImpl_Release(LPDIRECTINPUT7A iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) releasing from %d\n", This, ref + 1 );

    if (ref) return ref;

    /* Remove self from the list of the IDirectInputs */
    EnterCriticalSection( &dinput_hook_crit );
    list_remove( &This->entry );
    LeaveCriticalSection( &dinput_hook_crit );

    check_hook_thread();

    This->crit.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &This->crit );
    HeapFree( GetProcessHeap(), 0, This );

    return 0;
}

static ULONG WINAPI IDirectInputWImpl_Release(LPDIRECTINPUT7W iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    return IDirectInputAImpl_Release( &This->IDirectInput7A_iface );
}

static HRESULT WINAPI IDirectInputAImpl_QueryInterface(LPDIRECTINPUT7A iface, REFIID riid, LPVOID *ppobj)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A( iface );

    TRACE( "(%p)->(%s,%p)\n", This, debugstr_guid(riid), ppobj );

    if (IsEqualGUID( &IID_IUnknown, riid ) ||
        IsEqualGUID( &IID_IDirectInputA,  riid ) ||
        IsEqualGUID( &IID_IDirectInput2A, riid ) ||
        IsEqualGUID( &IID_IDirectInput7A, riid ))
    {
        *ppobj = &This->IDirectInput7A_iface;
        IUnknown_AddRef( (IUnknown*)*ppobj );

        return DI_OK;
    }

    if (IsEqualGUID( &IID_IDirectInputW,  riid ) ||
        IsEqualGUID( &IID_IDirectInput2W, riid ) ||
        IsEqualGUID( &IID_IDirectInput7W, riid ))
    {
        *ppobj = &This->IDirectInput7W_iface;
        IUnknown_AddRef( (IUnknown*)*ppobj );

        return DI_OK;
    }

    if (IsEqualGUID( &IID_IDirectInput8A, riid ))
    {
        *ppobj = &This->IDirectInput8A_iface;
        IUnknown_AddRef( (IUnknown*)*ppobj );

        return DI_OK;
    }

    if (IsEqualGUID( &IID_IDirectInput8W, riid ))
    {
        *ppobj = &This->IDirectInput8W_iface;
        IUnknown_AddRef( (IUnknown*)*ppobj );

        return DI_OK;
    }

    FIXME( "Unsupported interface: %s\n", debugstr_guid(riid));
    return E_FAIL;
}

static HRESULT WINAPI IDirectInputWImpl_QueryInterface(LPDIRECTINPUT7W iface, REFIID riid, LPVOID *ppobj)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    return IDirectInputAImpl_QueryInterface( &This->IDirectInput7A_iface, riid, ppobj );
}

static HRESULT WINAPI IDirectInputAImpl_Initialize(LPDIRECTINPUT7A iface, HINSTANCE hinst, DWORD x) {
	TRACE("(this=%p,%p,%x)\n",iface, hinst, x);
	
	/* Initialize can return: DIERR_BETADIRECTINPUTVERSION, DIERR_OLDDIRECTINPUTVERSION and DI_OK.
	 * Since we already initialized the device, return DI_OK. In the past we returned DIERR_ALREADYINITIALIZED
	 * which broke applications like Tomb Raider Legend because it isn't a legal return value.
	 */
	return DI_OK;
}

static HRESULT WINAPI IDirectInputWImpl_Initialize(LPDIRECTINPUT7W iface, HINSTANCE hinst, DWORD x)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    return IDirectInputAImpl_Initialize( &This->IDirectInput7A_iface, hinst, x );
}

static HRESULT WINAPI IDirectInputAImpl_GetDeviceStatus(LPDIRECTINPUT7A iface, REFGUID rguid)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A( iface );
    HRESULT hr;
    LPDIRECTINPUTDEVICEA device;

    TRACE( "(%p)->(%s)\n", This, debugstr_guid(rguid) );

    hr = IDirectInput_CreateDevice( iface, rguid, &device, NULL );
    if (hr != DI_OK) return DI_NOTATTACHED;

    IUnknown_Release( device );

    return DI_OK;
}

static HRESULT WINAPI IDirectInputWImpl_GetDeviceStatus(LPDIRECTINPUT7W iface, REFGUID rguid)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    return IDirectInputAImpl_GetDeviceStatus( &This->IDirectInput7A_iface, rguid );
}

static HRESULT WINAPI IDirectInputAImpl_RunControlPanel(LPDIRECTINPUT7A iface,
							HWND hwndOwner,
							DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A( iface );

    FIXME( "(%p)->(%p,%08x): stub\n", This, hwndOwner, dwFlags );

    return DI_OK;
}

static HRESULT WINAPI IDirectInputWImpl_RunControlPanel(LPDIRECTINPUT7W iface, HWND hwndOwner, DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
    return IDirectInputAImpl_RunControlPanel( &This->IDirectInput7A_iface, hwndOwner, dwFlags );
}

static HRESULT WINAPI IDirectInput2AImpl_FindDevice(LPDIRECTINPUT7A iface, REFGUID rguid,
						    LPCSTR pszName, LPGUID pguidInstance)
{
    IDirectInputImpl *This = impl_from_IDirectInput7A( iface );

    FIXME( "(%p)->(%s, %s, %p): stub\n", This, debugstr_guid(rguid), pszName, pguidInstance );

    return DI_OK;
}

static HRESULT WINAPI IDirectInput2WImpl_FindDevice(LPDIRECTINPUT7W iface, REFGUID rguid,
						    LPCWSTR pszName, LPGUID pguidInstance)
{
    IDirectInputImpl *This = impl_from_IDirectInput7W( iface );

    FIXME( "(%p)->(%s, %s, %p): stub\n", This, debugstr_guid(rguid), debugstr_w(pszName), pguidInstance );

    return DI_OK;
}

static HRESULT WINAPI IDirectInput7AImpl_CreateDeviceEx(LPDIRECTINPUT7A iface, REFGUID rguid,
							REFIID riid, LPVOID* pvOut, LPUNKNOWN lpUnknownOuter)
{
  IDirectInputImpl *This = impl_from_IDirectInput7A( iface );
  HRESULT ret_value = DIERR_DEVICENOTREG;
  unsigned int i;

  TRACE("(%p)->(%s, %s, %p, %p)\n", This, debugstr_guid(rguid), debugstr_guid(riid), pvOut, lpUnknownOuter);

  if (!rguid || !pvOut) return E_POINTER;

  /* Loop on all the devices to see if anyone matches the given GUID */
  for (i = 0; i < NB_DINPUT_DEVICES; i++) {
    HRESULT ret;

    if (!dinput_devices[i]->create_device) continue;
    if ((ret = dinput_devices[i]->create_device(This, rguid, riid, pvOut, 0)) == DI_OK)
      return DI_OK;

    if (ret == DIERR_NOINTERFACE)
      ret_value = DIERR_NOINTERFACE;
  }

  if (ret_value == DIERR_NOINTERFACE)
  {
    WARN("invalid device GUID %s\n", debugstr_guid(rguid));
  }

  return ret_value;
}

static HRESULT WINAPI IDirectInput7WImpl_CreateDeviceEx(LPDIRECTINPUT7W iface, REFGUID rguid,
							REFIID riid, LPVOID* pvOut, LPUNKNOWN lpUnknownOuter)
{
  IDirectInputImpl *This = impl_from_IDirectInput7W( iface );
  HRESULT ret_value = DIERR_DEVICENOTREG;
  unsigned int i;

  TRACE("(%p)->(%s, %s, %p, %p)\n", This, debugstr_guid(rguid), debugstr_guid(riid), pvOut, lpUnknownOuter);

  if (!rguid || !pvOut) return E_POINTER;

  /* Loop on all the devices to see if anyone matches the given GUID */
  for (i = 0; i < NB_DINPUT_DEVICES; i++) {
    HRESULT ret;

    if (!dinput_devices[i]->create_device) continue;
    if ((ret = dinput_devices[i]->create_device(This, rguid, riid, pvOut, 1)) == DI_OK)
      return DI_OK;

    if (ret == DIERR_NOINTERFACE)
      ret_value = DIERR_NOINTERFACE;
  }

  return ret_value;
}

static HRESULT WINAPI IDirectInputAImpl_CreateDevice(LPDIRECTINPUT7A iface, REFGUID rguid,
                                                     LPDIRECTINPUTDEVICEA* pdev, LPUNKNOWN punk)
{
    return IDirectInput7AImpl_CreateDeviceEx(iface, rguid, NULL, (LPVOID*)pdev, punk);
}

static HRESULT WINAPI IDirectInputWImpl_CreateDevice(LPDIRECTINPUT7W iface, REFGUID rguid,
                                                     LPDIRECTINPUTDEVICEW* pdev, LPUNKNOWN punk)
{
    return IDirectInput7WImpl_CreateDeviceEx(iface, rguid, NULL, (LPVOID*)pdev, punk);
}

/*******************************************************************************
 *      DirectInput8
 */

static ULONG WINAPI IDirectInput8AImpl_AddRef(LPDIRECTINPUT8A iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_AddRef( &This->IDirectInput7A_iface );
}

static ULONG WINAPI IDirectInput8WImpl_AddRef(LPDIRECTINPUT8W iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputAImpl_AddRef( &This->IDirectInput7A_iface );
}

static HRESULT WINAPI IDirectInput8AImpl_QueryInterface(LPDIRECTINPUT8A iface, REFIID riid, LPVOID *ppobj)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_QueryInterface( &This->IDirectInput7A_iface, riid, ppobj );
}

static HRESULT WINAPI IDirectInput8WImpl_QueryInterface(LPDIRECTINPUT8W iface, REFIID riid, LPVOID *ppobj)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputAImpl_QueryInterface( &This->IDirectInput7A_iface, riid, ppobj );
}

static ULONG WINAPI IDirectInput8AImpl_Release(LPDIRECTINPUT8A iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_Release( &This->IDirectInput7A_iface );
}

static ULONG WINAPI IDirectInput8WImpl_Release(LPDIRECTINPUT8W iface)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputAImpl_Release( &This->IDirectInput7A_iface );
}

static HRESULT WINAPI IDirectInput8AImpl_CreateDevice(LPDIRECTINPUT8A iface, REFGUID rguid,
                                                      LPDIRECTINPUTDEVICE8A* pdev, LPUNKNOWN punk)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInput7AImpl_CreateDeviceEx( &This->IDirectInput7A_iface, rguid, NULL, (LPVOID*)pdev, punk );
}

static HRESULT WINAPI IDirectInput8WImpl_CreateDevice(LPDIRECTINPUT8W iface, REFGUID rguid,
                                                      LPDIRECTINPUTDEVICE8W* pdev, LPUNKNOWN punk)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInput7WImpl_CreateDeviceEx( &This->IDirectInput7W_iface, rguid, NULL, (LPVOID*)pdev, punk );
}

static HRESULT WINAPI IDirectInput8AImpl_EnumDevices(LPDIRECTINPUT8A iface, DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback,
                                                     LPVOID pvRef, DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_EnumDevices( &This->IDirectInput7A_iface, dwDevType, lpCallback, pvRef, dwFlags );
}

static HRESULT WINAPI IDirectInput8WImpl_EnumDevices(LPDIRECTINPUT8W iface, DWORD dwDevType, LPDIENUMDEVICESCALLBACKW lpCallback,
                                                     LPVOID pvRef, DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputWImpl_EnumDevices( &This->IDirectInput7W_iface, dwDevType, lpCallback, pvRef, dwFlags );
}

static HRESULT WINAPI IDirectInput8AImpl_GetDeviceStatus(LPDIRECTINPUT8A iface, REFGUID rguid)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_GetDeviceStatus( &This->IDirectInput7A_iface, rguid );
}

static HRESULT WINAPI IDirectInput8WImpl_GetDeviceStatus(LPDIRECTINPUT8W iface, REFGUID rguid)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputAImpl_GetDeviceStatus( &This->IDirectInput7A_iface, rguid );
}

static HRESULT WINAPI IDirectInput8AImpl_RunControlPanel(LPDIRECTINPUT8A iface, HWND hwndOwner, DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_RunControlPanel( &This->IDirectInput7A_iface, hwndOwner, dwFlags );
}

static HRESULT WINAPI IDirectInput8WImpl_RunControlPanel(LPDIRECTINPUT8W iface, HWND hwndOwner, DWORD dwFlags)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputAImpl_RunControlPanel( &This->IDirectInput7A_iface, hwndOwner, dwFlags );
}

static HRESULT WINAPI IDirectInput8AImpl_Initialize(LPDIRECTINPUT8A iface, HINSTANCE hinst, DWORD x)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInputAImpl_Initialize( &This->IDirectInput7A_iface, hinst, x );
}

static HRESULT WINAPI IDirectInput8WImpl_Initialize(LPDIRECTINPUT8W iface, HINSTANCE hinst, DWORD x)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInputAImpl_Initialize( &This->IDirectInput7A_iface, hinst, x );
}

static HRESULT WINAPI IDirectInput8AImpl_FindDevice(LPDIRECTINPUT8A iface, REFGUID rguid, LPCSTR pszName, LPGUID pguidInstance)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    return IDirectInput2AImpl_FindDevice( &This->IDirectInput7A_iface, rguid, pszName, pguidInstance );
}

static HRESULT WINAPI IDirectInput8WImpl_FindDevice(LPDIRECTINPUT8W iface, REFGUID rguid, LPCWSTR pszName, LPGUID pguidInstance)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W( iface );
    return IDirectInput2WImpl_FindDevice( &This->IDirectInput7W_iface, rguid, pszName, pguidInstance );
}

static HRESULT WINAPI IDirectInput8AImpl_EnumDevicesBySemantics(
      LPDIRECTINPUT8A iface, LPCSTR ptszUserName, LPDIACTIONFORMATA lpdiActionFormat,
      LPDIENUMDEVICESBYSEMANTICSCBA lpCallback,
      LPVOID pvRef, DWORD dwFlags
)
{
    IDirectInputImpl *This = impl_from_IDirectInput8A( iface );
    DIDEVICEINSTANCEA didevi;
    LPDIRECTINPUTDEVICE8A lpdid;
    BOOL ret;
    DWORD callbackFlags = 0;
    int i;

    FIXME("(this=%p,%s,%p,%p,%p,%04x): semi-stub\n", This, ptszUserName, lpdiActionFormat,
          lpCallback, pvRef, dwFlags);
#define X(x) if (dwFlags & x) FIXME("\tdwFlags |= "#x"\n");
	X(DIEDBSFL_ATTACHEDONLY)
	X(DIEDBSFL_THISUSER)
	X(DIEDBSFL_FORCEFEEDBACK)
	X(DIEDBSFL_AVAILABLEDEVICES)
	X(DIEDBSFL_MULTIMICEKEYBOARDS)
	X(DIEDBSFL_NONGAMINGDEVICES)
#undef X

    _dump_diactionformatA(lpdiActionFormat);

    didevi.dwSize = sizeof(didevi);

    /* enum the keyboard first */
    IDirectInput_CreateDevice(iface, &GUID_SysKeyboard, &lpdid, NULL);
    IDirectInputDevice_GetDeviceInfo(lpdid, &didevi);
    /* if there's any DIKEYBOARD action, keyboard is priority 1*/
    for(i=0; i < lpdiActionFormat->dwActionSize; i++)
        if ((lpdiActionFormat->rgoAction[i].dwSemantic & DIKEYBOARD_MASK) == DIKEYBOARD_MASK)
            callbackFlags |= DIEDBS_MAPPEDPRI1;

    ret = lpCallback(&didevi, lpdid, callbackFlags, 1, pvRef);

    if (ret == DIENUM_STOP) return DI_OK;

    callbackFlags = 0;

    /* and then the mouse */
    IDirectInput_CreateDevice(iface, &GUID_SysMouse, &lpdid, NULL);
    IDirectInputDevice_GetDeviceInfo(lpdid, &didevi);
    /* same as above, but for the mouse */
    for(i=0; i < lpdiActionFormat->dwActionSize; i++)
        if ((lpdiActionFormat->rgoAction[i].dwSemantic & DIMOUSE_MASK) == DIMOUSE_MASK)
            callbackFlags |= DIEDBS_MAPPEDPRI1;

    lpCallback(&didevi, lpdid, callbackFlags, 0, pvRef);

    return DI_OK;
}

static HRESULT WINAPI IDirectInput8WImpl_EnumDevicesBySemantics(
      LPDIRECTINPUT8W iface, LPCWSTR ptszUserName, LPDIACTIONFORMATW lpdiActionFormat,
      LPDIENUMDEVICESBYSEMANTICSCBW lpCallback,
      LPVOID pvRef, DWORD dwFlags
)
{
    IDirectInputImpl *This = impl_from_IDirectInput8W(iface);
    DIDEVICEINSTANCEW didevi;
    LPDIRECTINPUTDEVICE8W lpdid;
    BOOL ret;
    DWORD callbackFlags = 0;
    int i;

    FIXME("(this=%p,%s,%p,%p,%p,%04x): semi-stub\n", This, debugstr_w(ptszUserName), lpdiActionFormat,
          lpCallback, pvRef, dwFlags);

    didevi.dwSize = sizeof(didevi);

    /* enum the keyboard first */
    IDirectInput_CreateDevice(iface, &GUID_SysKeyboard, &lpdid, NULL);
    IDirectInputDevice_GetDeviceInfo(lpdid, &didevi);
    /* if there's any DIKEYBOARD action, keyboard is priority 1*/
    for(i=0; i < lpdiActionFormat->dwActionSize; i++)
        if ((lpdiActionFormat->rgoAction[i].dwSemantic & DIKEYBOARD_MASK) == DIKEYBOARD_MASK)
            callbackFlags |= DIEDBS_MAPPEDPRI1;

    ret = lpCallback(&didevi, lpdid, callbackFlags, 1, pvRef);

    if (ret == DIENUM_STOP) return DI_OK;

    callbackFlags = 0;

    /* and then the mouse */
    IDirectInput_CreateDevice(iface, &GUID_SysMouse, &lpdid, NULL);
    IDirectInputDevice_GetDeviceInfo(lpdid, &didevi);
    /* same as above, but for the mouse */
    for(i=0; i < lpdiActionFormat->dwActionSize; i++)
        if ((lpdiActionFormat->rgoAction[i].dwSemantic & DIMOUSE_MASK) == DIMOUSE_MASK)
            callbackFlags |= DIEDBS_MAPPEDPRI1;

    lpCallback(&didevi, lpdid, callbackFlags, 0, pvRef);

    return DI_OK;
}

static HRESULT WINAPI IDirectInput8AImpl_ConfigureDevices(
      LPDIRECTINPUT8A iface, LPDICONFIGUREDEVICESCALLBACK lpdiCallback,
      LPDICONFIGUREDEVICESPARAMSA lpdiCDParams, DWORD dwFlags, LPVOID pvRefData
)
{
      IDirectInputImpl *This = impl_from_IDirectInput8A( iface );

      FIXME("(this=%p,%p,%p,%04x,%p): stub\n", This, lpdiCallback, lpdiCDParams,
            dwFlags, pvRefData);
      return 0;
}

static HRESULT WINAPI IDirectInput8WImpl_ConfigureDevices(
      LPDIRECTINPUT8W iface, LPDICONFIGUREDEVICESCALLBACK lpdiCallback,
      LPDICONFIGUREDEVICESPARAMSW lpdiCDParams, DWORD dwFlags, LPVOID pvRefData
)
{
      IDirectInputImpl *This = impl_from_IDirectInput8W( iface );

      FIXME("(this=%p,%p,%p,%04x,%p): stub\n", This, lpdiCallback, lpdiCDParams,
            dwFlags, pvRefData);
      return 0;
}

static const IDirectInput7AVtbl ddi7avt = {
    IDirectInputAImpl_QueryInterface,
    IDirectInputAImpl_AddRef,
    IDirectInputAImpl_Release,
    IDirectInputAImpl_CreateDevice,
    IDirectInputAImpl_EnumDevices,
    IDirectInputAImpl_GetDeviceStatus,
    IDirectInputAImpl_RunControlPanel,
    IDirectInputAImpl_Initialize,
    IDirectInput2AImpl_FindDevice,
    IDirectInput7AImpl_CreateDeviceEx
};

static const IDirectInput7WVtbl ddi7wvt = {
    IDirectInputWImpl_QueryInterface,
    IDirectInputWImpl_AddRef,
    IDirectInputWImpl_Release,
    IDirectInputWImpl_CreateDevice,
    IDirectInputWImpl_EnumDevices,
    IDirectInputWImpl_GetDeviceStatus,
    IDirectInputWImpl_RunControlPanel,
    IDirectInputWImpl_Initialize,
    IDirectInput2WImpl_FindDevice,
    IDirectInput7WImpl_CreateDeviceEx
};

static const IDirectInput8AVtbl ddi8avt = {
    IDirectInput8AImpl_QueryInterface,
    IDirectInput8AImpl_AddRef,
    IDirectInput8AImpl_Release,
    IDirectInput8AImpl_CreateDevice,
    IDirectInput8AImpl_EnumDevices,
    IDirectInput8AImpl_GetDeviceStatus,
    IDirectInput8AImpl_RunControlPanel,
    IDirectInput8AImpl_Initialize,
    IDirectInput8AImpl_FindDevice,
    IDirectInput8AImpl_EnumDevicesBySemantics,
    IDirectInput8AImpl_ConfigureDevices
};

static const IDirectInput8WVtbl ddi8wvt = {
    IDirectInput8WImpl_QueryInterface,
    IDirectInput8WImpl_AddRef,
    IDirectInput8WImpl_Release,
    IDirectInput8WImpl_CreateDevice,
    IDirectInput8WImpl_EnumDevices,
    IDirectInput8WImpl_GetDeviceStatus,
    IDirectInput8WImpl_RunControlPanel,
    IDirectInput8WImpl_Initialize,
    IDirectInput8WImpl_FindDevice,
    IDirectInput8WImpl_EnumDevicesBySemantics,
    IDirectInput8WImpl_ConfigureDevices
};

/*******************************************************************************
 * DirectInput ClassFactory
 */
typedef struct
{
    /* IUnknown fields */
    IClassFactory IClassFactory_iface;
    LONG          ref;
} IClassFactoryImpl;

static inline IClassFactoryImpl *impl_from_IClassFactory(IClassFactory *iface)
{
        return CONTAINING_RECORD(iface, IClassFactoryImpl, IClassFactory_iface);
}

static HRESULT WINAPI DICF_QueryInterface(LPCLASSFACTORY iface,REFIID riid,LPVOID *ppobj) {
	IClassFactoryImpl *This = impl_from_IClassFactory(iface);

	FIXME("(%p)->(%s,%p),stub!\n",This,debugstr_guid(riid),ppobj);
	return E_NOINTERFACE;
}

static ULONG WINAPI DICF_AddRef(LPCLASSFACTORY iface) {
	IClassFactoryImpl *This = impl_from_IClassFactory(iface);
	return InterlockedIncrement(&(This->ref));
}

static ULONG WINAPI DICF_Release(LPCLASSFACTORY iface) {
	IClassFactoryImpl *This = impl_from_IClassFactory(iface);
	/* static class, won't be  freed */
	return InterlockedDecrement(&(This->ref));
}

static HRESULT WINAPI DICF_CreateInstance(
	LPCLASSFACTORY iface,LPUNKNOWN pOuter,REFIID riid,LPVOID *ppobj
) {
	IClassFactoryImpl *This = impl_from_IClassFactory(iface);

	TRACE("(%p)->(%p,%s,%p)\n",This,pOuter,debugstr_guid(riid),ppobj);
        if ( IsEqualGUID( &IID_IUnknown, riid ) ||
             IsEqualGUID( &IID_IDirectInputA, riid ) ||
	     IsEqualGUID( &IID_IDirectInputW, riid ) ||
	     IsEqualGUID( &IID_IDirectInput2A, riid ) ||
	     IsEqualGUID( &IID_IDirectInput2W, riid ) ||
	     IsEqualGUID( &IID_IDirectInput7A, riid ) ||
	     IsEqualGUID( &IID_IDirectInput7W, riid ) ||
	     IsEqualGUID( &IID_IDirectInput8A, riid ) ||
	     IsEqualGUID( &IID_IDirectInput8W, riid ) ) {
		/* FIXME: reuse already created dinput if present? */
		return DirectInputCreateEx(0,0,riid,ppobj,pOuter);
	}

	FIXME("(%p,%p,%s,%p) Interface not found!\n",This,pOuter,debugstr_guid(riid),ppobj);	
	return E_NOINTERFACE;
}

static HRESULT WINAPI DICF_LockServer(LPCLASSFACTORY iface,BOOL dolock) {
	IClassFactoryImpl *This = impl_from_IClassFactory(iface);
	FIXME("(%p)->(%d),stub!\n",This,dolock);
	return S_OK;
}

static const IClassFactoryVtbl DICF_Vtbl = {
	DICF_QueryInterface,
	DICF_AddRef,
	DICF_Release,
	DICF_CreateInstance,
	DICF_LockServer
};
static IClassFactoryImpl DINPUT_CF = {{&DICF_Vtbl}, 1 };

/***********************************************************************
 *		DllCanUnloadNow (DINPUT.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

/***********************************************************************
 *		DllGetClassObject (DINPUT.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    TRACE("(%s,%s,%p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    if ( IsEqualCLSID( &IID_IClassFactory, riid ) ) {
        *ppv = &DINPUT_CF;
	IClassFactory_AddRef((IClassFactory*)*ppv);
    return S_OK;
    }

    FIXME("(%s,%s,%p): no interface found.\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *		DllRegisterServer (DINPUT.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( DINPUT_instance, NULL );
}

/***********************************************************************
 *		DllUnregisterServer (DINPUT.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( DINPUT_instance, NULL );
}

/******************************************************************************
 *	DInput hook thread
 */

static LRESULT CALLBACK LL_hook_proc( int code, WPARAM wparam, LPARAM lparam )
{
    IDirectInputImpl *dinput;
    int skip = 0;

    if (code != HC_ACTION) return CallNextHookEx( 0, code, wparam, lparam );

    EnterCriticalSection( &dinput_hook_crit );
    LIST_FOR_EACH_ENTRY( dinput, &direct_input_list, IDirectInputImpl, entry )
    {
        IDirectInputDeviceImpl *dev;

        EnterCriticalSection( &dinput->crit );
        LIST_FOR_EACH_ENTRY( dev, &dinput->devices_list, IDirectInputDeviceImpl, entry )
            if (dev->acquired && dev->event_proc)
            {
                TRACE("calling %p->%p (%lx %lx)\n", dev, dev->event_proc, wparam, lparam);
                skip |= dev->event_proc( &dev->IDirectInputDevice8A_iface, wparam, lparam );
            }
        LeaveCriticalSection( &dinput->crit );
    }
    LeaveCriticalSection( &dinput_hook_crit );

    return skip ? 1 : CallNextHookEx( 0, code, wparam, lparam );
}

static LRESULT CALLBACK callwndproc_proc( int code, WPARAM wparam, LPARAM lparam )
{
    CWPSTRUCT *msg = (CWPSTRUCT *)lparam;
    IDirectInputImpl *dinput;
    HWND foreground;

    if (code != HC_ACTION || (msg->message != WM_KILLFOCUS &&
        msg->message != WM_ACTIVATEAPP && msg->message != WM_ACTIVATE))
        return CallNextHookEx( 0, code, wparam, lparam );

    foreground = GetForegroundWindow();

    EnterCriticalSection( &dinput_hook_crit );

    LIST_FOR_EACH_ENTRY( dinput, &direct_input_list, IDirectInputImpl, entry )
    {
        IDirectInputDeviceImpl *dev;

        EnterCriticalSection( &dinput->crit );
        LIST_FOR_EACH_ENTRY( dev, &dinput->devices_list, IDirectInputDeviceImpl, entry )
        {
            if (!dev->acquired) continue;

            if (msg->hwnd == dev->win && msg->hwnd != foreground)
            {
                TRACE( "%p window is not foreground - unacquiring %p\n", dev->win, dev );
                IDirectInputDevice_Unacquire( &dev->IDirectInputDevice8A_iface );
            }
        }
        LeaveCriticalSection( &dinput->crit );
    }
    LeaveCriticalSection( &dinput_hook_crit );

    return CallNextHookEx( 0, code, wparam, lparam );
}

static DWORD WINAPI hook_thread_proc(void *param)
{
    static HHOOK kbd_hook, mouse_hook;
    MSG msg;

    /* Force creation of the message queue */
    PeekMessageW( &msg, 0, 0, 0, PM_NOREMOVE );
    SetEvent(*(LPHANDLE)param);

    while (GetMessageW( &msg, 0, 0, 0 ))
    {
        UINT kbd_cnt = 0, mice_cnt = 0;

        if (msg.message == WM_USER+0x10)
        {
            IDirectInputImpl *dinput;

            TRACE( "Processing hook change notification lp:%ld\n", msg.lParam );

            if (!msg.wParam && !msg.lParam)
            {
                if (kbd_hook) UnhookWindowsHookEx( kbd_hook );
                if (mouse_hook) UnhookWindowsHookEx( mouse_hook );
                kbd_hook = mouse_hook = NULL;
                break;
            }

            EnterCriticalSection( &dinput_hook_crit );

            /* Count acquired keyboards and mice*/
            LIST_FOR_EACH_ENTRY( dinput, &direct_input_list, IDirectInputImpl, entry )
            {
                IDirectInputDeviceImpl *dev;

                EnterCriticalSection( &dinput->crit );
                LIST_FOR_EACH_ENTRY( dev, &dinput->devices_list, IDirectInputDeviceImpl, entry )
                {
                    if (!dev->acquired || !dev->event_proc) continue;

                    if (IsEqualGUID( &dev->guid, &GUID_SysKeyboard ) ||
                        IsEqualGUID( &dev->guid, &DInput_Wine_Keyboard_GUID ))
                        kbd_cnt++;
                    else
                        if (IsEqualGUID( &dev->guid, &GUID_SysMouse ) ||
                            IsEqualGUID( &dev->guid, &DInput_Wine_Mouse_GUID ))
                            mice_cnt++;
                }
                LeaveCriticalSection( &dinput->crit );
            }
            LeaveCriticalSection( &dinput_hook_crit );

            if (kbd_cnt && !kbd_hook)
                kbd_hook = SetWindowsHookExW( WH_KEYBOARD_LL, LL_hook_proc, DINPUT_instance, 0 );
            else if (!kbd_cnt && kbd_hook)
            {
                UnhookWindowsHookEx( kbd_hook );
                kbd_hook = NULL;
            }

            if (mice_cnt && !mouse_hook)
                mouse_hook = SetWindowsHookExW( WH_MOUSE_LL, LL_hook_proc, DINPUT_instance, 0 );
            else if (!mice_cnt && mouse_hook)
            {
                UnhookWindowsHookEx( mouse_hook );
                mouse_hook = NULL;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

static DWORD hook_thread_id;

static CRITICAL_SECTION_DEBUG dinput_critsect_debug =
{
    0, 0, &dinput_hook_crit,
    { &dinput_critsect_debug.ProcessLocksList, &dinput_critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": dinput_hook_crit") }
};
static CRITICAL_SECTION dinput_hook_crit = { &dinput_critsect_debug, -1, 0, 0, 0, 0 };

static BOOL check_hook_thread(void)
{
    static HANDLE hook_thread;

    EnterCriticalSection(&dinput_hook_crit);

    TRACE("IDirectInputs left: %d\n", list_count(&direct_input_list));
    if (!list_empty(&direct_input_list) && !hook_thread)
    {
        HANDLE event;

        event = CreateEventW(NULL, FALSE, FALSE, NULL);
        hook_thread = CreateThread(NULL, 0, hook_thread_proc, &event, 0, &hook_thread_id);
        if (event && hook_thread)
        {
            HANDLE handles[2];
            handles[0] = event;
            handles[1] = hook_thread;
            WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        }
        LeaveCriticalSection(&dinput_hook_crit);
        CloseHandle(event);
    }
    else if (list_empty(&direct_input_list) && hook_thread)
    {
        DWORD tid = hook_thread_id;

        hook_thread_id = 0;
        PostThreadMessageW(tid, WM_USER+0x10, 0, 0);
        LeaveCriticalSection(&dinput_hook_crit);

        /* wait for hook thread to exit */
        WaitForSingleObject(hook_thread, INFINITE);
        CloseHandle(hook_thread);
        hook_thread = NULL;
    }
    else
        LeaveCriticalSection(&dinput_hook_crit);

    return hook_thread_id != 0;
}

void check_dinput_hooks(LPDIRECTINPUTDEVICE8W iface)
{
    static HHOOK callwndproc_hook;
    static ULONG foreground_cnt;
    IDirectInputDeviceImpl *dev = impl_from_IDirectInputDevice8W(iface);

    EnterCriticalSection(&dinput_hook_crit);

    if (dev->dwCoopLevel & DISCL_FOREGROUND)
    {
        if (dev->acquired)
            foreground_cnt++;
        else
            foreground_cnt--;
    }

    if (foreground_cnt && !callwndproc_hook)
        callwndproc_hook = SetWindowsHookExW( WH_CALLWNDPROC, callwndproc_proc,
                                              DINPUT_instance, GetCurrentThreadId() );
    else if (!foreground_cnt && callwndproc_hook)
    {
        UnhookWindowsHookEx( callwndproc_hook );
        callwndproc_hook = NULL;
    }

    PostThreadMessageW( hook_thread_id, WM_USER+0x10, 1, 0 );

    LeaveCriticalSection(&dinput_hook_crit);
}
