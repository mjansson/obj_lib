/* types.h  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform OBJ I/O library in C11 providing
 * OBJ ascii reading and writing functionality.
 *
 * The latest source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/obj_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any
 * restrictions.
 *
 */

#pragma once

/*! \file types.h
    OBJ data types */

#include <foundation/platform.h>
#include <foundation/types.h>

#include <obj/build.h>

#if defined(OBJ_COMPILE) && OBJ_COMPILE
#ifdef __cplusplus
#define OBJ_EXTERN extern "C"
#define OBJ_API extern "C"
#else
#define OBJ_EXTERN extern
#define OBJ_API extern
#endif
#else
#ifdef __cplusplus
#define OBJ_EXTERN extern "C"
#define OBJ_API extern "C"
#else
#define OBJ_EXTERN extern
#define OBJ_API extern
#endif
#endif

typedef stream_t* (*obj_stream_open)(const char*, size_t, unsigned int);

typedef struct obj_config_t obj_config_t;
typedef struct obj_t obj_t;
typedef struct obj_material_t obj_material_t;
typedef struct obj_color_t obj_color_t;
typedef struct obj_vertex_t obj_vertex_t;
typedef struct obj_normal_t obj_normal_t;
typedef struct obj_uv_t obj_uv_t;
typedef struct obj_corner_t obj_corner_t;
typedef struct obj_face_t obj_face_t;
typedef struct obj_subgroup_t obj_subgroup_t;
typedef struct obj_group_t obj_group_t;

struct obj_config_t {
	obj_stream_open stream_open;
	string_const_t* search_path;
	size_t search_path_count;
};

struct obj_color_t {
	real red;
	real green;
	real blue;
};

struct obj_material_t {
	string_t name;

	obj_color_t ambient_color;
	string_t ambient_texture;

	obj_color_t diffuse_color;
	string_t diffuse_texture;

	obj_color_t specular_color;
	string_t specular_texture;

	obj_color_t emissive_color;
	string_t emissive_texture;

	real dissolve_factor;
	string_t dissolve_texture;

	real shininess_exponent;
	string_t shininess_texture;

	string_t bump_texture;

	obj_color_t transmission_filter;
};

struct obj_vertex_t {
	real x;
	real y;
	real z;
};

struct obj_normal_t {
	real nx;
	real ny;
	real nz;
};

struct obj_uv_t {
	real u;
	real v;
};

struct obj_corner_t {
	//! Vertex index plus one, always greater than 0
	unsigned int vertex;
	//! Normal index plus one, thus equal to 0 for no/invalid normal
	unsigned int normal;
	//! UV index plus one, thus less to 0 for no/invalid UV
	unsigned int uv;
	//! Index of next corner sharing the same vertex index, less than 0 if none
	int next;
};

struct obj_face_t {
	//! Number of indices in face
	unsigned int count;
	//! Offset in subgroup index array where face indices start
	unsigned int offset;
};

struct obj_subgroup_t {
	//! Material index
	unsigned int material;
	//! Corner data (unique corner tuples, can be shared between faces)
	obj_corner_t* corner;
	//! Corner indices for all faces
	unsigned int* index;
	//! Subgroup faces
	obj_face_t* face;
	//! Triangulation by obj_triangulate (zero-based index into corner array)
	unsigned int* triangle;
	//! Number of triangles in triangulation
	unsigned int triangle_count;
};

struct obj_group_t {
	string_t name;
	obj_subgroup_t** subgroup;
};

struct obj_t {
	string_t base_path;
	obj_material_t* material;
	obj_vertex_t* vertex;
	obj_normal_t* normal;
	obj_uv_t* uv;
	obj_group_t** group;
};
