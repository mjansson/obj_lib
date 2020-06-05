/* mesh.c  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson
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

#include <obj/mesh.h>

#include <mesh/mesh.h>
#include <foundation/bucketarray.h>
#include <foundation/array.h>
#include <foundation/log.h>
#include <vector/vector.h>

mesh_t*
obj_to_mesh(obj_t* obj) {
	if (!obj)
		return nullptr;

	size_t total_triangle_count = 0;
	for (unsigned int igroup = 0, gsize = array_size(obj->group); igroup < gsize; ++igroup) {
		obj_group_t* group = obj->group[igroup];
		for (unsigned int isub = 0, sgsize = array_size(group->subgroup); isub < sgsize; ++isub) {
			obj_subgroup_t* subgroup = group->subgroup[isub];
			total_triangle_count += subgroup->triangle.count;
		}
	}

	mesh_t* mesh = mesh_allocate(obj->vertex.count, total_triangle_count);

	// Vertex data
	for (size_t ivertex = 0; ivertex < obj->vertex.count; ++ivertex) {
		obj_vertex_t* obj_vertex = bucketarray_get(&obj->vertex, ivertex);
		mesh_coordinate_t coordinate;
		coordinate.v = vector(obj_vertex->x, obj_vertex->y, obj_vertex->z, 1.0);
		bucketarray_push(&mesh->coordinate, &coordinate);
	}

	for (size_t inormal = 0; inormal < obj->normal.count; ++inormal) {
		obj_normal_t* obj_normal = bucketarray_get(&obj->normal, inormal);
		mesh_normal_t normal;
		normal.v = vector(obj_normal->nx, obj_normal->ny, obj_normal->nz, 0.0);
		bucketarray_push(&mesh->normal, &normal);
	}

	for (size_t iuv = 0; iuv < obj->uv.count; ++iuv) {
		obj_uv_t* obj_normal = bucketarray_get(&obj->uv, iuv);
		mesh_uv_t uv;
		uv.u = obj_normal->u;
		uv.v = obj_normal->v;
		bucketarray_push(&mesh->uv[0], &uv);
	}

	// Triangle data
	for (unsigned int igroup = 0, gsize = array_size(obj->group); igroup < gsize; ++igroup) {
		obj_group_t* group = obj->group[igroup];
		for (unsigned int isub = 0, sgsize = array_size(group->subgroup); isub < sgsize; ++isub) {
			obj_subgroup_t* subgroup = group->subgroup[isub];
			for (size_t itri = 0; itri < subgroup->triangle.count; ++itri) {
				obj_triangle_t* obj_triangle = bucketarray_get(&subgroup->triangle, itri);
				obj_corner_t* obj_corner[3] = {bucketarray_get(&subgroup->corner, obj_triangle->index[0]),
				                               bucketarray_get(&subgroup->corner, obj_triangle->index[1]),
				                               bucketarray_get(&subgroup->corner, obj_triangle->index[2])};

				mesh_triangle_t triangle = {0};
				mesh_vertex_t vertex = {0};
				vertex.coordinate = (obj_corner[0]->vertex > 0 ? (obj_corner[0]->vertex - 1) : 0);
				vertex.normal = (obj_corner[0]->normal > 0 ? (obj_corner[0]->normal - 1) : 0);
				vertex.uv[0] = (obj_corner[0]->uv > 0 ? (obj_corner[0]->uv - 1) : 0);
				triangle.vertex[0] = (unsigned int)mesh->vertex.count;
				bucketarray_push(&mesh->vertex, &vertex);

				vertex.coordinate = (obj_corner[1]->vertex > 0 ? (obj_corner[1]->vertex - 1) : 0);
				vertex.normal = (obj_corner[1]->normal > 0 ? (obj_corner[1]->normal - 1) : 0);
				vertex.uv[0] = (obj_corner[1]->uv > 0 ? (obj_corner[1]->uv - 1) : 0);
				triangle.vertex[1] = (unsigned int)mesh->vertex.count;
				bucketarray_push(&mesh->vertex, &vertex);

				vertex.coordinate = (obj_corner[2]->vertex > 0 ? (obj_corner[2]->vertex - 1) : 0);
				vertex.normal = (obj_corner[2]->normal > 0 ? (obj_corner[2]->normal - 1) : 0);
				vertex.uv[0] = (obj_corner[2]->uv > 0 ? (obj_corner[2]->uv - 1) : 0);
				triangle.vertex[2] = (unsigned int)mesh->vertex.count;
				bucketarray_push(&mesh->vertex, &vertex);

				bucketarray_push(&mesh->triangle, &triangle);
			}
		}
	}

	return mesh;
}

void
obj_from_mesh(obj_t* obj, mesh_t* mesh) {
	if (!obj || !mesh)
		return;

	log_error(HASH_OBJ, ERROR_NOT_IMPLEMENTED, STRING_CONST("OBJ from mesh not yet implemented"));
}
