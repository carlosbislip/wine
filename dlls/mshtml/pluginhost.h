/*
 * Copyright 2010 Jacek Caban for CodeWeavers
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

typedef struct HTMLObjectElement HTMLObjectElement;

typedef struct {
    IOleClientSite       IOleClientSite_iface;
    IAdviseSinkEx        IAdviseSinkEx_iface;
    IPropertyNotifySink  IPropertyNotifySink_iface;
    IDispatch            IDispatch_iface;
    IOleInPlaceSiteEx    IOleInPlaceSiteEx_iface;
    IOleControlSite      IOleControlSite_iface;
    IBindHost            IBindHost_iface;
    IServiceProvider     IServiceProvider_iface;

    LONG ref;

    IUnknown *plugin_unk;
    HWND hwnd;
} PluginHost;

HRESULT create_plugin_host(IUnknown*,PluginHost**);
void update_plugin_window(PluginHost*,HWND,const RECT*);