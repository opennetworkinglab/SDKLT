/*! \file bcmpkt_ppool.c
 *
 * BCMPKT packet pool library is to provide simple packet pool solution
 * for user to get efficiency.
 */
/*
 * Copyright: (c) 2018 Broadcom. All Rights Reserved. "Broadcom" refers to
 * Broadcom Limited and/or its subsidiaries.
 *
 * Broadcom Switch Software License
 *
 * This license governs the use of the accompanying Broadcom software. Your
 * use of the software indicates your acceptance of the terms and conditions
 * of this license. If you do not agree to the terms and conditions of this
 * license, do not use the software.
 * 1. Definitions
 *    "Licensor" means any person or entity that distributes its Work.
 *    "Software" means the original work of authorship made available under
 *    this license.
 *    "Work" means the Software and any additions to or derivative works of
 *    the Software that are made available under this license.
 *    The terms "reproduce," "reproduction," "derivative works," and
 *    "distribution" have the meaning as provided under U.S. copyright law.
 *    Works, including the Software, are "made available" under this license
 *    by including in or with the Work either (a) a copyright notice
 *    referencing the applicability of this license to the Work, or (b) a copy
 *    of this license.
 * 2. Grant of Copyright License
 *    Subject to the terms and conditions of this license, each Licensor
 *    grants to you a perpetual, worldwide, non-exclusive, and royalty-free
 *    copyright license to reproduce, prepare derivative works of, publicly
 *    display, publicly perform, sublicense and distribute its Work and any
 *    resulting derivative works in any form.
 * 3. Grant of Patent License
 *    Subject to the terms and conditions of this license, each Licensor
 *    grants to you a perpetual, worldwide, non-exclusive, and royalty-free
 *    patent license to make, have made, use, offer to sell, sell, import, and
 *    otherwise transfer its Work, in whole or in part. This patent license
 *    applies only to the patent claims licensable by Licensor that would be
 *    infringed by Licensor's Work (or portion thereof) individually and
 *    excluding any combinations with any other materials or technology.
 *    If you institute patent litigation against any Licensor (including a
 *    cross-claim or counterclaim in a lawsuit) to enforce any patents that
 *    you allege are infringed by any Work, then your patent license from such
 *    Licensor to the Work shall terminate as of the date such litigation is
 *    filed.
 * 4. Redistribution
 *    You may reproduce or distribute the Work only if (a) you do so under
 *    this License, (b) you include a complete copy of this License with your
 *    distribution, and (c) you retain without modification any copyright,
 *    patent, trademark, or attribution notices that are present in the Work.
 * 5. Derivative Works
 *    You may specify that additional or different terms apply to the use,
 *    reproduction, and distribution of your derivative works of the Work
 *    ("Your Terms") only if (a) Your Terms provide that the limitations of
 *    Section 7 apply to your derivative works, and (b) you identify the
 *    specific derivative works that are subject to Your Terms.
 *    Notwithstanding Your Terms, this license (including the redistribution
 *    requirements in Section 4) will continue to apply to the Work itself.
 * 6. Trademarks
 *    This license does not grant any rights to use any Licensor's or its
 *    affiliates' names, logos, or trademarks, except as necessary to
 *    reproduce the notices described in this license.
 * 7. Limitations
 *    Platform. The Work and any derivative works thereof may only be used, or
 *    intended for use, with a Broadcom switch integrated circuit.
 *    No Reverse Engineering. You will not use the Work to disassemble,
 *    reverse engineer, decompile, or attempt to ascertain the underlying
 *    technology of a Broadcom switch integrated circuit.
 * 8. Termination
 *    If you violate any term of this license, then your rights under this
 *    license (including the license grants of Sections 2 and 3) will
 *    terminate immediately.
 * 9. Disclaimer of Warranty
 *    THE WORK IS PROVIDED "AS IS" WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WARRANTIES OR CONDITIONS OF
 *    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE OR
 *    NON-INFRINGEMENT. YOU BEAR THE RISK OF UNDERTAKING ANY ACTIVITIES UNDER
 *    THIS LICENSE. SOME STATES' CONSUMER LAWS DO NOT ALLOW EXCLUSION OF AN
 *    IMPLIED WARRANTY, SO THIS DISCLAIMER MAY NOT APPLY TO YOU.
 * 10. Limitation of Liability
 *    EXCEPT AS PROHIBITED BY APPLICABLE LAW, IN NO EVENT AND UNDER NO LEGAL
 *    THEORY, WHETHER IN TORT (INCLUDING NEGLIGENCE), CONTRACT, OR OTHERWISE
 *    SHALL ANY LICENSOR BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY DIRECT,
 *    INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF
 *    OR RELATED TO THIS LICENSE, THE USE OR INABILITY TO USE THE WORK
 *    (INCLUDING BUT NOT LIMITED TO LOSS OF GOODWILL, BUSINESS INTERRUPTION,
 *    LOST PROFITS OR DATA, COMPUTER FAILURE OR MALFUNCTION, OR ANY OTHER
 *    COMMERCIAL DAMAGES OR LOSSES), EVEN IF THE LICENSOR HAS BEEN ADVISED OF
 *    THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sal/sal_alloc.h>
#include <sal/sal_spinlock.h>
#include <shr/shr_error.h>
#include <shr/shr_debug.h>
#include <bsl/bsl.h>
#include <bcmpkt/bcmpkt_buf.h>
#include <bcmpkt/bcmpkt_internal.h>
#include <bcmdrd/bcmdrd_dev.h>
#include "bcmpkt_buf_internal.h"

#define BSL_LOG_MODULE              BSL_LS_BCMPKT_PACKET

/*! Packet buffer pool structure. */
typedef struct bcmpkt_ppool_s {
    /* Active means may allocate buffer from it. */
    bool active;
    /* Buffer number. */
    int pkt_count;
    /* Increase for each free from pkt_count. */
    int free_count;
    /* Point to the first buffer in free list. */
    bcmpkt_packet_t *free_list;
    /* Buffer pool protection lock. */
    sal_spinlock_t lock;
} bcmpkt_ppool_t;

