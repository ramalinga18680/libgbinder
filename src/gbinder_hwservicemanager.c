/*
 * Copyright (C) 2018-2019 Jolla Ltd.
 * Copyright (C) 2018-2019 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gbinder_servicemanager_p.h"
#include "gbinder_rpc_protocol.h"
#include "gbinder_log.h"

#include <gbinder_client.h>
#include <gbinder_local_object.h>
#include <gbinder_local_request.h>
#include <gbinder_remote_reply.h>
#include <gbinder_remote_request.h>
#include <gbinder_reader.h>

#include <errno.h>
#include <pthread.h>

typedef struct gbinder_hwservicemanager_watch {
    char* name;
    GBinderLocalObject* callback;
} GBinderHwServiceManagerWatch;

typedef GBinderServiceManagerClass GBinderHwServiceManagerClass;
typedef struct gbinder_hwservicemanager {
    GBinderServiceManager manager;
    GHashTable* watch_table;
} GBinderHwServiceManager;

G_DEFINE_TYPE(GBinderHwServiceManager,
    gbinder_hwservicemanager,
    GBINDER_TYPE_SERVICEMANAGER)

#define PARENT_CLASS gbinder_hwservicemanager_parent_class
#define GBINDER_TYPE_HWSERVICEMANAGER (gbinder_hwservicemanager_get_type())
#define GBINDER_HWSERVICEMANAGER(obj) \
    G_TYPE_CHECK_INSTANCE_CAST((obj), GBINDER_TYPE_HWSERVICEMANAGER, \
    GBinderHwServiceManager)

enum gbinder_hwservicemanager_calls {
    GET_TRANSACTION = GBINDER_FIRST_CALL_TRANSACTION,
    ADD_TRANSACTION,
    GET_TRANSPORT_TRANSACTION,
    LIST_TRANSACTION,
    LIST_BY_INTERFACE_TRANSACTION,
    REGISTER_FOR_NOTIFICATIONS_TRANSACTION,
    DEBUG_DUMP_TRANSACTION,
    REGISTER_PASSTHROUGH_CLIENT_TRANSACTION
};

enum gbinder_hwservicemanager_notifications {
    ON_REGISTRATION_TRANSACTION = GBINDER_FIRST_CALL_TRANSACTION
};

#define HWSERVICEMANAGER_IFACE  "android.hidl.manager@1.0::IServiceManager"
#define HWSERVICEMANAGER_NOTIFICATION_IFACE \
    "android.hidl.manager@1.0::IServiceNotification"

static
void
gbinder_hwservicemanager_handle_registration(
    GBinderHwServiceManager* self,
    GBinderReader* reader)
{
    char* fqname = gbinder_reader_read_hidl_string(reader);
    char* name = gbinder_reader_read_hidl_string(reader);
    gboolean preexisting;

    /* (string fqName, string name, bool preexisting) */
    if (fqname && name && gbinder_reader_read_bool(reader, &preexisting) &&
        gbinder_reader_at_end(reader)) {
        char* full_name = g_strconcat(fqname, "/", name, NULL);

        GDEBUG("%s %s", full_name, preexisting ? "true" : "false");
        gbinder_servicemanager_service_registered(&self->manager, full_name);
        g_free(full_name);
    } else {
        GWARN("Failed to parse IServiceNotification::onRegistration payload");
    }
    g_free(fqname);
    g_free(name);
}

