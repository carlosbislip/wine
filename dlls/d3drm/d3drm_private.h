/*
 *      Direct3DRM private interfaces (D3DRM.DLL)
 *
 * Copyright 2010 Christian Costa
 *
 * This file contains the (internal) driver registration functions,
 * driver enumeration APIs and DirectDraw creation functions.
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

#ifndef __D3DRM_PRIVATE_INCLUDED__
#define __D3DRM_PRIVATE_INCLUDED__

#include "d3drm.h"

HRESULT Direct3DRM_create(LPDIRECT3DRM* ppDirect3DRM) DECLSPEC_HIDDEN;
HRESULT Direct3DRMMeshBuilder_create(LPDIRECT3DRMMESHBUILDER* ppMeshBuilder) DECLSPEC_HIDDEN;

#endif /* __D3DRM_PRIVATE_INCLUDED__ */
