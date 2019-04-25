/* obj.c  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson / Rampant Pixels
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

#include "obj.h"

#include <foundation/array.h>
#include <foundation/stream.h>

int
obj_module_initialize(obj_config_t config) {
	FOUNDATION_UNUSED(config);
	return 0;
}

void
obj_module_finalize(void) {
}

bool
obj_module_is_initialized(void) {
	return true;
}

void
obj_module_parse_config(const char* path, size_t path_size, const char* buffer, size_t size,
                        const struct json_token_t* tokens, size_t num_tokens) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(path_size);
	FOUNDATION_UNUSED(buffer);
	FOUNDATION_UNUSED(size);
	FOUNDATION_UNUSED(tokens);
	FOUNDATION_UNUSED(num_tokens);
}

void
obj_initialize(obj_t* obj) {
	memset(obj, 0, sizeof(obj_t));
}

void
obj_finalize(obj_t* obj) {
	if (!obj)
		return;

	array_deallocate(obj->vertex);
	array_deallocate(obj->normal);
	array_deallocate(obj->uv);
}

static inline bool
is_whitespace(char c) {
	return (c == ' ') || (c == '\t');
}

static inline bool
is_endline(char c) {
	return (c == '\n') || (c == '\r');
}

static string_const_t
skip_whitespace(const char* buffer, size_t length) {
	size_t pos = 0;
	while (pos < length) {
		if (!is_whitespace(buffer[pos]))
			break;
		++pos;
	}
	return string_const(buffer + pos, length - pos);
}

static string_const_t
skip_whitespace_and_endline(const char* buffer, size_t length) {
	size_t pos = 0;
	while (pos < length) {
		if (!is_whitespace(buffer[pos]) && !is_endline(buffer[pos]))
			break;
		++pos;
	}
	return string_const(buffer + pos, length - pos);
}

static string_const_t
skip_line(const char* buffer, size_t length) {
	size_t pos = 0;
	while (pos < length) {
		if (is_endline(buffer[pos]))
			break;
		++pos;
	}
	while (pos < length) {
		if (!is_endline(buffer[pos]))
			break;
		++pos;
	}
	return string_const(buffer + pos, length - pos);
}
int
obj_read(obj_t* obj, stream_t* stream) {
	size_t file_size = stream_size(stream);
	size_t reserve_count = file_size / 128;
	if (reserve_count < (128 * 1024))
		reserve_count = 128 * 1024;

	array_clear(obj->vertex);
	array_clear(obj->normal);
	array_clear(obj->uv);
	array_reserve(obj->vertex, reserve_count);

	const size_t buffer_capacity = 4000;
	char* buffer = memory_allocate(HASH_OBJ, buffer_capacity, 0, MEMORY_PERSISTENT);

	string_const_t tokens_storage[64];
	const size_t tokens_capacity = sizeof(tokens_storage) / sizeof(tokens_storage[0]);

	obj_material_t* current_material = nullptr;
	obj_group_t* current_group = nullptr;
	bool material_changed = false;
	size_t num_vertex_since_group = 0;

	while (!stream_eos(stream)) {
		size_t last_remain = 0;
		size_t was_read = stream_read(stream, buffer, buffer_capacity);

		string_const_t remain = {buffer, was_read};
		string_const_t* tokens = tokens_storage + 1;

		remain = skip_whitespace_and_endline(STRING_ARGS(remain));
		while (remain.length > 3) {
			size_t num_tokens = 0;
			size_t offset = 1;

			while (remain.length && (offset < remain.length)) {
				if (is_whitespace(remain.str[offset]) || is_endline(remain.str[offset])) {
					if (offset && (num_tokens < tokens_capacity))
						tokens_storage[num_tokens++] = string_const(remain.str, offset);
					if (is_endline(remain.str[offset]))
						break;
					remain = skip_whitespace(remain.str + offset, remain.length - offset);
					offset = 0;
				} else {
					++offset;
				}
			}

			// Check if incomplete line was read (no endline and not end of file)
			size_t end_line = offset;
			if (offset == remain.length) {
				if (was_read == buffer_capacity)
					break;
				++end_line;
				last_remain = 0;
				if (num_tokens < tokens_capacity)
					tokens_storage[num_tokens++] = remain;
			}
			if (!num_tokens)
				break;

			string_const_t command = tokens_storage[0];
			--num_tokens;

			if (command.str[0] == 'v') {
				if (string_equal(STRING_ARGS(command), STRING_CONST("v"))) {
					if (num_tokens >= 2) {
						obj_vertex_t vertex = {
						    string_to_real(STRING_ARGS(tokens[0])), string_to_real(STRING_ARGS(tokens[1])),
						    (num_tokens > 2) ? string_to_real(STRING_ARGS(tokens[2])) : 0.0f};
						array_push(obj->vertex, vertex);
					} else {
						obj_vertex_t vertex = {0, 0, 0};
						array_push(obj->vertex, vertex);
					}
					++num_vertex_since_group;
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("vt"))) {
					if (!array_capacity(obj->uv))
						array_reserve(obj->uv, reserve_count);
					if (num_tokens >= 2) {
						obj_uv_t uv = {string_to_real(STRING_ARGS(tokens[0])),
						               string_to_real(STRING_ARGS(tokens[1]))};
						array_push(obj->uv, uv);
					} else {
						obj_uv_t uv = {0, 0};
						array_push(obj->uv, uv);
					}
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("vn"))) {
					if (!array_capacity(obj->normal))
						array_reserve(obj->normal, reserve_count);
					if (num_tokens >= 3) {
						obj_normal_t normal = {string_to_real(STRING_ARGS(tokens[0])),
						                       string_to_real(STRING_ARGS(tokens[1])),
						                       string_to_real(STRING_ARGS(tokens[2]))};
						array_push(obj->normal, normal);
					} else {
						obj_normal_t normal = {0, 0, 0};
						array_push(obj->normal, normal);
					}
				}
			} else if (command.str[0] == 'f') {
				if (string_equal(STRING_ARGS(command), STRING_CONST("f")) && (num_tokens > 2)) {
					size_t num_corners = num_tokens;

					if (!current_group) {
						current_group = memory_allocate(HASH_OBJ, sizeof(obj_group_t), 0,
						                                MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
						array_push(obj->group, current_group);

						size_t estimated_triangles = (num_vertex_since_group * 3) / 2;
						size_t estimated_corners = estimated_triangles * 3;

						array_reserve(current_group->face, estimated_triangles);
						array_reserve(current_group->corner, estimated_corners);

						num_vertex_since_group = 0;

						material_changed = true;
					}

					if (material_changed) {
						if (!current_material)
							current_material = setupMaterial(context, materialDefinition, defaultData);
						material_changed = false;
					}

					unsigned int last_corner_count = array_size(current_group->corner);
					obj_face_t face = {0, last_corner_count};
					bool valid_face = (num_corners >= 3);
					for (size_t icorner = 0; valid_face && (icorner < num_corners); ++icorner) {
						string_const_t corner_token[3];
						size_t num_corner_tokens = string_explode(STRING_ARGS(tokens[icorner]),
						                                          STRING_CONST("/"), corner_token, 3, true);

						unsigned int ivert = 0;
						unsigned int iuv = 0;
						unsigned int inorm = 0;
						if (num_corner_tokens)
							ivert = string_to_uint(STRING_ARGS(corner_token[0]), false);
						if (num_corner_tokens > 1)
							iuv = string_to_uint(STRING_ARGS(corner_token[1]), false);
						if (num_corner_tokens > 2)
							inorm = string_to_uint(STRING_ARGS(corner_token[2]), false);

						if (ivert < 0)
							ivert = array_size(obj->vertex) + ivert + 1;
						if (inorm < 0)
							inorm = array_size(obj->normal) + inorm + 1;
						if (iuv < 0)
							iuv = array_size(obj->uv) + iuv + 1;

						if ((ivert <= 0) || (ivert > (ssize_t)array_size(obj->vertex)))
							valid_face = false;
						if ((inorm <= 0) || (inorm > (ssize_t)array_size(obj->normal)))
							inorm = 0;
						if ((iuv <= 0) || (iuv > (ssize_t)array_size(obj->uv)))
							iuv = 0;

						if (valid_face) {
							obj_corner_t corner = {ivert, inorm, iuv};
							array_push(current_group->corner, corner);
							++face.count;
						}
					}

					if (valid_face) {
						array_push(current_group->face, face);
					} else {
						array_resize(current_group->corner, last_corner_count);
					}
				}
			} else if (command.str[0] == 'm') {
				if (string_equal(STRING_ARGS(command), STRING_CONST("mtllib"))) {
					// if (num_tokens)
					//	load material lib
				}
			} else if (command.str[0] == 'u') {
				if (string_equal(STRING_ARGS(command), STRING_CONST("usemtl"))) {
					if (num_tokens) {
					}
				}
			} else if (command.str[0] == 'g') {
				if (string_equal(STRING_ARGS(command), STRING_CONST("g"))) {
				}
			}

			remain.str += end_line;
			remain.length -= end_line;

			remain = skip_whitespace_and_endline(STRING_ARGS(remain));
			last_remain = remain.length;
		}

		if (!stream_eos(stream) && last_remain)
			stream_seek(stream, -(ssize_t)last_remain, STREAM_SEEK_CURRENT);
	}

	memory_deallocate(buffer);

	return -1;
}

int
obj_write(const obj_t* obj, stream_t* stream) {
	FOUNDATION_UNUSED(obj);
	FOUNDATION_UNUSED(stream);
	return -1;
}

int
obj_triangulate(obj_t* obj) {
	FOUNDATION_UNUSED(obj);
	return -1;
}
