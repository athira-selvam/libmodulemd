/*
 * This file is part of libmodulemd
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * Fedora-License-Identifier: MIT
 * SPDX-2.0-License-Identifier: MIT
 * SPDX-3.0-License-Identifier: MIT
 *
 * This program is free software.
 * For more information on the license, see COPYING.
 * For more information on free software, see <https://www.gnu.org/philosophy/free-sw.en.html>.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <signal.h>

#include "modulemd-defaults.h"
#include "modulemd-module.h"
#include "modulemd-module-stream.h"
#include "modulemd-module-stream-v2.h"
#include "private/glib-extensions.h"
#include "private/modulemd-defaults-v1-private.h"
#include "private/modulemd-module-private.h"
#include "private/modulemd-yaml.h"
#include "private/test-utils.h"

typedef struct _ModuleFixture
{
} ModuleFixture;

extern int modulemd_test_signal;

static void
module_test_construct (ModuleFixture *fixture, gconstpointer user_data)
{
  g_autoptr (ModulemdModule) m = NULL;
  g_autoptr (GPtrArray) list = NULL;
  g_autoptr (GError) error = NULL;

  /* Test that the new() function works */
  m = modulemd_module_new ("testmodule");
  g_assert_nonnull (m);
  g_assert_true (MODULEMD_IS_MODULE (m));
  g_assert_true (modulemd_module_validate (m, &error));
  g_assert_no_error (error);
  g_assert_cmpstr (modulemd_module_get_module_name (m), ==, "testmodule");
  g_assert_null (modulemd_module_get_defaults (m));
  list = modulemd_module_get_streams_by_stream_name_as_list (m, "teststream");
  g_assert_cmpint (list->len, ==, 0);
  g_clear_pointer (&list, g_ptr_array_unref);
  g_assert_null (modulemd_module_get_stream_by_NSVC (m, "test", 42, "test"));
  g_clear_object (&m);

  /* Test that object instantiation works wiht a name */
  m = g_object_new (MODULEMD_TYPE_MODULE, "module-name", "testmodule", NULL);
  g_assert_true (MODULEMD_IS_MODULE (m));
  g_assert_cmpstr (modulemd_module_get_module_name (m), ==, "testmodule");
  g_clear_object (&m);

  /* Test that we abort with a NULL name to new() */
  modulemd_test_signal = 0;
  signal (SIGTRAP, modulemd_test_signal_handler);
  m = modulemd_module_new (NULL);
  g_assert_cmpint (modulemd_test_signal, ==, SIGTRAP);
  g_clear_object (&m);

  /* Test that we abort if we instantiate without a name */
  modulemd_test_signal = 0;
  signal (SIGTRAP, modulemd_test_signal_handler);
  m = g_object_new (MODULEMD_TYPE_MODULE, NULL);
  g_assert_cmpint (modulemd_test_signal, ==, SIGTRAP);
  g_clear_object (&m);

  /* test that we abort if we instantiate with a NULL name */
  modulemd_test_signal = 0;
  signal (SIGTRAP, modulemd_test_signal_handler);
  m = g_object_new (MODULEMD_TYPE_MODULE, "module-name", NULL, NULL);
  g_assert_cmpint (modulemd_test_signal, ==, SIGTRAP);
  g_clear_object (&m);
}


static void
module_test_defaults (ModuleFixture *fixture, gconstpointer user_data)
{
  g_autoptr (ModulemdModule) m = NULL;
  g_autoptr (ModulemdDefaultsV1) d = NULL;
  g_autoptr (GError) nested_error = NULL;
  ModulemdDefaults *d_got = NULL;

  m = modulemd_module_new ("testmodule");

  /* Verify that setting defaults that don't match this module name fails and
   * returns an error
   */
  d = modulemd_defaults_v1_new ("test");
  g_assert_nonnull (d);

  g_assert_cmpint (
    modulemd_module_set_defaults (
      m, MODULEMD_DEFAULTS (d), MD_DEFAULTS_VERSION_UNSET, &nested_error),
    ==,
    MD_DEFAULTS_VERSION_ERROR);
  g_assert_nonnull (nested_error);
  g_clear_object (&d);
  g_clear_pointer (&nested_error, g_error_free);

  d = modulemd_defaults_v1_new ("testmodule");
  g_assert_nonnull (d);
  g_assert_cmpint (
    modulemd_module_set_defaults (
      m, MODULEMD_DEFAULTS (d), MD_DEFAULTS_VERSION_UNSET, NULL),
    ==,
    MD_DEFAULTS_VERSION_ONE);

  d_got = modulemd_module_get_defaults (m);
  g_assert_nonnull (d_got);
  g_assert_cmpstr (
    modulemd_defaults_get_module_name (d_got), ==, "testmodule");

  g_assert_cmpint (
    modulemd_module_set_defaults (m, NULL, MD_DEFAULTS_VERSION_UNSET, NULL),
    ==,
    MD_DEFAULTS_VERSION_UNSET);
  g_assert_null (modulemd_module_get_defaults (m));
}