/*! Packet pool. */
static bcmpkt_ppool_t pp;

int
bcmpkt_ppool_create(int pkt_count)
{
    bcmpkt_packet_t *packet;

    SHR_FUNC_ENTER(BSL_UNIT_UNKNOWN);

    if (pkt_count < 0) {
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }

    if (pp.pkt_count) {
        SHR_RETURN_VAL_EXIT(SHR_E_EXISTS);
    }

    while (pkt_count-- > 0) {
        packet = sal_alloc(sizeof(bcmpkt_packet_t), "bcmpktPpoolCreate");
        if (!packet) {
            SHR_RETURN_VAL_EXIT(SHR_E_MEMORY);
        }

        packet->next = pp.free_list;
        pp.free_list = packet;
        pp.free_count++;
        pp.pkt_count++;
    }

    pp.active = true;
    pp.lock = sal_spinlock_create("bcmpktPacketPoolCreate");
    SHR_NULL_CHECK(pp.lock, SHR_E_MEMORY);

exit:
    if (SHR_FUNC_VAL_IS(SHR_E_MEMORY)) {
        while (pp.free_list) {
            packet = pp.free_list;
            pp.free_list = pp.free_list->next;
            sal_free(packet);
        }
        sal_memset(&pp, 0, sizeof(pp));
    }

    SHR_FUNC_EXIT();
}

int
bcmpkt_ppool_destroy(void)
{
    bcmpkt_packet_t *packet;

    if (!pp.active) {
        return SHR_E_NONE;
    }

    sal_spinlock_lock(pp.lock) ;
    pp.active = false;
    sal_spinlock_unlock(pp.lock);

    while (pp.free_list) {
        packet = pp.free_list;
        pp.free_list = pp.free_list->next;
        sal_free(packet);
    }
    sal_spinlock_destroy(pp.lock);
    sal_memset(&pp, 0, sizeof(pp));

    return SHR_E_NONE;
}

bcmpkt_packet_t *
bcmpkt_ppool_alloc(void)
{
    bcmpkt_packet_t *packet;

    if (!pp.lock) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Packet pool was not created\n")));
        return sal_alloc(sizeof(bcmpkt_packet_t), "bcmpktPpoolAlloc");
    }
    if (sal_spinlock_lock(pp.lock) != 0) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Lock failure\n")));
        sal_spinlock_unlock(pp.lock);
        return sal_alloc(sizeof(bcmpkt_packet_t), "bcmpktPpoolAlloc");
    }
    if (!pp.active || pp.free_count < 1) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Packet pool was not available\n")));
        sal_spinlock_unlock(pp.lock);
        return sal_alloc(sizeof(bcmpkt_packet_t), "bcmpktPpoolAlloc");
    }

    packet = pp.free_list;
    pp.free_list = pp.free_list->next;
    pp.free_count--;

    sal_spinlock_unlock(pp.lock);

    return packet;
}

int
bcmpkt_ppool_free(bcmpkt_packet_t *packet)
{
    SHR_FUNC_ENTER(BSL_UNIT_UNKNOWN);

    SHR_NULL_CHECK(packet, SHR_E_PARAM);

    if (!pp.lock) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Packet pool uninitialized\n")));
        sal_free(packet);
        SHR_RETURN_VAL_EXIT(SHR_E_NONE);
    }

    if (sal_spinlock_lock(pp.lock) != 0) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Lock failure\n")));
        sal_free(packet);
        sal_spinlock_unlock(pp.lock);
        SHR_RETURN_VAL_EXIT(SHR_E_NONE);
    }

    if (!pp.active) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Not active\n")));
        sal_free(packet);
        sal_spinlock_unlock(pp.lock);
        SHR_RETURN_VAL_EXIT(SHR_E_NONE);
    }

    /* Save up to pp.pkt_count free buffers. */
    if (pp.free_count < pp.pkt_count) {
        packet->next = pp.free_list;
        pp.free_list = packet;
        pp.free_count++;
    } else {
        sal_free(packet);
    }

    sal_spinlock_unlock(pp.lock);

exit:
    SHR_FUNC_EXIT();
}

int
bcmpkt_ppool_info_dump(void)
{
    cli_out("Packet buffer information:\n");
    cli_out("packet count: %d\n", pp.pkt_count);
    cli_out("Free count: %d\n", pp.free_count);

    return SHR_E_NONE;
}
