/**
 * @file
 * Implementation of the MAC-PDP module for flat-file storage.
 *
 * @author Copyright (c) 2012, Mark Gondree
 * @author Copyright (c) 2012, Alric Althoff
 * @author Copyright (c) 2008, Zachary N J Peterson
 * @date 2008-2013
 * @copyright BSD 2-Clause License,
 *            See http://opensource.org/licenses/BSD-2-Clause
 **/
/** @addtogroup SEPDP
 * @{ 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef _THREAD_SUPPORT
#include <pthread.h>
#endif
#include <pdp.h>
#include <pdp/sepdp.h>
#include "pdp_misc.h"
#include "pdp_storage.h"
#include "sepdp_storage.h"
#include "sepdp_misc.h"


/**
 * @brief Write tag data out to a file.
 * @param[in]     ctx    pointer to a context structure.
 * @param[in]     t      pointer to tag data.
 * @return 0 on success, non-zero on error
 **/
int sepdp_write_tags_to_file(const pdp_ctx_t *ctx, const pdp_sepdp_tagdata_t* t)
{
    unsigned char *buf = NULL;
    FILE *tagfile = NULL;
    unsigned int len = 0;
    size_t off = 0;
    int status = -1;

    if (!ctx || !t || !t->tokens || !t->tokens_num || !ctx->ofilepath)
        return -1;

    if ((access(ctx->ofilepath, F_OK) == 0) && (ctx->verbose)) {
        PDP_ERR("WARNING: overwriting [%s]", ctx->ofilepath);
    }
    if ((tagfile = fopen(ctx->ofilepath, "w")) == NULL){
        PDP_ERR("%d", ferror(tagfile));
        PDP_ERR("unable to create %s", ctx->ofilepath);
        goto cleanup;
    }

    if ((sepdp_serialize_tags(ctx, t, &buf, &len)) != 0) goto cleanup;
    do {        
        off += fwrite(buf + off, 1, len, tagfile);
        if (ferror(tagfile)) goto cleanup;
        
    } while (off < len);
    status = 0;

cleanup:
    sfree(buf, len);
    if (tagfile) fclose(tagfile);
    return status;
}


/**
 * @brief Implements the interface of pdp_get_tag_noop
 * 
 * To be used with MAC-PDP and a flat-file storage back-end.
 *
 * @todo use mmap to speed up access?
 *
 * @param[in]     ctx    pointer to a context structure.
 * @param[in]     index  the index of tag.
 * @param[out]    buf    pointer to an output buffer.
 * @param[in,out] len    initially the size of buf, output bytes written.
 * @return 0 on success, non-zero on error
 **/
int sepdp_get_tag_file(const pdp_ctx_t* ctx, unsigned int index, 
                       void *buf, unsigned int *len)
{
    pdp_sepdp_tag_t **tag_ptr = (pdp_sepdp_tag_t **) buf;
    pdp_sepdp_tag_t *tag = NULL;
    static FILE* tagfile = NULL;
    unsigned char *data = NULL;
    unsigned int data_len = 0;
    int status = -1;
    
    // If buf is NULL, it is a signal to re-set the stateful variables
    if (!buf && tagfile) {
        fclose(tagfile);
        tagfile = NULL;
        return 0;
    }
    
    if (!ctx || !tag_ptr)
        return -1;

    // if file is not open already, open it
    if (!tagfile && ((tagfile = fopen(ctx->ofilepath, "r")) == NULL)) {
        PDP_ERR("%d", ferror(tagfile));
        PDP_ERR("unable to open %s", ctx->ofilepath);
        return -1;
    }

    // Allocate space for the tag
    if ((tag = malloc(sizeof(pdp_sepdp_tag_t))) == NULL) goto cleanup;
    memset(tag, 0, sizeof(pdp_sepdp_tag_t));
    if (*tag_ptr != NULL) {
        // there is already a tag here, so free it
        sepdp_tag_free(ctx, *tag_ptr);
    }
    *tag_ptr = tag;

    data_len = sepdp_serialized_tag_size(ctx);
    if ((data = malloc(data_len)) == NULL) goto cleanup;
    memset(data, 0, data_len);

    // We seek directly to the index of the i-th tag
    if (fseek(tagfile, index * data_len, SEEK_SET) < 0) goto cleanup;
    fread(data, 1, data_len, tagfile);
    if (ferror(tagfile)) goto cleanup;
    
    // de-serialize the data block
    if (sepdp_deserialize_tag(ctx, tag, data, data_len) != 0)
        goto cleanup;

    status = 0;
    
cleanup:
    sfree(data, data_len);
    if (status && tag_ptr && *tag_ptr) {
        sepdp_tag_free(ctx, *tag_ptr);
        *tag_ptr = NULL;
    }
    return status;
}


/**
 * @brief Implements the interface of pdp_get_block_noop
 * 
 * To be used with SEPDP and a flat-file storage back-end.
 *
 * @param[in]     ctx    pointer to a context structure.
 * @param[in]     i      the index of block.
 * @param[out]    b      pointer to an output buffer.
 * @param[in,out] len    initially the size of buf, output bytes written.
 * @return 0 on success, non-zero on error
 **/
int sepdp_get_block_file(const pdp_ctx_t* ctx, unsigned int i, 
                          void *b, unsigned int *len)
{
    if (!is_sepdp(ctx)) return -1;
    return pdp_get_block_file(ctx, i, ctx->sepdp_param->block_size, 
                              (unsigned char *) b, len);
}

/** @} */
