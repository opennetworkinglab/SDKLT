/*! \file bcmlt_ltable_entry.c
 *
 * Handles entry operations.
 * This file handles all operations that are related to entries including
 * entry allocation and free, entry commit and get entry info.
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
#include <sal/sal_types.h>
#include <shr/shr_error.h>
#include <shr/shr_debug.h>
#include <shr/shr_lmem_mgr.h>
#include <bcmlt/bcmlt.h>
#include <bcmtrm/trm_api.h>
#include "bcmlt_internal.h"

/*******************************************************************************
 * Local definitions
 */
#define BSL_LOG_MODULE BSL_LS_BCMLT_ENTRY

/*******************************************************************************
 * Public functions
 */
int bcmlt_entry_clean_data_fields(bcmtrm_entry_t *entry)
{
#define MAX_KEY_FIELDS  128
    uint32_t key_ids[MAX_KEY_FIELDS];
    uint32_t key_count = MAX_KEY_FIELDS;
    uint32_t j;
    shr_fmm_t *field = entry->l_field;
    shr_fmm_t *prev_field = NULL;

    if (!entry->l_field) {
        return SHR_E_NONE;
    }

    sal_memset(key_ids, 0, sizeof(key_ids));
    bcmlt_db_table_keys_get(entry->info.unit, entry->db_hdl,
                            MAX_KEY_FIELDS, key_ids, &key_count);
    if (key_count > MAX_KEY_FIELDS) {
        LOG_ERROR(BSL_LOG_MODULE,
                  (BSL_META_U(entry->info.unit,
                              "Entry has too many fields=%u max allowed %d\n"),
                   key_count, MAX_KEY_FIELDS));
        return SHR_E_UNAVAIL;
    }
    while (field) {
        for (j = 0; j < key_count; j++) {
            if (field->id == key_ids[j]) {
                break;
            }
        }
        if (j < key_count) {   /* Field is a key field */
            prev_field = field;
            field = field->next;
            continue;
        }

        /* Field is not key, delete it */
        if (entry->fld_arr) {
            entry->fld_arr[field->id] = NULL;
        }
        if (prev_field) {
            prev_field->next = field->next;
            shr_fmm_free(field);
            field = prev_field->next;
        } else {    /* First element in the list */
            entry->l_field = field->next;
            shr_fmm_free(field);
            field = entry->l_field;
        }
    }
    return SHR_E_NONE;
}

