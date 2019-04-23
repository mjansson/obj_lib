/* obj.h  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson / Rampant Pixels
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

/*! \file obj.h
    OBJ library entry points */

#include <obj/types.h>

/*! Initialize OBJ library
    \return 0 if success, <0 if error */
OBJ_API int
obj_module_initialize(obj_config_t config);

/*! Finalize OBJ library */
OBJ_API void
obj_module_finalize(void);

/*! Query if OBJ library is initialized
    \return true if library is initialized, false if not */
OBJ_API bool
obj_module_is_initialized(void);

/*! Query version of OBJ library
    \return Library version */
OBJ_API version_t
obj_module_version(void);

/*! Parse config declarations from JSON buffer
\param buffer Data buffer
\param size Size of data buffer
\param tokens JSON tokens
\param num_tokens Number of JSON tokens */
OBJ_API void
obj_module_parse_config(const char* path, size_t path_size, const char* buffer, size_t size,
                        const struct json_token_t* tokens, size_t num_tokens);

/*! Initialize OBJ data structure
\param obj Target OBJ data structure */
OBJ_API void
obj_initialize(obj_t* obj);

/*! Finalize OBJ data structure
\param obj OBJ data structure */
OBJ_API void
obj_finalize(obj_t* obj);

/*! Read OBJ data
\param obj Target OBJ data structure
\param stream Source stream
\return 0 if success, <0 if error */
OBJ_API int
obj_read(obj_t* obj, stream_t* stream);

/*! Write OBJ data
\param obj Source OBJ data structure
\param stream Target stream */
OBJ_API int
obj_write(const obj_t* obj, stream_t* stream);
