/* viewer-installer-window-view-model.c
 *
 * Copyright (C) 2020 Hancom Gooroom <gooroom@hancom.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <curl/curl.h>
#include <json-glib/json-glib.h>

#include "define.h"
#include "viewer-installer-config.h"
#include "viewer-installer-window-view-model.h"

#define OUT_PATH "/var/tmp"
#define JSON_FILE "hancom-viewer-installer/viewer-installer-infos.json"

enum
{
    PROP_STATUS= 1,
    PROP_PROGRESS,
    PROP_LAST
};

enum {
    SIGNAL_UPDATE_DATA,
    SIGNAL_LAST
};

typedef struct 
{
    gchar     *error;
    gchar     *package;
    gchar     *file_name;
    gchar     *sha256;
    gchar     *md5;

    guint     status;
    guint     progress;
    guint     install_id;

    gboolean  is_valid;

    GThread   *status_thread;
    GThread   *progress_thread;
    GPtrArray *dependencies;

}ViewerInstallerWindowViewModelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ViewerInstallerWindowViewModel, viewer_installer_window_view_model, G_TYPE_OBJECT)

static GParamSpec *pspec = NULL;
static GMutex status_mutex, progress_mutex;

static size_t
write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

static int xferinfo(void *user_data,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL(user_data), 0);

    guint p;
    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (user_data);

    p = ((double)dlnow / (double)dltotal ) * 100;

    if (priv->progress != p)
    {
        g_object_set (G_OBJECT (user_data), "progress", p, NULL);
    }

    return 0;
}

static size_t
hangul_download_check_cb (char* buffer, size_t size, size_t nmemb, void *user_data)
{
    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL(user_data), 0);

    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (user_data);

    if (buffer)
    {
        gchar **data;
        gchar *sha256;
        data = g_strsplit (buffer, ":", -1);

        if (g_str_has_suffix (data[0], "Checksum"))
        {
            sha256 = g_strstrip (data[1]);

            if (g_strcmp0 (sha256, priv->sha256) == 0)
            {
                priv->is_valid = TRUE;
            }
        }

        if (data)
        {
            g_strfreev (data);
        }
    }
    return nmemb * size;
}

static gboolean
hangul_download_check (void *user_data, gchar* uri)
{
    CURL *curl;
    CURLcode res;
    gboolean result = FALSE;

    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL(user_data), result);

    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (user_data);

    curl = curl_easy_init ();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, uri);
        curl_easy_setopt(curl, CURLOPT_REFERER, VIEWER_REFERER);
        if (priv->md5)
            curl_easy_setopt(curl, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5, priv->md5);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, hangul_download_check_cb);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, user_data);

        res = curl_easy_perform(curl);
    }

    if (res == CURLE_OK)
    {
        result = priv->is_valid;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup ();
    return result;
}

static gboolean
hangul_download_func (gpointer user_data)
{
    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL(user_data), G_SOURCE_REMOVE);

    gchar *out_file;
    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (user_data);

    out_file = g_strdup_printf ("%s/%s", OUT_PATH, priv->file_name);

    if (g_file_test (out_file, G_FILE_TEST_EXISTS))
    {
        unlink (out_file);
    }

    gchar *uri;
    uri = g_strdup_printf ("%s/%s", VIEWER_INSTALL_URL, priv->file_name);

    if (!hangul_download_check (user_data, uri))
    {
        priv->error = g_strdup (_("File is not valid"));
        g_object_set (G_OBJECT (user_data), "status", STATUS_ERROR, NULL);
        return G_SOURCE_REMOVE;
    }

    FILE *fp;
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init ();
    if (curl)
    {
        fp = fopen (out_file, "wb");
        curl_easy_setopt(curl, CURLOPT_URL, uri);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "HancomGooroom");
        curl_easy_setopt(curl, CURLOPT_REFERER, VIEWER_REFERER);

        if (priv->md5)
            curl_easy_setopt(curl, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5, priv->md5);

        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, user_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
    }

    if (res != CURLE_OK)
    {
          g_object_set (G_OBJECT (user_data), "status", STATUS_ERROR, NULL);
    }
    else
    {
          g_object_set (G_OBJECT (user_data), "status", STATUS_DOWNLOADED, NULL);
    }

    g_free (out_file);
    curl_global_cleanup ();

    return G_SOURCE_REMOVE;
}

static gboolean
hangul_install_func  (gpointer user_data)
{
    int i;
    gchar *file;
    gchar *param = g_strdup ("");
    GError *error = NULL;
    ViewerInstallerWindowViewModelPrivate *priv; 
    priv = viewer_installer_window_view_model_get_instance_private (user_data);

    if (priv->dependencies && 0 < priv->dependencies->len)
    {
        for (i = 0; i < priv->dependencies->len; i++)
        {
            gchar *tmp = g_strdup (param);
            const gchar *val = g_ptr_array_index (priv->dependencies, i);
            param = g_strdup_printf ("%s%s ", tmp, val);
            g_free (tmp);
        }
    }

    gchar *command;
    file = g_strdup_printf ("%s/%s", OUT_PATH, priv->file_name);
    command = g_strdup_printf ("pkexec %s/%s/%s %s %s", LIBDIR, GETTEXT_PACKAGE, VIEWER_SCRIPT, file, param);
    g_spawn_command_line_sync (command, NULL, NULL, NULL, &error);

    if (error != NULL)
    {
        priv->error = g_strdup (error->message);
        g_object_set (G_OBJECT (user_data), "status", STATUS_ERROR, NULL);
    }
    else
    {
        g_object_set (G_OBJECT (user_data), "status", STATUS_INSTALLED, NULL);
    }

    g_free (command);
    unlink (file);
    g_free (file);

    return G_SOURCE_REMOVE;
}

gchar*
viewer_installer_window_view_model_get_file_name (ViewerInstallerWindowViewModel *view_model)
{
    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (view_model), NULL);

    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (view_model);
    return priv->file_name;
}

gchar*
viewer_installer_window_view_model_get_package (ViewerInstallerWindowViewModel *view_model)
{
    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (view_model), NULL);

    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (view_model);
    return priv->package;
}

gchar*
viewer_installer_window_view_model_get_error (ViewerInstallerWindowViewModel *view_model)
{
    g_return_val_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (view_model), NULL);

    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (view_model);
    return priv->error;
}

void
viewer_installer_window_view_model_download(ViewerInstallerWindowViewModel *view_model)
{
    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (view_model));

    gboolean is_connected;
    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (view_model);

    GNetworkMonitor *monitor = g_network_monitor_get_default();
    is_connected = g_network_monitor_get_network_available (monitor);

    if (!is_connected)
    {
        priv->error = g_strdup (_("Network is not active"));
        g_object_set (G_OBJECT (view_model), "status", STATUS_ERROR, NULL);
        return;
    }

    g_object_set (G_OBJECT (view_model), "status", STATUS_DOWNLOADING, NULL);

    if (priv->status_thread)
        g_thread_unref (priv->status_thread);

    priv->status_thread = g_thread_new ("hangul-download", (GThreadFunc)hangul_download_func, view_model);
}

void
viewer_installer_window_view_model_install(ViewerInstallerWindowViewModel *view_model)
{
    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (view_model));

    ViewerInstallerWindowViewModelPrivate *priv;
    priv = viewer_installer_window_view_model_get_instance_private (view_model);

    g_object_set (G_OBJECT (view_model), "status", STATUS_INSTALLING, NULL);

    if (priv->progress_thread)
        g_thread_unref (priv->progress_thread);

    priv->progress_thread = g_thread_new ("hangul-install", (GThreadFunc)hangul_install_func, view_model);
}

static void
viewer_installer_window_view_model_infos_init (ViewerInstallerWindowViewModel *view_model)
{
    GError *error;
    gchar *filename;
    JsonNode *json_root;
    JsonObject *json_item;
    JsonNode *json_node;
    g_autoptr(JsonParser) json_parser = NULL;

    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (view_model));

    ViewerInstallerWindowViewModelPrivate *priv = viewer_installer_window_view_model_get_instance_private (view_model);

    error = NULL;
    json_parser = json_parser_new ();
    filename = g_strdup_printf ("%s/%s", LIBDIR, JSON_FILE);

    if (!json_parser_load_from_file (json_parser, filename, &error))
        goto error; 

    json_root = json_parser_get_root (json_parser);
    if (json_root == NULL)
        goto error;

    if (json_node_get_node_type (json_root) != JSON_NODE_OBJECT)
        goto error;

    json_item = json_node_get_object (json_root);
    if (json_item == NULL)
        goto error;

    if (json_object_has_member (json_item, "package"))
    {
        json_item = json_object_get_object_member (json_item, "package");
        json_node = json_object_get_member (json_item, "name");
        if (json_node != NULL)
            priv->package = g_strdup (json_node_get_string (json_node));

        json_node = json_object_get_member (json_item, "file-name");
        if (json_node != NULL)
            priv->file_name = g_strdup (json_node_get_string (json_node));

        json_node = json_object_get_member (json_item, "MD5");
        if (json_node != NULL)
            priv->md5 = g_strdup (json_node_get_string (json_node));

        json_node = json_object_get_member (json_item, "SHA256");
        if (json_node != NULL)
            priv->sha256 = g_strdup (json_node_get_string (json_node));

        if (json_object_has_member (json_item, "dependency"))
        {
            int i;
            gchar *dependency;
            JsonArray *array = json_object_get_array_member (json_item, "dependency");
            for (i = 0; i < json_array_get_length (array); i++)
            {
                json_node = json_array_get_element (array, i);
                if (json_node == NULL)
                    continue;
                dependency = g_strdup (json_node_get_string (json_node));
                g_ptr_array_add (priv->dependencies, dependency);
            }
        }
        return;
    }

error :
    priv->error = g_strdup ("error, json");
    return;
}

static void
viewer_installer_window_view_model_set_property (GObject *object,
                                                guint property_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (object));

    ViewerInstallerWindowViewModel *view_model;
    ViewerInstallerWindowViewModelPrivate *priv;

    view_model = VIEWER_INSTALLER_WINDOW_VIEW_MODEL (object);
    priv = viewer_installer_window_view_model_get_instance_private (view_model);

    if (property_id == PROP_STATUS)
    {
        g_mutex_lock (&status_mutex);
        priv->status = g_value_get_uint (value);
        g_mutex_unlock (&status_mutex);
    }
    else if (property_id == PROP_PROGRESS)
    {
        g_mutex_lock (&progress_mutex);
        priv->progress = g_value_get_uint (value);
        g_mutex_unlock (&progress_mutex);
    }
}

static void
viewer_installer_window_view_model_get_property (GObject *object,
                                                guint property_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (object));

    ViewerInstallerWindowViewModel *view_model;
    ViewerInstallerWindowViewModelPrivate *priv;

    view_model = VIEWER_INSTALLER_WINDOW_VIEW_MODEL (object);
    priv = viewer_installer_window_view_model_get_instance_private (view_model);

    if (property_id == PROP_STATUS)
    {
        g_mutex_lock (&status_mutex);
        g_value_set_uint (value, priv->status);
        g_mutex_unlock (&status_mutex);
    }
    else if (property_id == PROP_PROGRESS)
    {
        g_mutex_lock (&progress_mutex);
        g_value_set_uint (value, priv->progress);
        g_mutex_unlock (&progress_mutex);
    }
}

static void
viewer_installer_window_view_model_network_changed (GNetworkMonitor *monitor,
                                                    gboolean network_available,
                                                    gpointer user_data)
{
    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (user_data));

    ViewerInstallerWindowViewModel *view_model;
    ViewerInstallerWindowViewModelPrivate *priv;

    view_model = VIEWER_INSTALLER_WINDOW_VIEW_MODEL (user_data);
    priv = viewer_installer_window_view_model_get_instance_private (view_model);

    if (network_available)
        return;

    if (STATUS_INSTALLING <= priv->status)
        return;

    gchar *out_file;
    out_file = g_strdup_printf ("%s/%s", OUT_PATH, priv->file_name);
    unlink (out_file);

    priv->error = g_strdup (_("Network is not active"));
    g_object_set (G_OBJECT (view_model), "status", STATUS_ERROR, NULL);
}

static void
viewer_installer_window_view_model_dispose (GObject *object)
{
    g_return_if_fail (VIEWER_INSTALLER_WINDOW_VIEW_MODEL (object));

    ViewerInstallerWindowViewModel *view_model;
    ViewerInstallerWindowViewModelPrivate *priv;

    view_model = VIEWER_INSTALLER_WINDOW_VIEW_MODEL (object);
    priv = viewer_installer_window_view_model_get_instance_private (view_model);

    if (priv->status_thread)
    {
        g_thread_join (priv->status_thread);
        priv->status_thread = NULL;
    }

    if (priv->status_thread)
    {
        g_thread_join (priv->status_thread);
        priv->status_thread = NULL;
    }

    if (priv->error)
    {
        g_free (priv->error);
        priv->error = NULL;
    }

     if (priv->package)
    {
        g_free (priv->package);
        priv->package = NULL;
    }

    if (priv->file_name)
    {
        g_free (priv->file_name);
        priv->file_name = NULL;
    }

    if (priv->dependencies)
    {
        g_ptr_array_unref (priv->dependencies);
        priv->dependencies = NULL;
    }

    if (priv->sha256)
    {
        g_free (priv->sha256);
        priv->sha256 = NULL;
    }

    if (priv->md5)
    {
        g_free (priv->md5);
        priv->md5 = NULL;
    }

    G_OBJECT_CLASS (viewer_installer_window_view_model_parent_class)->dispose (object);
}

static void
viewer_installer_window_view_model_finalize (GObject *object)
{
    G_OBJECT_CLASS (viewer_installer_window_view_model_parent_class)->finalize (object);
}

static void
viewer_installer_window_view_model_class_init (ViewerInstallerWindowViewModelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = viewer_installer_window_view_model_dispose;
    object_class->finalize = viewer_installer_window_view_model_finalize;
    object_class->set_property = viewer_installer_window_view_model_set_property;
    object_class->get_property = viewer_installer_window_view_model_get_property;

    pspec= g_param_spec_uint ("status", "Status", "Install Status", STATUS_NORMAL, N_STATUS, STATUS_NORMAL, G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_STATUS, pspec);

    pspec= g_param_spec_uint ("progress", "Progress", "Download progress", 0, 100, 0, G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_PROGRESS, pspec);
}

static void
viewer_installer_window_view_model_init (ViewerInstallerWindowViewModel *self)
{
    ViewerInstallerWindowViewModelPrivate *priv = viewer_installer_window_view_model_get_instance_private (self);
    priv->status = STATUS_NORMAL;
    priv->is_valid = FALSE;
    priv->install_id = 0;
    priv->progress = 0;
    priv->package = NULL;
    priv->file_name = NULL;
    priv->status_thread = NULL;
    priv->progress_thread = NULL;
    priv->dependencies = g_ptr_array_new ();

    GNetworkMonitor *monitor = g_network_monitor_get_default();
    g_signal_connect (monitor, "network-changed", G_CALLBACK (viewer_installer_window_view_model_network_changed), self);

    g_object_notify_by_pspec (G_OBJECT(self), pspec);

    viewer_installer_window_view_model_infos_init (self);
}

ViewerInstallerWindowViewModel *
viewer_installer_window_view_model_new (void)
{
    ViewerInstallerWindowViewModel *view_model;
    view_model = g_object_new (VIEWER_INSTALLER_TYPE_WINDOW_VIEW_MODEL, NULL);
    return view_model;
}