int bcmlt_entry_allocate(int unit,
                         const char *table_name,
                         bcmlt_entry_handle_t *entry_hdl)
{
    bcmtrm_entry_t *entry;
    bcmlt_table_attrib_t *table_attr;
    void *hdl;
    shr_lmm_hdl_t fld_array_hdl;

    SHR_FUNC_ENTER(unit);
    if (!bcmlt_is_initialized()) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(unit,
                                "BCMLT was not initialized yet\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_INIT);
    }
    UNIT_VALIDATION(unit);
    if (!table_name || !entry_hdl) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(unit,
                                "Table name and entry handle shouldn't"\
                                " be NULL\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }
    /* Find the table ID associated with the name */
    SHR_IF_ERR_EXIT(bcmlt_db_table_info_get(unit,
                                            table_name,
                                            &table_attr,
                                            &fld_array_hdl,
                                            &hdl));

    entry = bcmtrm_entry_alloc(unit,
                               table_attr->table_id,
                               table_attr->interactive,
                               table_attr->pt,
                               fld_array_hdl,
                               table_attr->name);
    SHR_NULL_CHECK(entry, SHR_E_MEMORY);
    entry->db_hdl = hdl;
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
    entry->dummy = malloc(1);
#endif
    /* Clean the fields array */
    if (fld_array_hdl) {
        sal_memset(entry->fld_arr, 0, sizeof(void *) * (table_attr->max_fid+1));
        entry->max_fid = table_attr->max_fid;
    } else {
        entry->max_fid = 0;
    }

    SHR_IF_ERR_EXIT(bcmlt_hdl_alloc(entry, entry_hdl));

    entry->info.entry_hdl = *entry_hdl;
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_clear(bcmlt_entry_handle_t entry_hdl)
{
    shr_fmm_t *field;
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);

    /* Sanity check. Entry state must be E_ACTIVE */
    ENTRY_VALIDATE(entry);
    if (entry->p_trans) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(entry->info.unit,
                                "Entry is part of a transaction\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_BUSY);
    }

    /* Free all the fields */
    while (entry->l_field) {
        field = entry->l_field;
        entry->l_field = field->next;
        if (entry->fld_arr) {
            entry->fld_arr[field->id] = NULL;
        }
        shr_fmm_free(field);
    }

    entry->attrib = 0;   /* Clear attributes */
    entry->usr_data = NULL;  /* Clear user data */
    entry->asynch = false;
    entry->delete = false;
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_free(bcmlt_entry_handle_t entry_hdl)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    /*
     * Sanity check, don't allow deletion of entry in the following
     * cases:
     * 1. entry in idle state
     * 2. entry is part of a transaciton
     */
    if (!entry || (entry->state > E_COMMITTED) ||
        entry->state < E_IDLE) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Invalid entry handle\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }
    if (entry->state == E_IDLE) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META("Stale entry handle\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_NOT_FOUND);
    }
    if (entry->p_trans) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(entry->info.unit,
                                "Entry is part of a transaction\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_BUSY);
    }

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
    free(entry->dummy);
#endif

    SHR_IF_ERR_EXIT(bcmlt_hdl_free(entry_hdl));

    SHR_IF_ERR_EXIT(bcmtrm_entry_free(entry));
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_commit(bcmlt_entry_handle_t entry_hdl,
                       bcmlt_opcode_t opcode,
                       bcmlt_priority_level_t priority)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    /* Input parameters checking */
    ENTRY_VALIDATE(entry);

    if (entry->pt) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(entry->info.unit,
                                "Use the PT functions for PT entry\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }

    LT_OPCODE_VALIDATE(opcode, entry->info.unit);
    PRIORITY_VALIDATE(priority, entry->info.unit);

    entry->opcode.lt_opcode = opcode;
    entry->priority = BCMLT_PRIORITY_NORMAL; /* No priorities for sync oper */

    /* Clear get from hw attribute if opcode is not lookup */
    if (opcode == BCMLT_OPCODE_LOOKUP) {
        /* Clean out all fields other than key fields. */
        SHR_IF_ERR_EXIT(bcmlt_entry_clean_data_fields(entry));
    } else if (opcode == BCMLT_OPCODE_TRAVERSE) {
        /* Clean out all fields other than key fields. */
        if (entry->l_field) {
            SHR_IF_ERR_EXIT(bcmlt_entry_clean_data_fields(entry));
            if (!entry->l_field) {   /* If no key fields don't search again */
                entry->info.status = SHR_E_NOT_FOUND;
                SHR_RETURN_VAL_EXIT(SHR_E_NONE);
            }
        }
    } else {
        entry->attrib &= ~TRM_E_ATTR_HW_GET;
    }

    entry->usr_data = NULL;
    entry->p_trans = NULL;    /* Not part of transaction */
    entry->info.notif_opt = BCMLT_NOTIF_OPTION_NO_NOTIF;

    bcmlt_replay_entry_record(entry);  /* Record the entry op */
    SHR_IF_ERR_EXIT(bcmtrm_entry_req(entry));
    LOG_VERBOSE(BSL_LOG_MODULE,
                (BSL_META_U(entry->info.unit, "exit status=%d\n"),
                 entry->info.status));
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_commit_async(bcmlt_entry_handle_t entry_hdl,
                             bcmlt_opcode_t opcode,
                             void *user_data,
                             bcmlt_notif_option_t notif_opt,
                             bcmlt_entry_cb notif_fn,
                             bcmlt_priority_level_t priority)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    /* Input parameters checking */
    ENTRY_VALIDATE(entry);
    if (entry->pt) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(entry->info.unit,
                                "Use the PT functions for PT entry\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }
    LT_OPCODE_VALIDATE(opcode, entry->info.unit);
    PRIORITY_VALIDATE(priority, entry->info.unit);
    NOTIFICATION_VALIDATE(notif_opt, notif_fn, entry->info.unit);

    entry->opcode.lt_opcode = opcode;
    entry->priority = priority;

    /* Clear get from hw attribute if opcode is not lookup */
    if (opcode == BCMLT_OPCODE_LOOKUP) {
        /* Clean out all fields other than key fields. */
        SHR_IF_ERR_EXIT(bcmlt_entry_clean_data_fields(entry));
    } else if (opcode == BCMLT_OPCODE_TRAVERSE) {
        /* Clean out all fields other than key fields. */
        if (entry->l_field) {
            SHR_IF_ERR_EXIT(bcmlt_entry_clean_data_fields(entry));
            if (!entry->l_field) {   /* If no key fields don't search again */
                SHR_RETURN_VAL_EXIT(SHR_E_NOT_FOUND);
            }
        }
    } else {
        entry->attrib &= ~TRM_E_ATTR_HW_GET;
    }
    entry->p_trans = NULL;    /* Not part of transaction */

    entry->cb.entry_cb = notif_fn;
    entry->info.notif_opt = notif_opt;
    entry->usr_data = user_data;
    bcmlt_replay_entry_record(entry);  /* Record the entry op */
    SHR_IF_ERR_EXIT(bcmtrm_asynch_entry_req(entry));
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_pt_entry_commit(bcmlt_entry_handle_t entry_hdl,
                          bcmlt_pt_opcode_t opcode,
                          bcmlt_priority_level_t priority)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    /* Input parameters checking */
    ENTRY_VALIDATE(entry);
    if (!entry->pt) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(entry->info.unit,
                                "Use the PT functions only for PT entry\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }

    PT_OPCODE_VALIDATE(opcode, entry->info.unit);
    PRIORITY_VALIDATE(priority, entry->info.unit);

    entry->opcode.pt_opcode = opcode;
    entry->priority = BCMLT_PRIORITY_NORMAL; /* No priorities for sync oper */

    /* Clear get from hw attribute if opcode is not get/lookup */
    if ((opcode != BCMLT_PT_OPCODE_GET) &&
        (opcode != BCMLT_PT_OPCODE_LOOKUP)) {
        entry->attrib &= ~TRM_E_ATTR_HW_GET;
    } else {
        /* Make sure not to pass this flag for PT operations */
        entry->attrib &= ~TRM_E_ATTR_EXC_DEF;
    }

    entry->state = E_COMMITTING;
    entry->p_trans = NULL;    /* Not part of transaction */
    entry->usr_data = NULL;
    bcmlt_replay_entry_record(entry);  /* Record the entry op */
    SHR_IF_ERR_EXIT(bcmtrm_entry_req(entry));
    LOG_VERBOSE(BSL_LOG_MODULE,
                (BSL_META_U(entry->info.unit, "exit status=%d\n"),
                 entry->info.status));
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_pt_entry_commit_async(bcmlt_entry_handle_t entry_hdl,
                                bcmlt_pt_opcode_t opcode,
                                void *user_data,
                                bcmlt_notif_option_t notif_opt,
                                bcmlt_entry_pt_cb notif_fn,
                                bcmlt_priority_level_t priority)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    ENTRY_VALIDATE(entry);
    if (!entry->pt) {
        LOG_VERBOSE(BSL_LOG_MODULE,
                    (BSL_META_U(entry->info.unit,
                                "Use the PT functions only for PT entry\n")));
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }

    PT_OPCODE_VALIDATE(opcode, entry->info.unit);
    PRIORITY_VALIDATE(priority, entry->info.unit);
    PT_NOTIFICATION_VALIDATE(notif_opt, notif_fn, entry->info.unit);

    entry->opcode.pt_opcode = opcode;
    entry->priority = priority;

    /* Clear get from hw attribute if opcode is not get/lookup */
    if ((opcode != BCMLT_PT_OPCODE_GET) &&
        (opcode != BCMLT_PT_OPCODE_LOOKUP)) {
        entry->attrib &= ~TRM_E_ATTR_HW_GET;
    } else {
        /* Make sure not to pass this flag for PT operations */
        entry->attrib &= ~TRM_E_ATTR_EXC_DEF;
    }

    entry->state = E_COMMITTING;
    entry->p_trans = NULL;    /* Not part of transaction */
    entry->cb.entry_pt_cb = notif_fn;
    entry->info.notif_opt = notif_opt;
    entry->usr_data = user_data;
    bcmlt_replay_entry_record(entry);  /* Record the entry op */
    SHR_IF_ERR_EXIT(bcmtrm_asynch_entry_req(entry));
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_info_get(bcmlt_entry_handle_t entry_hdl,
                         bcmlt_entry_info_t  *entry_info)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    if (!entry || !entry_info || (entry->state == E_IDLE)) {
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }
    *entry_info = entry->info;
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_attrib_set(bcmlt_entry_handle_t entry_hdl,
                           uint32_t attrib)
{
    uint32_t check_attrib = 1;
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    ENTRY_VALIDATE(entry);

    /*
     * Go through all the attributes and translate them into the TRM entry
     * attribute
     */
    while (attrib) {
        if (attrib & check_attrib) {
            attrib &= ~check_attrib;
            switch (check_attrib) {
            case BCMLT_ENT_ATTR_GET_FROM_HW:
                entry->attrib |= TRM_E_ATTR_HW_GET;
                break;
            case BCMLT_ENT_ATTR_FILTER_DEFAULTS:
                entry->attrib |= TRM_E_ATTR_EXC_DEF;
                break;
            }
        }
        check_attrib <<= 1;     /* Shift by one to next attribute */
    }
exit:
    SHR_FUNC_EXIT();
}

int bcmlt_entry_attrib_get(bcmlt_entry_handle_t entry_hdl,
                           uint32_t *attrib)
{
    uint32_t check_attrib = 1;
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);
    uint32_t trm_attrib;

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    ENTRY_VALIDATE(entry);

    /*
     * Go through all the attributes and translate them from TRM attributes
     * into the BCMLT entry attributes
     */
    trm_attrib = entry->attrib;
    *attrib = 0;
    while (trm_attrib) {
        if (trm_attrib & check_attrib) {
            trm_attrib &= ~check_attrib;
            switch (check_attrib) {
            case TRM_E_ATTR_HW_GET:
                *attrib |= BCMLT_ENT_ATTR_GET_FROM_HW;
                break;
            case TRM_E_ATTR_EXC_DEF:
                *attrib |= BCMLT_ENT_ATTR_FILTER_DEFAULTS;
                break;
            }
        }
        check_attrib <<= 1;     /* Shift by one to next attribute */
    }
exit:
    SHR_FUNC_EXIT();
}


int bcmlt_entry_oper_get(bcmlt_entry_handle_t entry_hdl,
                         bcmlt_unified_opcode_t *uni_opcode)
{
    bcmtrm_entry_t *entry = bcmlt_hdl_data_get(entry_hdl);

    SHR_FUNC_ENTER(entry ? entry->info.unit : BSL_UNIT_UNKNOWN);
    ENTRY_VALIDATE(entry);
    if (!uni_opcode) {
        SHR_RETURN_VAL_EXIT(SHR_E_PARAM);
    }

    uni_opcode->pt = entry->pt;
    if (entry->pt) {
        uni_opcode->opcode.pt = entry->opcode.pt_opcode;
    } else {
        uni_opcode->opcode.lt = entry->opcode.lt_opcode;
    }

exit:
    SHR_FUNC_EXIT();
}
