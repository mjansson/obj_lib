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
			array_deallocate(subgroup->triangle);
			array_deallocate(subgroup->index);
			array_deallocate(subgroup->corner);
			array_deallocate(subgroup->face);
			memory_deallocate(subgroup);
		}
		array_deallocate(group->subgroup);
		string_deallocate(group->name.str);
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

	memory_deallocate(buffer);
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

					array_clear(vertex_to_corner);

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
						unsigned int corner_index;
						unsigned int corner_count = array_size(current_subgroup->corner);
						if ((ivert > array_size(vertex_to_corner)) ||
						    (vertex_to_corner[ivert - 1] < 0)) {
							obj_corner_t corner = {ivert, inorm, iuv, -1};
							corner_index = array_size(current_subgroup->corner);
							array_push(current_subgroup->corner, corner);
							if (ivert > array_size(vertex_to_corner)) {
								unsigned int prev_size = array_size(vertex_to_corner);
								array_resize(vertex_to_corner, ivert);
								memset(vertex_to_corner + prev_size, 0xFF,
								       sizeof(int) * (ivert - prev_size));
							}
							vertex_to_corner[ivert - 1] = corner_index;
						} else {
							corner_index = vertex_to_corner[ivert - 1];
							obj_corner_t* last_corner = nullptr;
							while (corner_index < corner_count) {
								obj_corner_t* corner = current_subgroup->corner + corner_index;
								if (!corner->normal || !inorm || (corner->normal == inorm)) {
									if (!corner->uv || !iuv || (corner->uv == iuv)) {
										if (inorm && !corner->normal)
											corner->normal = inorm;
										if (iuv && !corner->uv)
											corner->uv = iuv;
										break;
									}
								}
								corner_index = (unsigned int)corner->next;
								last_corner = corner;
							}
							if (corner_index >= corner_count) {
								obj_corner_t corner = {ivert, inorm, iuv, -1};
								corner_index = array_size(current_subgroup->corner);
								array_push(current_subgroup->corner, corner);
								last_corner->next = corner_index;
							}
						}
						array_push(current_subgroup->index, corner_index);
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
				string_deallocate(group_name.str);
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

static void
vertex_sub(obj_vertex_t* from, obj_vertex_t* to, real* diff) {
	diff[0] = to->x - from->x;
	diff[1] = to->y - from->y;
	diff[2] = to->z - from->z;
}

static void
vec_cross(const real* FOUNDATION_RESTRICT first_edge, const real* FOUNDATION_RESTRICT second_edge,
          real* FOUNDATION_RESTRICT normal) {
	normal[0] = first_edge[1] * second_edge[2] - first_edge[2] * second_edge[1];
	normal[1] = first_edge[2] * second_edge[0] - first_edge[0] * second_edge[2];
	normal[2] = first_edge[0] * second_edge[1] - first_edge[1] * second_edge[0];
}

static real
vec_dot(const real* FOUNDATION_RESTRICT first_normal,
        const real* FOUNDATION_RESTRICT second_normal) {
	return (first_normal[0] * second_normal[0]) + (first_normal[1] * second_normal[1]) +
	       (first_normal[2] * second_normal[2]);
}

static void
vec_normalize(real* vec) {
	real length_sqr = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
	real oolength = math_rsqrt(length_sqr);
	vec[0] *= oolength;
	vec[1] *= oolength;
	vec[2] *= oolength;
}

static bool
polygon_convex(unsigned int* index, obj_corner_t* corner, obj_vertex_t* vertex,
               unsigned int corner_count) {
	if (corner_count < 4)
		return true;

	int prev_corner;
	int cur_corner = index[0];
	int next_corner = index[1];

	obj_vertex_t* next_vertex = vertex + (corner[next_corner].vertex - 1);
	obj_vertex_t* cur_vertex = vertex + (corner[cur_corner].vertex - 1);

	real edge[3];
	vertex_sub(cur_vertex, next_vertex, edge);

	real last_normal[3] = {0, 0, 0};
	real last_edge[3];

	bool first_normal = true;
	for (unsigned int icorner = 0; icorner < corner_count; ++icorner) {
		prev_corner = cur_corner;
		cur_corner = next_corner;
		next_corner = index[(icorner + 2) % corner_count];

		if ((prev_corner == cur_corner) || (prev_corner == next_corner) ||
		    (cur_corner == next_corner))
			continue;

		last_edge[0] = edge[0];
		last_edge[1] = edge[1];
		last_edge[2] = edge[2];

		cur_vertex = next_vertex;
		next_vertex = vertex + (corner[next_corner].vertex - 1);
		vertex_sub(cur_vertex, next_vertex, edge);

		real normal[3];
		vec_cross(last_edge, edge, normal);
		if (!first_normal && (vec_dot(last_normal, normal) < 0))
			return false;
		if (first_normal && (!math_real_is_zero(normal[0]) || !math_real_is_zero(normal[1]) ||
		                     !math_real_is_zero(normal[2]))) {
			last_normal[0] = normal[0];
			last_normal[1] = normal[1];
			last_normal[2] = normal[2];
			first_normal = false;
		}
	}

	return true;
}