static
GBinderLocalReply*
gbinder_hwservicemanager_notification(
    GBinderLocalObject* obj,
    GBinderRemoteRequest* req,
    guint code,
    guint flags,
    int* status,
    void* user_data)
{
    GBinderHwServiceManager* self = GBINDER_HWSERVICEMANAGER(user_data);
    const char* iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, HWSERVICEMANAGER_NOTIFICATION_IFACE)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case ON_REGISTRATION_TRANSACTION:
            GDEBUG(HWSERVICEMANAGER_NOTIFICATION_IFACE " %u onRegistration",
                code);
            gbinder_hwservicemanager_handle_registration(self, &reader);
            *status = GBINDER_STATUS_OK;
            break;
        default:
            GDEBUG(HWSERVICEMANAGER_NOTIFICATION_IFACE " %u", code);
            *status = GBINDER_STATUS_FAILED;
            break;
        }
    } else {
        GDEBUG("%s %u", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return NULL;
}

GBinderServiceManager*
gbinder_hwservicemanager_new(
    const char* dev)
{
    return gbinder_servicemanager_new_with_type
        (gbinder_hwservicemanager_get_type(), dev);
}

static
char**
gbinder_hwservicemanager_list(
    GBinderServiceManager* self)
{
    GBinderLocalRequest* req = gbinder_client_new_request(self->client);
    GBinderRemoteReply* reply = gbinder_client_transact_sync_reply
        (self->client, LIST_TRANSACTION, req, NULL);

    gbinder_local_request_unref(req);
    if (reply) {
        GBinderReader reader;
        char** result = NULL;
        int status = -1;

        gbinder_remote_reply_init_reader(reply, &reader);

        /* Read status */
        GVERIFY(gbinder_reader_read_int32(&reader, &status));
        GASSERT(status == GBINDER_STATUS_OK);

        /* Followed by hidl_vec<string> */
        result = gbinder_reader_read_hidl_string_vec(&reader);
        gbinder_remote_reply_unref(reply);
        return result;
    }
    return NULL;
}

static
GBinderRemoteObject*
gbinder_hwservicemanager_get_service(
    GBinderServiceManager* self,
    const char* fqinstance,
    int* status)
{
    /* e.g. "android.hardware.radio@1.1::IRadio/slot1" */
    const char* sep = strchr(fqinstance, '/');
    GBinderRemoteObject* obj = NULL;

    if (sep) {
        GBinderRemoteReply* reply;
        GBinderLocalRequest* req = gbinder_client_new_request(self->client);
        char* fqname = g_strndup(fqinstance, sep - fqinstance);
        const char* name = sep + 1;

        gbinder_local_request_append_hidl_string(req, fqname);
        gbinder_local_request_append_hidl_string(req, name);

        reply = gbinder_client_transact_sync_reply(self->client,
            GET_TRANSACTION, req, status);

        if (reply) {
            GBinderReader reader;
            int status = -1;

            gbinder_remote_reply_init_reader(reply, &reader);

            /* Read status */
            GVERIFY(gbinder_reader_read_int32(&reader, &status));
            GASSERT(status == GBINDER_STATUS_OK);

            /* Read the object */
            obj = gbinder_reader_read_object(&reader);
            gbinder_remote_reply_unref(reply);
        }

        gbinder_local_request_unref(req);
        g_free(fqname);
    } else {
        GERR("Invalid instance \"%s\"", fqinstance);
        if (status) *status = (-EINVAL);
    }
    return obj;
}

static
int
gbinder_hwservicemanager_add_service(
    GBinderServiceManager* self,
    const char* name,
    GBinderLocalObject* obj)
{
    int status;
    GBinderRemoteReply* reply;
    GBinderLocalRequest* req = gbinder_client_new_request(self->client);

    /* add(string name, interface service) generates (bool success); */
    gbinder_local_request_append_hidl_string(req, name);
    gbinder_local_request_append_local_object(req, obj);

    reply = gbinder_client_transact_sync_reply(self->client,
        ADD_TRANSACTION, req, &status);

    gbinder_remote_reply_unref(reply);
    gbinder_local_request_unref(req);
    return status;
}

static
void
gbinder_hwservicemanager_watch_free(
    gpointer data)
{
    GBinderHwServiceManagerWatch* watch = data;

    g_free(watch->name);
    gbinder_local_object_drop(watch->callback);
    g_free(watch);
}

static
GBINDER_SERVICEMANAGER_NAME_CHECK
gbinder_hwservicemanager_check_name(
    GBinderServiceManager* self,
    const char* name)
{
    if (name) {
        const gsize len = strlen(name);
        static const char allowed_chars[] = "./0123456789:@"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        if (len && strspn(name, allowed_chars) == len) {
            return strchr(name, '/') ?
                GBINDER_SERVICEMANAGER_NAME_NORMALIZE :
                GBINDER_SERVICEMANAGER_NAME_OK;
        }
    }
    return GBINDER_SERVICEMANAGER_NAME_INVALID;
}

static
char*
gbinder_hwservicemanager_normalize_name(
    GBinderServiceManager* self,
    const char* name)
{
    /* Slash must be there, see gbinder_hwservicemanager_check_name() above */
    return g_strndup(name, strchr(name, '/') - name);
}

static
gboolean
gbinder_hwservicemanager_watch(
    GBinderServiceManager* manager,
    const char* name)
{
    GBinderHwServiceManager* self = GBINDER_HWSERVICEMANAGER(manager);
    GBinderLocalRequest* req = gbinder_client_new_request(manager->client);
    GBinderRemoteReply* reply;
    GBinderHwServiceManagerWatch* watch =
        g_new0(GBinderHwServiceManagerWatch, 1);
    gboolean success = FALSE;
    int status;

    watch->name = g_strdup(name);
    watch->callback = gbinder_servicemanager_new_local_object(manager,
        HWSERVICEMANAGER_NOTIFICATION_IFACE,
        gbinder_hwservicemanager_notification, self);
    g_hash_table_replace(self->watch_table, watch->name, watch);

    /* registerForNotifications(string fqName, string name,
     * IServiceNotification callback) generates (bool success); */
    gbinder_local_request_append_hidl_string(req, name);
    gbinder_local_request_append_hidl_string(req, "");
    gbinder_local_request_append_local_object(req, watch->callback);
    reply = gbinder_client_transact_sync_reply(manager->client,
        REGISTER_FOR_NOTIFICATIONS_TRANSACTION, req, &status);

    if (status == GBINDER_STATUS_OK && reply) {
        GBinderReader reader;

        gbinder_remote_reply_init_reader(reply, &reader);
        if (gbinder_reader_read_int32(&reader, &status) &&
            status == GBINDER_STATUS_OK) {
            gbinder_reader_read_bool(&reader, &success);
        }
    }
    gbinder_remote_reply_unref(reply);
    gbinder_local_request_unref(req);

    if (!success) {
        /* unwatch() won't be called if we return FALSE */
        g_hash_table_remove(self->watch_table, watch->name);
    }
    return success;
}

static
void
gbinder_hwservicemanager_unwatch(
    GBinderServiceManager* manager,
    const char* name)
{
    g_hash_table_remove(GBINDER_HWSERVICEMANAGER(manager)->watch_table, name);
}

static
void
gbinder_hwservicemanager_init(
    GBinderHwServiceManager* self)
{
    self->watch_table = g_hash_table_new_full(g_str_hash, g_str_equal,
        NULL, gbinder_hwservicemanager_watch_free);
}

static
void
gbinder_hwservicemanager_finalize(
    GObject* object)
{
    GBinderHwServiceManager* self = GBINDER_HWSERVICEMANAGER(object);

    g_hash_table_destroy(self->watch_table);
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
gbinder_hwservicemanager_class_init(
    GBinderHwServiceManagerClass* klass)
{
    klass->iface = HWSERVICEMANAGER_IFACE;
    klass->default_device = GBINDER_DEFAULT_HWBINDER;
    klass->rpc_protocol = &gbinder_rpc_protocol_hwbinder;

    klass->list = gbinder_hwservicemanager_list;
    klass->get_service = gbinder_hwservicemanager_get_service;
    klass->add_service = gbinder_hwservicemanager_add_service;
    klass->check_name = gbinder_hwservicemanager_check_name;
    klass->normalize_name = gbinder_hwservicemanager_normalize_name;
    klass->watch = gbinder_hwservicemanager_watch;
    klass->unwatch = gbinder_hwservicemanager_unwatch;
    G_OBJECT_CLASS(klass)->finalize = gbinder_hwservicemanager_finalize;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
