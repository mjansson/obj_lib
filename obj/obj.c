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
#include <foundation/path.h>

static unsigned int INVALID_INDEX = 0xFFFFFFFF;

static obj_config_t _obj_config;

int
obj_module_initialize(obj_config_t config) {
	_obj_config = config;
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

static void
obj_finalize_groups(obj_t* obj) {
	for (unsigned int igroup = 0, gsize = array_size(obj->group); igroup < gsize; ++igroup) {
		obj_group_t* group = obj->group[igroup];
		for (unsigned int isub = 0, sgsize = array_size(group->subgroup); isub < sgsize; ++isub) {
			obj_subgroup_t* subgroup = group->subgroup[isub];
			array_deallocate(subgroup->corner);
			array_deallocate(subgroup->face);
			memory_deallocate(subgroup);
		}
		memory_deallocate(group);
	}

	array_clear(obj->group);
}

static void
obj_finalize_material(obj_material_t* material) {
	string_deallocate(material->name.str);
	string_deallocate(material->ambient_texture.str);
	string_deallocate(material->diffuse_texture.str);
	string_deallocate(material->specular_texture.str);
	string_deallocate(material->emissive_texture.str);
	string_deallocate(material->dissolve_texture.str);
	string_deallocate(material->shininess_texture.str);
	string_deallocate(material->bump_texture.str);
}

static void
obj_finalize_materials(obj_t* obj) {
	for (unsigned int imat = 0, msize = array_size(obj->material); imat < msize; ++imat)
		obj_finalize_material(obj->material + imat);

	array_clear(obj->material);
}

void
obj_finalize(obj_t* obj) {
	if (!obj)
		return;

	obj_finalize_groups(obj);
	obj_finalize_materials(obj);

	array_deallocate(obj->group);
	array_deallocate(obj->material);
	array_deallocate(obj->vertex);
	array_deallocate(obj->normal);
	array_deallocate(obj->uv);

	string_deallocate(obj->base_path.str);
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

static void
set_color(obj_color_t* color, real red, real green, real blue) {
	color->red = red;
	color->green = green;
	color->blue = blue;
}

static obj_color_t
parse_color(const string_const_t* tokens, size_t num_tokens) {
	obj_color_t color = {0};
	if (num_tokens)
		color.red = string_to_real(STRING_ARGS(tokens[0]));
	if (num_tokens > 1)
		color.green = string_to_real(STRING_ARGS(tokens[1]));
	else
		color.green = color.red;
	if (num_tokens > 2)
		color.blue = string_to_real(STRING_ARGS(tokens[2]));
	else
		color.blue = color.green;
	return color;
}

obj_material_t
material_default(void) {
	obj_material_t material = {0};
	material.name = string_clone(STRING_CONST("default"));
	set_color(&material.ambient_color, 0, 0, 0);
	set_color(&material.diffuse_color, 1, 1, 1);
	set_color(&material.specular_color, 0, 0, 0);
	set_color(&material.emissive_color, 0, 0, 0);
	set_color(&material.transmission_filter, 0, 0, 0);
	material.dissolve_factor = 1;
	material.shininess_exponent = 1;
	return material;
}

int
load_material_lib(obj_t* obj, const char* filename, size_t length) {
	stream_t* stream = nullptr;
	if (_obj_config.stream_open) {
		stream = _obj_config.stream_open(filename, length, STREAM_IN);
	} else {
		stream = stream_open(filename, length, STREAM_IN);
		if (!stream) {
			string_t testpath = path_allocate_concat(STRING_ARGS(obj->base_path), filename, length);
			stream = stream_open(STRING_ARGS(testpath), STREAM_IN);
			string_deallocate(testpath.str);
		}
		for (size_t ipath = 0; !stream && ipath < _obj_config.search_path_count; ++ipath) {
			string_t testpath =
			    path_allocate_concat(STRING_ARGS(_obj_config.search_path[ipath]), filename, length);
			stream = stream_open(STRING_ARGS(testpath), STREAM_IN);
			string_deallocate(testpath.str);
		}
	}
	if (!stream)
		return 1;

	const size_t buffer_capacity = 4000;
	char* buffer = memory_allocate(HASH_OBJ, buffer_capacity, 0, MEMORY_PERSISTENT);

	bool material_valid = false;
	obj_material_t material = {0};

	while (!stream_eos(stream)) {
		size_t last_remain = 0;
		size_t was_read = stream_read(stream, buffer, buffer_capacity);

		string_const_t remain = {buffer, was_read};
		string_const_t tokens_storage[64];
		const size_t tokens_capacity = sizeof(tokens_storage) / sizeof(tokens_storage[0]);
		string_const_t* tokens = tokens_storage + 1;

		remain = skip_whitespace_and_endline(STRING_ARGS(remain));
		while (remain.length > 3) {
			size_t num_tokens = 0;
			size_t offset = 1;

			while (remain.length && (offset < remain.length)) {
				if (is_whitespace(remain.str[offset]) || is_endline(remain.str[offset])) {
					if (offset && (num_tokens < tokens_capacity)) {
						tokens_storage[num_tokens++] = string_const(remain.str, offset);
					}
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

			if (string_equal(STRING_ARGS(command), STRING_CONST("newmtl"))) {
				if (material_valid)
					array_push(obj->material, material);
				else
					obj_finalize_material(&material);
				material = material_default();
				material.name = (num_tokens && tokens[0].length) ?
				                    string_clone(STRING_ARGS(tokens[0])) :
				                    string_clone(STRING_CONST("__unnamed"));
				material_valid = true;
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("d")) && num_tokens) {
				material.dissolve_factor = string_to_real(STRING_ARGS(tokens[0]));
			} else if (num_tokens && (command.str[0] == 'K')) {
				if (string_equal(STRING_ARGS(command), STRING_CONST("Ka"))) {
					material.ambient_color = parse_color(tokens, num_tokens);
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("Kd"))) {
					material.diffuse_color = parse_color(tokens, num_tokens);
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("Ks"))) {
					material.specular_color = parse_color(tokens, num_tokens);
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("Ke"))) {
					material.emissive_color = parse_color(tokens, num_tokens);
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("Ns")) && num_tokens) {
				material.shininess_exponent = string_to_real(STRING_ARGS(tokens[0]));
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("Tf")) && num_tokens) {
				material.transmission_filter = parse_color(tokens, num_tokens);
			} else if (num_tokens && (command.str[0] == 'm')) {
				if (string_equal(STRING_ARGS(command), STRING_CONST("map_Ka"))) {
					material.ambient_texture = string_clone(STRING_ARGS(tokens[0]));
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("map_Kd"))) {
					material.diffuse_texture = string_clone(STRING_ARGS(tokens[0]));
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("map_Ks"))) {
					material.specular_texture = string_clone(STRING_ARGS(tokens[0]));
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("map_Ke"))) {
					material.emissive_texture = string_clone(STRING_ARGS(tokens[0]));
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("map_d"))) {
					material.dissolve_texture = string_clone(STRING_ARGS(tokens[0]));
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("map_Ns"))) {
					material.shininess_texture = string_clone(STRING_ARGS(tokens[0]));
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("map_bump"))) {
					material.bump_texture = string_clone(STRING_ARGS(tokens[0]));
				}
			}

			remain.str += end_line;
			remain.length -= end_line;

			remain = skip_whitespace_and_endline(STRING_ARGS(remain));
			last_remain = remain.length;
		}

		if (!stream_eos(stream))
			stream_seek(stream, -(ssize_t)last_remain, STREAM_SEEK_CURRENT);
	}

	if (material_valid)
		array_push(obj->material, material);
	else
		obj_finalize_material(&material);

	stream_deallocate(stream);

	return 0;
}