static bool
point_inside_triangle_2d(const real* FOUNDATION_RESTRICT v0, const real* FOUNDATION_RESTRICT v1,
                         const real* FOUNDATION_RESTRICT v2, const real* FOUNDATION_RESTRICT pt) {
	double ax = (double)(v2[0] - v1[0]);
	double ay = (double)(v2[1] - v1[1]);
	double bx = (double)(v0[0] - v2[0]);
	double by = (double)(v0[1] - v2[1]);
	double cx = (double)(v1[0] - v0[0]);
	double cy = (double)(v1[1] - v0[1]);
	double apx = (double)(pt[0] - v0[0]);
	double apy = (double)(pt[1] - v0[1]);
	double bpx = (double)(pt[0] - v1[0]);
	double bpy = (double)(pt[1] - v1[1]);
	double cpx = (double)(pt[0] - v2[0]);
	double cpy = (double)(pt[1] - v2[1]);
	double s0 = ax * bpy - ay * bpx;
	double s1 = cx * apy - cy * apx;
	double s2 = bx * cpy - by * cpx;
	return ((s0 >= 0) && (s1 >= 0) && (s2 >= 0)) || ((s0 <= 0) && (s1 <= 0) && (s2 <= 0));
}

static unsigned int
triangulate_convex(unsigned int* index, obj_corner_t* corner, obj_vertex_t* vertex,
                   unsigned int corner_count, unsigned int** triangle) {
	FOUNDATION_UNUSED(vertex);
	FOUNDATION_UNUSED(corner);
	unsigned int triangle_count = 0;
	unsigned int base = 1;
	while (corner_count >= 3) {
		array_push(*triangle, index[0]);
		array_push(*triangle, index[base]);
		array_push(*triangle, index[base + 1]);
		++base;
		++triangle_count;
		--corner_count;
	}
	return triangle_count;
}

