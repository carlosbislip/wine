/*
 * Copyright 2011 Jacek Caban for CodeWeavers
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


#include <stdarg.h>
#include <assert.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"

#include "mshtml_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

static inline HTMLDOMAttribute *impl_from_IHTMLDOMAttribute(IHTMLDOMAttribute *iface)
{
    return CONTAINING_RECORD(iface, HTMLDOMAttribute, IHTMLDOMAttribute_iface);
}

static HRESULT WINAPI HTMLDOMAttribute_QueryInterface(IHTMLDOMAttribute *iface,
                                                 REFIID riid, void **ppv)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IHTMLDOMAttribute_iface;
    }else if(IsEqualGUID(&IID_IHTMLDOMAttribute, riid)) {
        TRACE("(%p)->(IID_IHTMLDOMAttribute %p)\n", This, ppv);
        *ppv = &This->IHTMLDOMAttribute_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv =  NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLDOMAttribute_AddRef(IHTMLDOMAttribute *iface)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLDOMAttribute_Release(IHTMLDOMAttribute *iface)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        assert(!This->elem);
        release_dispex(&This->dispex);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLDOMAttribute_GetTypeInfoCount(IHTMLDOMAttribute *iface, UINT *pctinfo)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLDOMAttribute_GetTypeInfo(IHTMLDOMAttribute *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLDOMAttribute_GetIDsOfNames(IHTMLDOMAttribute *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLDOMAttribute_Invoke(IHTMLDOMAttribute *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLDOMAttribute_get_nodeName(IHTMLDOMAttribute *iface, BSTR *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute_put_nodeName(IHTMLDOMAttribute *iface, VARIANT v)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute_get_nodeValue(IHTMLDOMAttribute *iface, VARIANT *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    DISPPARAMS dp = {NULL, NULL, 0, 0};
    EXCEPINFO ei;

    TRACE("(%p)->(%p)\n", This, p);

    if(!This->elem) {
        FIXME("NULL This->elem\n");
        return E_UNEXPECTED;
    }

    memset(&ei, 0, sizeof(ei));
    return IDispatchEx_InvokeEx(&This->elem->node.dispex.IDispatchEx_iface, This->dispid, LOCALE_SYSTEM_DEFAULT,
            DISPATCH_PROPERTYGET, &dp, p, &ei, NULL);
}

static HRESULT WINAPI HTMLDOMAttribute_get_specified(IHTMLDOMAttribute *iface, VARIANT_BOOL *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLDOMAttributeVtbl HTMLDOMAttributeVtbl = {
    HTMLDOMAttribute_QueryInterface,
    HTMLDOMAttribute_AddRef,
    HTMLDOMAttribute_Release,
    HTMLDOMAttribute_GetTypeInfoCount,
    HTMLDOMAttribute_GetTypeInfo,
    HTMLDOMAttribute_GetIDsOfNames,
    HTMLDOMAttribute_Invoke,
    HTMLDOMAttribute_get_nodeName,
    HTMLDOMAttribute_put_nodeName,
    HTMLDOMAttribute_get_nodeValue,
    HTMLDOMAttribute_get_specified
};

static const tid_t HTMLDOMAttribute_iface_tids[] = {
    IHTMLDOMAttribute_tid,
    0
};
static dispex_static_data_t HTMLDOMAttribute_dispex = {
    NULL,
    DispHTMLDOMAttribute_tid,
    0,
    HTMLDOMAttribute_iface_tids
};

HRESULT HTMLDOMAttribute_Create(HTMLElement *elem, DISPID dispid, HTMLDOMAttribute **attr)
{
    HTMLDOMAttribute *ret;

    ret = heap_alloc_zero(sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLDOMAttribute_iface.lpVtbl = &HTMLDOMAttributeVtbl;
    ret->ref = 1;

    ret->dispid = dispid;
    ret->elem = elem;
    list_add_tail(&elem->attrs, &ret->entry);

    init_dispex(&ret->dispex, (IUnknown*)&ret->IHTMLDOMAttribute_iface,
            &HTMLDOMAttribute_dispex);

    *attr = ret;
    return S_OK;
}
