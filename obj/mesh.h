/* mesh.h  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson
 *
 * This library provides a cross-platform OBJ I/O library in C11 providing
 * OBJ ascii reading and writing functionality.
 *
 * The latest source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/obj_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any
 * restrictions.
 *
 */

#pragma once

/*! \file mesh.h
    OBJ to/from mesh transcoding */

#include <obj/types.h>
#include <obj/hashstrings.h>

//! External data structure
struct mesh_t;

/*! Transcode an OBJ data structure to a mesh
\param obj Source OBJ data structure
\return New mesh */
OBJ_API struct mesh_t*
obj_to_mesh(obj_t* obj);

/*! Transcode a mesh to an OBJ data structure
\param obj Destination OBJ data structure
\param mesh Source mesh */
OBJ_API void
obj_from_mesh(obj_t* obj, struct mesh_t* mesh);