static unsigned int
triangulate_concave(unsigned int* index, obj_corner_t* corner, obj_vertex_t* vertex,
                    unsigned int index_count, unsigned int** triangle) {
	if (index_count < 3)
		return 0;

	real xaxis[3];
	real yaxis[3];
	real normal[3];
	unsigned int cur_index = 0;
	unsigned int cur_corner = index[cur_index];
	do {
		unsigned int next_index = cur_index + 1;
		unsigned int next_corner = index[next_index];
		unsigned int last_corner = index[(cur_index + 2) % index_count];
		vertex_sub(vertex + (corner[next_corner].vertex - 1),
		           vertex + (corner[last_corner].vertex - 1), xaxis);
		vertex_sub(vertex + (corner[next_corner].vertex - 1),
		           vertex + (corner[cur_corner].vertex - 1), yaxis);
		vec_cross(xaxis, yaxis, normal);
		if (!math_real_is_zero(normal[0]) || !math_real_is_zero(normal[1]) ||
		    !math_real_is_zero(normal[2])) {
			vec_normalize(normal);
			break;
		}
		cur_index = next_index;
		cur_corner = next_corner;
	} while (cur_index < (index_count - 1));

	if (cur_index == (index_count - 1))
		return 0;  // All corners in a straight line

	vec_normalize(xaxis);
	vec_cross(normal, xaxis, yaxis);
	vec_normalize(yaxis);

#define BASE_COORDINATES 32
	real base_xy[BASE_COORDINATES * 2];
	real* coord = base_xy;

	unsigned int base_local[BASE_COORDINATES];
	unsigned int* local = base_local;

	if (index_count > BASE_COORDINATES) {
		coord = memory_allocate(HASH_OBJ, sizeof(real) * index_count * 2, 0, MEMORY_TEMPORARY);
		local = memory_allocate(HASH_OBJ, sizeof(unsigned int) * index_count, 0, MEMORY_TEMPORARY);
	}

	obj_vertex_t* origo = vertex + (corner[index[0]].vertex - 1);

	// Project polygon on plane
	coord[0] = 0;
	coord[1] = 0;
	local[0] = 0;
	for (unsigned int icorner = 1; icorner < index_count; ++icorner) {
		real diff[3];
		vertex_sub(origo, vertex + (corner[index[icorner]].vertex - 1), diff);
		coord[(icorner * 2) + 0] = vec_dot(diff, xaxis);
		coord[(icorner * 2) + 1] = vec_dot(diff, yaxis);
		local[icorner] = icorner;
	}

	real winding = 0;
	for (unsigned int icorner = 0; icorner < index_count; ++icorner) {
		unsigned int inext = (icorner + 1) % index_count;
		real xdiff = coord[inext * 2] - coord[icorner * 2];
		real ysum = coord[(inext * 2) + 1] + coord[(inext * 2) + 1];
		winding += xdiff * ysum;
	}

	// Ear clip polygon in 2D
	unsigned int local_count = index_count;
	unsigned int triangle_count = 0;
	while (local_count > 3) {
		unsigned int base = 1;
		do {
			unsigned int prev = base ? (base - 1) : (local_count - 1);
			unsigned int next = (base + 1) % local_count;
			unsigned int i0 = local[prev];
			unsigned int i1 = local[base];
			unsigned int i2 = local[next];

			if ((i0 == i1) || (i0 == i2) || (i1 == i2)) {
				memmove(local + base, local + base + 1,
				        sizeof(unsigned int) * (local_count - base - 1));
				base = 1;
				--local_count;
				continue;
			}

			real xdiff = coord[i1 * 2] - coord[i0 * 2];
			real ysum = coord[(i1 * 2) + 1] + coord[(i0 * 2) + 1];
			real triangle_winding = xdiff * ysum;

			xdiff = coord[i2 * 2] - coord[i1 * 2];
			ysum = coord[(i2 * 2) + 1] + coord[(i1 * 2) + 1];
			triangle_winding += xdiff * ysum;

			xdiff = coord[i0 * 2] - coord[i2 * 2];
			ysum = coord[(i0 * 2) + 1] + coord[(i2 * 2) + 1];
			triangle_winding += xdiff * ysum;

			if (((winding < 0) && (triangle_winding >= 0)) ||
			    ((winding > 0) && (triangle_winding <= 0))) {
				++base;
				continue;
			}

			bool point_inside = false;
			for (unsigned int ipt = 0; !point_inside && (ipt < local_count); ++ipt) {
				if ((ipt != prev) && (ipt != base) && (ipt != next))
					point_inside =
					    point_inside_triangle_2d(coord + (i0 * 2), coord + (i1 * 2),
					                             coord + (i2 * 2), coord + (local[ipt] * 2));
			}

			if (point_inside) {
				++base;
				continue;
			}

			array_push(*triangle, index[i0]);
			array_push(*triangle, index[i1]);
			array_push(*triangle, index[i2]);
			++triangle_count;

			memmove(local + base, local + base + 1,
			        sizeof(unsigned int) * (local_count - base - 1));
			base = 1;
			--local_count;

		} while ((base != local_count) && (local_count >= 3));

		if (base == local_count)
			break;  // Only degenerate zero area triangles left
	}

	return triangle_count;
}

static int
obj_triangulate_subgroup(obj_t* obj, obj_subgroup_t* subgroup) {
	array_clear(subgroup->triangle);
	array_reserve(subgroup->triangle, 3 * array_size(subgroup->face));
	subgroup->triangle_count = 0;

	for (unsigned int iface = 0, fsize = array_size(subgroup->face); iface < fsize; ++iface) {
		obj_face_t* face = subgroup->face + iface;
		bool convex = polygon_convex(subgroup->index + face->offset, subgroup->corner, obj->vertex,
		                             face->count);

		if (convex)
			subgroup->triangle_count +=
			    triangulate_convex(subgroup->index + face->offset, subgroup->corner, obj->vertex,
			                       face->count, &subgroup->triangle);
		else
			subgroup->triangle_count +=
			    triangulate_concave(subgroup->index + face->offset, subgroup->corner, obj->vertex,
			                        face->count, &subgroup->triangle);
	}
	return 0;
}

int
obj_triangulate(obj_t* obj) {
	if (!obj)
		return -1;
	for (unsigned int igroup = 0, gsize = array_size(obj->group); igroup < gsize; ++igroup) {
		obj_group_t* group = obj->group[igroup];
		for (unsigned int isubgroup = 0, sgsize = array_size(group->subgroup); isubgroup < sgsize;
		     ++isubgroup) {
			obj_subgroup_t* subgroup = group->subgroup[isubgroup];
			if (subgroup->triangle_count)
				continue;
			int err = obj_triangulate_subgroup(obj, subgroup);
			if (err)
				return err;
		}
	}
	return 0;
}
