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
	int vertex;
	int normal;
	int uv;
};

struct obj_face_t {
	int count;
	int offset;
};

struct obj_subgroup_t {
	unsigned int material;
	obj_corner_t* corner;
	obj_face_t* face;
};

struct obj_group_t {
	string_t name;
	obj_subgroup_t** subgroups;
};

struct obj_t {
	obj_material_t* material;
	obj_vertex_t* vertex;
	obj_normal_t* normal;
	obj_uv_t* uv;
	obj_group_t** group;
};
