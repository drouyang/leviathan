/*
 * libhio (Hobbes I/O) client library
 * (c) Brian Kocoloski, 2016
 */

#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <hobbes_cmd_queue.h>
#include <hobbes_util.h>
#include <xemem.h>
#include <pet_xml.h>
#include <pet_log.h>

#include <libhio.h>

typedef enum {
    HIO_INVALID,
    HIO_RANK,
    HIO_APP,
} client_mode_t;


static hcq_handle_t  hio_hcq  = HCQ_INVALID_HANDLE;
static client_mode_t hio_mode = HIO_INVALID;
static uint32_t      hio_rank = (uint32_t)-1;


static int
__hcq_init(char * hcq_name)
{
    xemem_segid_t hcq_segid = XEMEM_INVALID_SEGID;

    hcq_segid = xemem_lookup_segid(hcq_name);
    if (hcq_segid == XEMEM_INVALID_SEGID) {
        ERROR("Cannot find XEMEM segid for hcq name %s\n", hcq_name);
        return -ENOENT;
    }

    /* Open the HCQ */
    hio_hcq = hcq_connect(hcq_segid);
    if (hio_hcq == HCQ_INVALID_HANDLE) {
        ERROR("Cannot connect to HCQ\n");
        return -1;
    }

    return 0;
}

static void
__hcq_deinit(void)
{
    assert(hio_hcq != HCQ_INVALID_HANDLE);

    hcq_disconnect(hio_hcq);

    hio_hcq = HCQ_INVALID_HANDLE;
}


int
libhio_client_init(char   * hcq_name,
                   uint32_t rank)
{
    int status;

    if (hio_mode != HIO_INVALID)
        return -EALREADY;

    if (rank == (uint32_t)-1)
        return -EINVAL;

    status = __hcq_init(hcq_name);
    if (status)
        return status;

    hio_rank = rank;
    hio_mode = HIO_RANK;

    return 0;
}

int
libhio_client_init_app(char  * hcq_name)
{
    int status;

    if (hio_mode != HIO_INVALID)
        return -EALREADY;

    status = __hcq_init(hcq_name);
    if (status)
        return status;

    hio_mode = HIO_APP;

    return 0;

}

void
libhio_client_deinit(void)
{
    if (hio_mode == HIO_INVALID)
        return;

    __hcq_deinit();

    hio_mode = HIO_INVALID;
}


static int
__libhio_client_call_rank_stub_fn(uint64_t    cmd_code,
                                  uint32_t    rank,
                                  pet_xml_t   hio_xml,
                                  hio_ret_t * hio_ret)
{
    char      tmp_str[64] = {0};
    char    * xml_str     = NULL;
    hcq_cmd_t cmd         = HCQ_INVALID_CMD;
    int       status      = 0;
    uint32_t  xml_size    = 0;
    pet_xml_t xml_resp    = PET_INVALID_XML;

    *hio_ret = -HIO_CLIENT_ERROR;

    /* Add cmd and rank to xml */
    snprintf(tmp_str, 64, "%lu", cmd_code);
    status = pet_xml_add_val(hio_xml, "cmd", tmp_str);
    if (status != 0)
        return -HIO_CLIENT_ERROR;

    snprintf(tmp_str, 64, "%u", rank);
    status = pet_xml_add_val(hio_xml, "rank", tmp_str);
    if (status != 0)
        return -HIO_CLIENT_ERROR;

    /* Convert to str */
    xml_str = pet_xml_get_str(hio_xml);
    if (xml_str == NULL)
        return -HIO_CLIENT_ERROR;

    /* Issue HCQ command */
    cmd = hcq_cmd_issue(
            hio_hcq,
            HIO_CMD_CODE,
            strlen(xml_str),
            xml_str);

    free(xml_str);

    if (cmd == HCQ_INVALID_CMD)
        return -HIO_BAD_CLIENT_HCQ;

    /* Get HCQ response */
    status = hcq_get_ret_code(hio_hcq, cmd);
    if (status != HIO_SUCCESS)
        return status;

    /* Response is an XML */
    xml_str = hcq_get_ret_data(hio_hcq, cmd, &xml_size);
    assert((xml_size > 0) && (xml_str != NULL));

    xml_resp = pet_xml_parse_str(xml_str);

    /* TODO: map XEMEM segids in seg list */

    /* Get return value */
    *hio_ret = smart_atoi(-HIO_SERVER_ERROR, pet_xml_get_val(xml_resp, "ret"));

    pet_xml_free(xml_resp);

    /* Complete HCQ command */
    hcq_cmd_complete(hio_hcq, cmd);

    return HIO_SUCCESS;
}


int
libhio_client_call_stub_fn(uint64_t    cmd,
                           pet_xml_t   hio_xml,
                           hio_ret_t * hio_ret)
{
    if (hio_mode != HIO_RANK)
        return -HIO_WRONG_MODE;

    return __libhio_client_call_rank_stub_fn(
            cmd,
            hio_rank,
            hio_xml,
            hio_ret
        );
}

int
libhio_client_call_rank_stub_fn(uint64_t    cmd,
                                uint32_t    rank,
                                pet_xml_t   hio_xml,
                                hio_ret_t * hio_ret)
{
    if (hio_mode != HIO_APP)
        return -HIO_WRONG_MODE;

    return __libhio_client_call_rank_stub_fn(
            cmd,
            rank,
            hio_xml,
            hio_ret
        );
}
