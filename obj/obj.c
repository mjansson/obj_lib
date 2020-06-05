/* obj.c  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson
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

#include "obj.h"

#include <foundation/array.h>
#include <foundation/stream.h>
#include <foundation/path.h>
#include <foundation/bucketarray.h>

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
                        const struct json_token_t* tokens, size_t tokens_count) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(path_size);
	FOUNDATION_UNUSED(buffer);
	FOUNDATION_UNUSED(size);
	FOUNDATION_UNUSED(tokens);
	FOUNDATION_UNUSED(tokens_count);
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
			bucketarray_finalize(&subgroup->triangle);
			bucketarray_finalize(&subgroup->index);
			bucketarray_finalize(&subgroup->face);
			bucketarray_finalize(&subgroup->corner);
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
	bucketarray_finalize(&obj->vertex);
	bucketarray_finalize(&obj->normal);
	bucketarray_finalize(&obj->uv);

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

static void
set_color(obj_color_t* color, real red, real green, real blue) {
	color->red = red;
	color->green = green;
	color->blue = blue;
}

static obj_color_t
parse_color(const string_const_t* tokens, size_t tokens_count) {
	obj_color_t color = {0};
	if (tokens_count)
		color.red = string_to_real(STRING_ARGS(tokens[0]));
	if (tokens_count > 1)
		color.green = string_to_real(STRING_ARGS(tokens[1]));
	else
		color.green = color.red;
	if (tokens_count > 2)
		color.blue = string_to_real(STRING_ARGS(tokens[2]));
	else
		color.blue = color.green;
	return color;
}

static obj_material_t
material_default(void) {
	obj_material_t material;
	memset(&material, 0, sizeof(material));
	set_color(&material.ambient_color, 0, 0, 0);
	set_color(&material.diffuse_color, 1, 1, 1);
	set_color(&material.specular_color, 0, 0, 0);
	set_color(&material.emissive_color, 0, 0, 0);
	set_color(&material.transmission_filter, 0, 0, 0);
	material.dissolve_factor = 1;
	material.shininess_exponent = 1;
	return material;
}

static bool
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
			string_t testpath = path_allocate_concat(STRING_ARGS(_obj_config.search_path[ipath]), filename, length);
			stream = stream_open(STRING_ARGS(testpath), STREAM_IN);
			string_deallocate(testpath.str);
		}
	}
	if (!stream)
		return false;

	const size_t buffer_capacity = 65000;
	char* buffer = memory_allocate(HASH_OBJ, buffer_capacity, 0, MEMORY_PERSISTENT);

	bool material_valid = false;
	obj_material_t material = material_default();

	while (!stream_eos(stream)) {
		size_t last_remain = 0;
		size_t was_read = stream_read(stream, buffer, buffer_capacity);

		string_const_t remain = {buffer, was_read};
		string_const_t tokens_storage[64];
		const size_t tokens_capacity = sizeof(tokens_storage) / sizeof(tokens_storage[0]);
		string_const_t* tokens = tokens_storage + 1;

		remain = skip_whitespace_and_endline(STRING_ARGS(remain));
		while (remain.length > 3) {
			size_t tokens_count = 0;
			size_t offset = 1;

			while (remain.length && (offset < remain.length)) {
				if (is_whitespace(remain.str[offset]) || is_endline(remain.str[offset])) {
					if (offset && (tokens_count < tokens_capacity)) {
						tokens_storage[tokens_count++] = string_const(remain.str, offset);
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
				if (tokens_count < tokens_capacity)
					tokens_storage[tokens_count++] = remain;
			}
			if (!tokens_count)
				break;

			string_const_t command = tokens_storage[0];
			--tokens_count;

			if (string_equal(STRING_ARGS(command), STRING_CONST("newmtl"))) {
				if (material_valid)
					array_push(obj->material, material);
				else
					obj_finalize_material(&material);
				material = material_default();
				material.name = (tokens_count && tokens[0].length) ? string_clone(STRING_ARGS(tokens[0])) :
				                                                     string_clone(STRING_CONST("__unnamed"));
				material_valid = true;
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("d")) && tokens_count) {
				material.dissolve_factor = string_to_real(STRING_ARGS(tokens[0]));
			} else if (tokens_count && (command.str[0] == 'K')) {
				if (string_equal(STRING_ARGS(command), STRING_CONST("Ka"))) {
					material.ambient_color = parse_color(tokens, tokens_count);
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("Kd"))) {
					material.diffuse_color = parse_color(tokens, tokens_count);
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("Ks"))) {
					material.specular_color = parse_color(tokens, tokens_count);
				} else if (string_equal(STRING_ARGS(command), STRING_CONST("Ke"))) {
					material.emissive_color = parse_color(tokens, tokens_count);
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("Ns")) && tokens_count) {
				material.shininess_exponent = string_to_real(STRING_ARGS(tokens[0]));
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("Tf")) && tokens_count) {
				material.transmission_filter = parse_color(tokens, tokens_count);
			} else if (tokens_count && (command.str[0] == 'm')) {
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
			if (end_line < remain.length)
				remain.length -= end_line;
			else
				remain.length = 0;

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

	return true;
}

bool
obj_read(obj_t* obj, stream_t* stream) {
	size_t file_size = stream_size(stream);
	size_t estimated_vertex_count = file_size / 200;
	size_t reserve_vertex_count = estimated_vertex_count / 8;
	if (estimated_vertex_count < 1024) {
		estimated_vertex_count = 1024;
		reserve_vertex_count = estimated_vertex_count;
	}

	obj_finalize_groups(obj);
	obj_finalize_materials(obj);

	bucketarray_finalize(&obj->vertex);
	bucketarray_finalize(&obj->normal);
	bucketarray_finalize(&obj->uv);

	bucketarray_initialize(&obj->vertex, sizeof(obj_vertex_t), reserve_vertex_count);
	bucketarray_reserve(&obj->vertex, reserve_vertex_count);

	bucketarray_initialize(&obj->normal, sizeof(obj_normal_t), reserve_vertex_count);
	bucketarray_initialize(&obj->uv, sizeof(obj_uv_t), reserve_vertex_count);

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

	size_t vertex_count_since_group = 0;
	string_t group_name = {0};
	unsigned int material_index = INVALID_INDEX;

	bucketarray_t vertex_to_corner;
	bucketarray_initialize(&vertex_to_corner, sizeof(int), reserve_vertex_count);
	bucketarray_reserve(&vertex_to_corner, reserve_vertex_count);

	while (!stream_eos(stream)) {
		size_t last_remain = 0;
		size_t was_read = stream_read(stream, buffer, buffer_capacity);

		string_const_t remain = {buffer, was_read};
		string_const_t* tokens = tokens_storage + 1;

		remain = skip_whitespace_and_endline(STRING_ARGS(remain));
		while (remain.length > 3) {
			size_t tokens_count = 0;
			size_t offset = 1;

			while (remain.length && (offset < remain.length)) {
				if (is_whitespace(remain.str[offset]) || is_endline(remain.str[offset])) {
					if (offset && (tokens_count < tokens_capacity))
						tokens_storage[tokens_count++] = string_const(remain.str, offset);
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
				if (tokens_count < tokens_capacity)
					tokens_storage[tokens_count++] = remain;
			}
			if (!tokens_count)
				break;

			string_const_t command = tokens_storage[0];
			--tokens_count;

			if (string_equal(STRING_ARGS(command), STRING_CONST("v"))) {
				if (tokens_count >= 2) {
					obj_vertex_t vertex = {string_to_real(STRING_ARGS(tokens[0])),
					                       string_to_real(STRING_ARGS(tokens[1])),
					                       (tokens_count > 2) ? string_to_real(STRING_ARGS(tokens[2])) : 0.0f};
					bucketarray_push(&obj->vertex, &vertex);
				} else {
					obj_vertex_t vertex = {0, 0, 0};
					bucketarray_push(&obj->vertex, &vertex);
				}
				++vertex_count_since_group;
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("vt"))) {
				if (!obj->uv.bucket_count)
					bucketarray_reserve(&obj->uv, reserve_vertex_count);
				if (tokens_count >= 2) {
					obj_uv_t uv = {string_to_real(STRING_ARGS(tokens[0])), string_to_real(STRING_ARGS(tokens[1]))};
					bucketarray_push(&obj->uv, &uv);
				} else {
					obj_uv_t uv = {0, 0};
					bucketarray_push(&obj->uv, &uv);
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("vn"))) {
				if (!obj->normal.bucket_count)
					bucketarray_reserve(&obj->normal, reserve_vertex_count);
				if (tokens_count >= 3) {
					obj_normal_t normal = {string_to_real(STRING_ARGS(tokens[0])),
					                       string_to_real(STRING_ARGS(tokens[1])),
					                       string_to_real(STRING_ARGS(tokens[2]))};
					bucketarray_push(&obj->normal, &normal);
				} else {
					obj_normal_t normal = {0, 0, 0};
					bucketarray_push(&obj->normal, &normal);
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("f")) && (tokens_count > 2)) {
				size_t corners_count = tokens_count;

				if (!current_group) {
					current_group =
					    memory_allocate(HASH_OBJ, sizeof(obj_group_t), 0, MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
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

					size_t estimated_triangles = (vertex_count_since_group * 3) / 4;
					size_t estimated_corners = estimated_triangles * 3;

					size_t min_bucket_size = 1024;
					if ((estimated_triangles / 4) > min_bucket_size)
						min_bucket_size = (estimated_triangles / 4);

					bucketarray_initialize(&current_subgroup->face, sizeof(obj_face_t), min_bucket_size);
					bucketarray_reserve(&current_subgroup->face, estimated_triangles / 2);

					bucketarray_initialize(&current_subgroup->triangle, sizeof(obj_triangle_t), min_bucket_size);

					min_bucket_size = 1024;
					if ((estimated_corners / 4) > min_bucket_size)
						min_bucket_size = (estimated_corners / 4);

					bucketarray_initialize(&current_subgroup->index, sizeof(unsigned int), min_bucket_size);
					bucketarray_reserve(&current_subgroup->index, estimated_corners / 2);

					bucketarray_initialize(&current_subgroup->corner, sizeof(obj_corner_t), min_bucket_size);
					bucketarray_reserve(&current_subgroup->corner, estimated_corners / 2);

					current_subgroup->material = material_index;

					bucketarray_clear(&vertex_to_corner);

					vertex_count_since_group = 0;
				}

				size_t last_index_count = current_subgroup->index.count;
				obj_face_t face = {0, (unsigned int)last_index_count};
				bool valid_face = (corners_count >= 3);
				for (size_t icorner = 0; valid_face && (icorner < corners_count); ++icorner) {
					string_const_t corner_token[3];
					size_t corner_tokens_count =
					    string_explode(STRING_ARGS(tokens[icorner]), STRING_CONST("/"), corner_token, 3, true);

					int relvert = 0;
					int reluv = 0;
					int relnorm = 0;
					if (corner_tokens_count)
						relvert = string_to_int(STRING_ARGS(corner_token[0]));
					if (corner_tokens_count > 1)
						reluv = string_to_int(STRING_ARGS(corner_token[1]));
					if (corner_tokens_count > 2)
						relnorm = string_to_int(STRING_ARGS(corner_token[2]));

					if (relvert < 0)
						relvert += (int)obj->vertex.count + 1;
					if (relnorm < 0)
						relnorm = (int)obj->normal.count + 1;
					if (reluv < 0)
						reluv = (int)obj->uv.count + 1;

					if ((relvert <= 0) || (relvert > (int)obj->vertex.count))
						valid_face = false;
					if ((relnorm <= 0) || (relnorm > (int)obj->normal.count))
						relnorm = 0;
					if ((reluv <= 0) || (reluv > (int)obj->uv.count))
						reluv = 0;

					if (valid_face) {
						size_t corner_index;
						unsigned int ivert = (unsigned int)relvert;
						unsigned int inorm = (unsigned int)relnorm;
						unsigned int iuv = (unsigned int)reluv;
						if ((ivert > vertex_to_corner.count) ||
						    (*bucketarray_get_as(int, &vertex_to_corner, ivert - 1) < 0)) {
							obj_corner_t corner = {ivert, inorm, iuv, -1};
							corner_index = current_subgroup->corner.count;
							bucketarray_push(&current_subgroup->corner, &corner);
							if (ivert > vertex_to_corner.count)
								bucketarray_resize_fill(&vertex_to_corner, ivert, 0xff);
							*bucketarray_get_as(int, &vertex_to_corner, ivert - 1) = (int)corner_index;
						} else {
							corner_index = (size_t)*bucketarray_get_as(int, &vertex_to_corner, ivert - 1);
							size_t last_corner_index = (size_t)-1;
							while (corner_index < current_subgroup->corner.count) {
								obj_corner_t* corner = bucketarray_get(&current_subgroup->corner, corner_index);
								if (!corner->normal || !inorm || (corner->normal == inorm)) {
									if (!corner->uv || !iuv || (corner->uv == iuv)) {
										if (inorm && !corner->normal)
											corner->normal = inorm;
										if (iuv && !corner->uv)
											corner->uv = iuv;
										break;
									}
								}
								corner_index = (size_t)corner->next;
								last_corner_index = corner_index;
							}
							if (corner_index >= current_subgroup->corner.count) {
								obj_corner_t corner = {ivert, inorm, iuv, -1};
								corner_index = current_subgroup->corner.count;
								bucketarray_push(&current_subgroup->corner, &corner);
								if (last_corner_index < corner_index) {
									obj_corner_t* last_corner =
									    bucketarray_get(&current_subgroup->corner, last_corner_index);
									last_corner->next = (int)corner_index;
								}
							}
						}
						bucketarray_push(&current_subgroup->index, &corner_index);
						++face.count;
					}
				}

				if (valid_face) {
					bucketarray_push(&current_subgroup->face, &face);
				} else {
					bucketarray_resize(&current_subgroup->index, last_index_count);
				}
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("mtllib")) && tokens_count) {
				load_material_lib(obj, STRING_ARGS(tokens[0]));
			} else if (string_equal(STRING_ARGS(command), STRING_CONST("usemtl")) && tokens_count) {
				string_const_t name = tokens[0];
				unsigned int next_material = INVALID_INDEX;
				for (unsigned int imat = 0, msize = array_size(obj->material); imat < msize; ++imat) {
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
				group_name = (tokens_count && tokens[0].length) ? string_clone_string(tokens[0]) :
				                                                  string_clone(STRING_CONST("__unnamed"));
				current_group = nullptr;
			}

			remain.str += end_line;
			remain.length = (remain.length > end_line) ? (remain.length - end_line) : 0;

			remain = skip_whitespace_and_endline(STRING_ARGS(remain));
			last_remain = remain.length;
		}

		if (!stream_eos(stream) && last_remain)
			stream_seek(stream, -(ssize_t)last_remain, STREAM_SEEK_CURRENT);
	}

	bucketarray_finalize(&vertex_to_corner);

	string_deallocate(group_name.str);
	memory_deallocate(buffer);

	return true;
}

bool
obj_write(const obj_t* obj, stream_t* stream) {
	FOUNDATION_UNUSED(obj);
	FOUNDATION_UNUSED(stream);
	return false;
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
vec_dot(const real* FOUNDATION_RESTRICT first_normal, const real* FOUNDATION_RESTRICT second_normal) {
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
polygon_convex(bucketarray_t* index, unsigned int base_offset, bucketarray_t* corner, bucketarray_t* vertex,
               unsigned int corner_count) {
	if (corner_count < 4)
		return true;

	unsigned int prev_corner;
	unsigned int cur_corner = *bucketarray_get_as(unsigned int, index, base_offset);
	unsigned int next_corner = *bucketarray_get_as(unsigned int, index, base_offset + 1);

	obj_vertex_t* next_vertex =
	    bucketarray_get(vertex, bucketarray_get_as(obj_corner_t, corner, next_corner)->vertex - 1);
	obj_vertex_t* cur_vertex =
	    bucketarray_get(vertex, bucketarray_get_as(obj_corner_t, corner, cur_corner)->vertex - 1);

	real edge[3];
	vertex_sub(cur_vertex, next_vertex, edge);

	real last_normal[3] = {0, 0, 0};
	real last_edge[3];

	bool first_normal = true;
	for (unsigned int icorner = 0; icorner < corner_count; ++icorner) {
		prev_corner = cur_corner;
		cur_corner = next_corner;
		next_corner = *bucketarray_get_as(unsigned int, index, base_offset + ((icorner + 2) % corner_count));

		if ((prev_corner == cur_corner) || (prev_corner == next_corner) || (cur_corner == next_corner))
			continue;

		last_edge[0] = edge[0];
		last_edge[1] = edge[1];
		last_edge[2] = edge[2];

		cur_vertex = next_vertex;
		next_vertex = bucketarray_get(vertex, bucketarray_get_as(obj_corner_t, corner, next_corner)->vertex - 1);
		vertex_sub(cur_vertex, next_vertex, edge);

		real normal[3];
		vec_cross(last_edge, edge, normal);
		if (!first_normal && (vec_dot(last_normal, normal) < 0))
			return false;
		if (first_normal &&
		    (!math_real_is_zero(normal[0]) || !math_real_is_zero(normal[1]) || !math_real_is_zero(normal[2]))) {
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
triangulate_convex(bucketarray_t* index, unsigned int base_offset, bucketarray_t* corner, bucketarray_t* vertex,
                   unsigned int corner_count, bucketarray_t* triangle) {
	FOUNDATION_UNUSED(vertex);
	FOUNDATION_UNUSED(corner);
	unsigned int triangle_count = 0;
	unsigned int base = 1;
	unsigned int first_index = *bucketarray_get_as(unsigned int, index, base_offset);
	unsigned int next_index = *bucketarray_get_as(unsigned int, index, base_offset + base);
	while (corner_count >= 3) {
		unsigned int last_index = *bucketarray_get_as(unsigned int, index, base_offset + base + 1);
		obj_triangle_t new_triangle = {first_index, next_index, last_index};
		bucketarray_push(triangle, &new_triangle);
		++base;
		++triangle_count;
		--corner_count;
		next_index = last_index;
	}
	return triangle_count;
}

static unsigned int
triangulate_concave(bucketarray_t* index, unsigned int base_offset, bucketarray_t* corner, bucketarray_t* vertex,
                    unsigned int index_count, bucketarray_t* triangle) {
	if (index_count < 3)
		return 0;

	real xaxis[3];
	real yaxis[3];
	real normal[3];
	unsigned int cur_index = 0;
	unsigned int cur_corner_index = *bucketarray_get_as(unsigned int, index, base_offset + cur_index);
	obj_corner_t* cur_corner = bucketarray_get(corner, cur_corner_index);
	do {
		unsigned int next_index = cur_index + 1;
		unsigned int next_corner_index = *bucketarray_get_as(unsigned int, index, base_offset + next_index);
		unsigned int last_corner_index =
		    *bucketarray_get_as(unsigned int, index, base_offset + ((cur_index + 2) % index_count));
		obj_corner_t* next_corner = bucketarray_get(corner, next_corner_index);
		obj_corner_t* last_corner = bucketarray_get(corner, last_corner_index);
		vertex_sub(bucketarray_get(vertex, next_corner->vertex - 1), bucketarray_get(vertex, last_corner->vertex - 1),
		           xaxis);
		vertex_sub(bucketarray_get(vertex, next_corner->vertex - 1), bucketarray_get(vertex, cur_corner->vertex - 1),
		           yaxis);
		vec_cross(xaxis, yaxis, normal);
		if (!math_real_is_zero(normal[0]) || !math_real_is_zero(normal[1]) || !math_real_is_zero(normal[2])) {
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

	cur_corner = bucketarray_get(corner, *bucketarray_get_as(unsigned int, index, base_offset));
	obj_vertex_t* origo = bucketarray_get(vertex, cur_corner->vertex - 1);

	// Project polygon on plane
	coord[0] = 0;
	coord[1] = 0;
	local[0] = 0;
	for (unsigned int icorner = 1; icorner < index_count; ++icorner) {
		real diff[3];
		cur_corner = bucketarray_get(corner, *bucketarray_get_as(unsigned int, index, base_offset + icorner));
		vertex_sub(origo, bucketarray_get(vertex, cur_corner->vertex - 1), diff);
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
				memmove(local + base, local + base + 1, sizeof(unsigned int) * (local_count - base - 1));
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

			if (((winding < 0) && (triangle_winding >= 0)) || ((winding > 0) && (triangle_winding <= 0))) {
				++base;
				continue;
			}

			bool point_inside = false;
			for (unsigned int ipt = 0; !point_inside && (ipt < local_count); ++ipt) {
				if ((ipt != prev) && (ipt != base) && (ipt != next))
					point_inside = point_inside_triangle_2d(coord + (i0 * 2), coord + (i1 * 2), coord + (i2 * 2),
					                                        coord + (local[ipt] * 2));
			}

			if (point_inside) {
				++base;
				continue;
			}

			obj_triangle_t new_triangle = {*bucketarray_get_as(unsigned int, index, base_offset + i0),
			                               *bucketarray_get_as(unsigned int, index, base_offset + i1),
			                               *bucketarray_get_as(unsigned int, index, base_offset + i2)};
			bucketarray_push(triangle, &new_triangle);
			++triangle_count;

			memmove(local + base, local + base + 1, sizeof(unsigned int) * (local_count - base - 1));
			base = 1;
			--local_count;

		} while ((base != local_count) && (local_count >= 3));

		if (base == local_count)
			break;  // Only degenerate zero area triangles left
	}

	return triangle_count;
}

static bool
obj_triangulate_subgroup(obj_t* obj, obj_subgroup_t* subgroup) {
	bucketarray_reserve(&subgroup->triangle, 3 * subgroup->face.count);
	bucketarray_resize(&subgroup->triangle, 0);

	for (size_t iface = 0, fsize = subgroup->face.count; iface < fsize; ++iface) {
		obj_face_t* face = bucketarray_get(&subgroup->face, iface);
		bool convex = polygon_convex(&subgroup->index, face->offset, &subgroup->corner, &obj->vertex, face->count);

		if (convex)
			triangulate_convex(&subgroup->index, face->offset, &subgroup->corner, &obj->vertex, face->count,
			                   &subgroup->triangle);
		else
			triangulate_concave(&subgroup->index, face->offset, &subgroup->corner, &obj->vertex, face->count,
			                    &subgroup->triangle);
	}
	return true;
}

bool
obj_triangulate(obj_t* obj) {
	if (!obj)
		return false;
	for (unsigned int igroup = 0, gsize = array_size(obj->group); igroup < gsize; ++igroup) {
		obj_group_t* group = obj->group[igroup];
		for (unsigned int isubgroup = 0, sgsize = array_size(group->subgroup); isubgroup < sgsize; ++isubgroup) {
			obj_subgroup_t* subgroup = group->subgroup[isubgroup];
			if (subgroup->triangle.count)
				continue;
			if (!obj_triangulate_subgroup(obj, subgroup))
				return false;
		}
	}
	return true;
}