int
obj_read(obj_t* obj, stream_t* stream) {
	size_t file_size = stream_size(stream);
	size_t reserve_count = file_size / 128;
	if (reserve_count < (128 * 1024))
		reserve_count = 128 * 1024;

	obj_finalize_groups(obj);
	obj_finalize_materials(obj);

	array_clear(obj->vertex);
	array_clear(obj->normal);
	array_clear(obj->uv);

	array_reserve(obj->vertex, reserve_count);

	string_deallocate(obj->base_path.str);
	string_const_t path = stream_path(stream);
	path = path_directory_name(STRING_ARGS(path));
	obj->base_path = string_clone(STRING_ARGS(path));

	const size_t buffer_capacity = 4000;
	char* buffer = memory_allocate(HASH_OBJ, buffer_capacity, 0, MEMORY_PERSISTENT);

	string_const_t tokens_storage[64];
	const size_t tokens_capacity = sizeof(tokens_storage) / sizeof(tokens_storage[0]);

	obj_group_t* current_group = nullptr;
	obj_subgroup_t* current_subgroup = nullptr;

	size_t num_vertex_since_group = 0;
	string_t group_name = {0};
	unsigned int material_index = INVALID_INDEX;

	int* vertex_to_corner = nullptr;
	array_reserve(vertex_to_corner, array_capacity(obj->vertex));

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

			if (string_equal(STRING_ARGS(command), STRING_CONST("v"))) {
				if (num_tokens >= 2) {
					obj_vertex_t vertex = {
					    string_to_real(STRING_ARGS(tokens[0])),
					    string_to_real(STRING_ARGS(tokens[1])),
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
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("f")) && (num_tokens > 2)) {
				size_t num_corners = num_tokens;

				if (!current_group) {
					current_group = memory_allocate(HASH_OBJ, sizeof(obj_group_t), 0,
					                                MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
					array_push(obj->group, current_group);

					current_group->name = group_name;
					group_name = string(0, 0);

					current_subgroup = nullptr;
					array_clear(vertex_to_corner);
				}

				if (!current_subgroup) {
					if (material_index > array_size(obj->material)) {
						obj_material_t material = material_default();
						material_index = array_size(obj->material);
						array_push(obj->material, material);
					}

					current_subgroup = memory_allocate(HASH_OBJ, sizeof(obj_subgroup_t), 0,
					                                   MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
					array_push(current_group->subgroup, current_subgroup);

					size_t estimated_triangles = (num_vertex_since_group * 3) / 2;
					size_t estimated_corners = estimated_triangles * 3;

					array_reserve(current_subgroup->face, estimated_triangles);
					array_reserve(current_subgroup->corner, estimated_corners);
					array_reserve(current_subgroup->index, estimated_corners);
					current_subgroup->material = material_index;

					num_vertex_since_group = 0;
				}

				unsigned int last_index_count = array_size(current_subgroup->index);
				obj_face_t face = {0, last_index_count};
				bool valid_face = (num_corners >= 3);
				for (size_t icorner = 0; valid_face && (icorner < num_corners); ++icorner) {
					string_const_t corner_token[3];
					size_t num_corner_tokens = string_explode(
					    STRING_ARGS(tokens[icorner]), STRING_CONST("/"), corner_token, 3, true);

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
						if ((ivert > array_size(vertex_to_corner)) ||
						    !vertex_to_corner[ivert - 1]) {
							obj_corner_t corner = {ivert, inorm, iuv};
							unsigned int corner_index = array_size(vertex_to_corner);
							array_push(current_subgroup->corner, corner);
							if (ivert > array_size(vertex_to_corner)) {
							}
							vertex_to_corner[ivert - 1] = corner_index;
							array_push()
						} else {
						}

						++face.count;
					}
				}

				if (valid_face) {
					array_push(current_subgroup->face, face);
				} else {
					array_resize(current_subgroup->index, last_index_count);
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("mtllib")) && num_tokens) {
				load_material_lib(obj, STRING_ARGS(tokens[0]));
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("usemtl")) && num_tokens) {
				string_const_t name = tokens[0];
				unsigned int next_material = INVALID_INDEX;
				for (unsigned int imat = 0, msize = array_size(obj->material); imat < msize;
				     ++imat) {
					if (string_equal(STRING_ARGS(name), STRING_ARGS(obj->material[imat].name))) {
						next_material = imat;
						break;
					}
				}
				if (next_material != material_index) {
					material_index = next_material;
					current_subgroup = nullptr;
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("g"))) {
				group_name = (num_tokens && tokens[0].length) ?
				                 string_clone_string(tokens[0]) :
				                 string_clone(STRING_CONST("__unnamed"));
				current_group = nullptr;
			}

			remain.str += end_line;
			remain.length -= end_line;

			remain = skip_whitespace_and_endline(STRING_ARGS(remain));
			last_remain = remain.length;
		}

		if (!stream_eos(stream) && last_remain)
			stream_seek(stream, -(ssize_t)last_remain, STREAM_SEEK_CURRENT);
	}

	string_deallocate(group_name.str);
	array_deallocate(vertex_to_corner);
	memory_deallocate(buffer);

	return 0;
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
	return 0;
}
