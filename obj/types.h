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

typedef struct obj_config_t obj_config_t;
typedef struct obj_t obj_t;

struct obj_config_t {
	size_t _unused;
};

struct obj_t {
	int _unused;
};
