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

#ifndef TEST_BINDER_H
#define TEST_BINDER_H

#include "test_common.h"

void
test_binder_br_noop(
    int fd);

void
test_binder_br_increfs(
    int fd,
    void* ptr);

void
test_binder_br_acquire(
    int fd,
    void* ptr);

void
test_binder_br_release(
    int fd,
    void* ptr);

void
test_binder_br_decrefs(
    int fd,
    void* ptr);

void
test_binder_br_transaction_complete(
    int fd);

void
test_binder_br_transaction_complete_later(
    int fd);

void
test_binder_br_dead_binder(
    int fd,
    guint handle);

void
test_binder_br_dead_reply(
    int fd);

void
test_binder_br_failed_reply(
    int fd);

void
test_binder_br_transaction(
    int fd,
    void* target,
    guint32 code,
    const GByteArray* bytes);

void
test_binder_br_reply(
    int fd,
    guint32 handle,
    guint32 code,
    const GByteArray* bytes);

void
test_binder_br_reply_status(
    int fd,
    gint32 status);

void
test_binder_br_reply_later(
    int fd,
    guint32 handle,
    guint32 code,
    const GByteArray* bytes);

void
test_binder_br_reply_status_later(
    int fd,
    gint32 status);

void
test_binder_set_looper_enabled(
    int fd,
    gboolean enabled);

void
test_binder_set_destroy(
    int fd,
    gpointer ptr,
    GDestroyNotify destroy);

#endif /* TEST_BINDER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