static void
module_test_streams (ModuleFixture *fixture, gconstpointer user_data)
{
  g_autoptr (ModulemdModule) m = modulemd_module_new ("testmodule");
  g_autoptr (ModulemdTranslation) t = NULL;
  g_autoptr (ModulemdTranslationEntry) te = NULL;
  ModulemdModuleStream *stream = NULL;
  GPtrArray *list = NULL;

  /* Create a translation pre-adding streams */
  te = modulemd_translation_entry_new ("nl_NL");
  modulemd_translation_entry_set_summary (te, "Een test omschrijving");
  t = modulemd_translation_new (1, "testmodule", "stream1", 42);
  modulemd_translation_set_translation_entry (t, te);
  g_clear_pointer (&te, g_object_unref);
  modulemd_module_add_translation (m, t);
  g_clear_pointer (&t, g_object_unref);

  /* Create and add some streams to cross */
  stream = modulemd_module_stream_new (2, "testmodule", "stream1");
  modulemd_module_stream_set_version (stream, 1);
  modulemd_module_stream_set_context (stream, "context1");
  modulemd_module_stream_v2_set_summary (MODULEMD_MODULE_STREAM_V2 (stream),
                                         "Stream 1");
  g_assert_cmpint (modulemd_module_add_stream (
                     m, stream, MD_MODULESTREAM_VERSION_UNSET, NULL),
                   ==,
                   MD_MODULESTREAM_VERSION_TWO);
  g_clear_object (&stream);
  stream = modulemd_module_stream_new (2, "testmodule", "stream1");
  modulemd_module_stream_set_version (stream, 3);
  modulemd_module_stream_set_context (stream, "context2");
  modulemd_module_stream_v2_set_summary (MODULEMD_MODULE_STREAM_V2 (stream),
                                         "Stream 1");
  g_assert_cmpint (modulemd_module_add_stream (
                     m, stream, MD_MODULESTREAM_VERSION_UNSET, NULL),
                   ==,
                   MD_MODULESTREAM_VERSION_TWO);
  g_clear_object (&stream);
  stream = modulemd_module_stream_new (2, "testmodule", "stream1");
  modulemd_module_stream_set_version (stream, 1);
  modulemd_module_stream_set_context (stream, "context2");
  modulemd_module_stream_v2_set_summary (MODULEMD_MODULE_STREAM_V2 (stream),
                                         "Stream 1");
  g_assert_cmpint (modulemd_module_add_stream (
                     m, stream, MD_MODULESTREAM_VERSION_UNSET, NULL),
                   ==,
                   MD_MODULESTREAM_VERSION_TWO);
  g_clear_object (&stream);
  stream = modulemd_module_stream_new (2, "testmodule", "stream2");
  modulemd_module_stream_set_version (stream, 42);
  modulemd_module_stream_set_context (stream, "context42");
  modulemd_module_stream_v2_set_summary (MODULEMD_MODULE_STREAM_V2 (stream),
                                         "Stream 2");
  g_assert_cmpint (modulemd_module_add_stream (
                     m, stream, MD_MODULESTREAM_VERSION_UNSET, NULL),
                   ==,
                   MD_MODULESTREAM_VERSION_TWO);
  g_clear_object (&stream);

  /* Create a translation post-adding streams */
  te = modulemd_translation_entry_new ("en_GB");
  modulemd_translation_entry_set_summary (te, "A test summary");
  t = modulemd_translation_new (1, "testmodule", "stream2", 42);
  modulemd_translation_set_translation_entry (t, te);
  g_clear_pointer (&te, g_object_unref);
  modulemd_module_add_translation (m, t);
  g_clear_pointer (&t, g_object_unref);

  /* Verify that we get all streams */
  list = modulemd_module_get_all_streams (m);
  g_assert_cmpint (list->len, ==, 4);
  list = NULL; /* get_all_streams is transfer none */

  /* Test by_stream_name */
  list =
    modulemd_module_get_streams_by_stream_name_as_list (m, "nosuchstream");
  g_assert_nonnull (list);
  g_assert_cmpint (list->len, ==, 0);
  g_clear_pointer (&list, g_ptr_array_unref);
  list = modulemd_module_get_streams_by_stream_name_as_list (m, "stream2");
  g_assert_nonnull (list);
  g_assert_cmpint (list->len, ==, 1);
  stream = (ModulemdModuleStream *)g_ptr_array_index (list, 0);
  g_assert_cmpstr (
    modulemd_module_stream_get_context (stream), ==, "context42");
  g_clear_pointer (&list, g_ptr_array_unref);

  /* Verify that ordering in the by_stream_name is right */
  list = modulemd_module_get_streams_by_stream_name_as_list (m, "stream1");
  g_assert_nonnull (list);
  g_assert_cmpint (list->len, ==, 3);
  stream = (ModulemdModuleStream *)g_ptr_array_index (list, 0);
  g_assert_nonnull (stream);
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 3);
  stream = (ModulemdModuleStream *)g_ptr_array_index (list, 1);
  g_assert_nonnull (stream);
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 1);
  stream = (ModulemdModuleStream *)g_ptr_array_index (list, 2);
  g_assert_nonnull (stream);
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 1);
  g_clear_pointer (&list, g_ptr_array_unref);

  /* Get streams by NSVC */
  stream = modulemd_module_get_stream_by_NSVC (m, "nosuch", 3, "nosuchctx");
  g_assert_null (stream);
  stream = modulemd_module_get_stream_by_NSVC (m, "stream1", 1, "context1");
  g_assert_nonnull (stream);
  g_assert_cmpstr (
    modulemd_module_stream_get_stream_name (stream), ==, "stream1");
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 1);
  g_assert_cmpstr (
    modulemd_module_stream_get_context (stream), ==, "context1");
  g_assert_cmpstr (modulemd_module_stream_v2_get_summary (
                     MODULEMD_MODULE_STREAM_V2 (stream), NULL),
                   ==,
                   "Stream 1");
  g_assert_cmpstr (modulemd_module_stream_v2_get_summary (
                     MODULEMD_MODULE_STREAM_V2 (stream), "nl_NL"),
                   ==,
                   "Een test omschrijving");
  stream = modulemd_module_get_stream_by_NSVC (m, "stream1", 1, "context2");
  g_assert_nonnull (stream);
  g_assert_cmpstr (
    modulemd_module_stream_get_stream_name (stream), ==, "stream1");
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 1);
  g_assert_cmpstr (
    modulemd_module_stream_get_context (stream), ==, "context2");
  stream = modulemd_module_get_stream_by_NSVC (m, "stream1", 3, "context1");
  g_assert_null (stream);
  stream = modulemd_module_get_stream_by_NSVC (m, "stream1", 3, "context2");
  g_assert_nonnull (stream);
  g_assert_cmpstr (
    modulemd_module_stream_get_stream_name (stream), ==, "stream1");
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 3);
  g_assert_cmpstr (
    modulemd_module_stream_get_context (stream), ==, "context2");
  stream = modulemd_module_get_stream_by_NSVC (m, "stream2", 42, "context42");
  g_assert_nonnull (stream);
  g_assert_cmpstr (
    modulemd_module_stream_get_stream_name (stream), ==, "stream2");
  g_assert_cmpint (modulemd_module_stream_get_version (stream), ==, 42);
  g_assert_cmpstr (
    modulemd_module_stream_get_context (stream), ==, "context42");
  g_assert_cmpstr (modulemd_module_stream_v2_get_summary (
                     MODULEMD_MODULE_STREAM_V2 (stream), NULL),
                   ==,
                   "Stream 2");
  g_assert_cmpstr (modulemd_module_stream_v2_get_summary (
                     MODULEMD_MODULE_STREAM_V2 (stream), "en_GB"),
                   ==,
                   "A test summary");
}


int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://bugzilla.redhat.com/show_bug.cgi?id=");

  // Define the tests.

  g_test_add ("/modulemd/v2/module/construct",
              ModuleFixture,
              NULL,
              NULL,
              module_test_construct,
              NULL);

  g_test_add ("/modulemd/v2/module/defaults",
              ModuleFixture,
              NULL,
              NULL,
              module_test_defaults,
              NULL);

  g_test_add ("/modulemd/v2/module/streams",
              ModuleFixture,
              NULL,
              NULL,
              module_test_streams,
              NULL);

  return g_test_run ();
}
