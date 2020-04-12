/* main.c  -  OBJ library  -  Public Domain  -  2019 Mattias Jansson
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

#include <obj/obj.h>

#include <foundation/foundation.h>
#include <test/test.h>

static application_t
test_obj_application(void) {
	application_t app;
	memset(&app, 0, sizeof(app));
	app.name = string_const(STRING_CONST("OBJ tests"));
	app.short_name = string_const(STRING_CONST("test_obj"));
	app.company = string_const(STRING_CONST(""));
	app.flags = APPLICATION_UTILITY;
	app.exception_handler = test_exception_handler;
	return app;
}

static memory_system_t
test_obj_memory_system(void) {
	return memory_system_malloc();
}

static foundation_config_t
test_obj_foundation_config(void) {
	foundation_config_t config;
	memset(&config, 0, sizeof(config));
	return config;
}

static int
test_obj_initialize(void) {
	obj_config_t config;
	memset(&config, 0, sizeof(config));
	log_set_suppress(HASH_OBJ, ERRORLEVEL_INFO);
	return obj_module_initialize(config);
}

static void
test_obj_finalize(void) {
	obj_module_finalize();
}

static void
test_obj_declare(void) {
}

static test_suite_t test_obj_suite = {test_obj_application,
                                      test_obj_memory_system,
                                      test_obj_foundation_config,
                                      test_obj_declare,
                                      test_obj_initialize,
                                      test_obj_finalize,
                                      0};

#if BUILD_MONOLITHIC

int
test_obj_run(void);

int
test_obj_run(void) {
	test_suite = test_obj_suite;
	return test_run_all();
}

#else

test_suite_t
test_suite_define(void);

test_suite_t
test_suite_define(void) {
	return test_obj_suite;
}

#endif
