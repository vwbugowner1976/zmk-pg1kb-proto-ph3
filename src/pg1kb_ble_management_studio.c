#include <errno.h>
#include <stdint.h>

#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/ble.h>
#include <zmk/studio/custom.h>

#include <mykeeb/ble/ble_management.pb.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(ZMK_BLE_PROFILE_COUNT <= 32, "BLE management status masks support at most 32 profiles");

static struct zmk_rpc_custom_subsystem_meta ble_management_meta = {
    .security = ZMK_STUDIO_RPC_HANDLER_SECURED,
};

static bool ble_management_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                              pb_callback_t *encode_response);

ZMK_RPC_CUSTOM_SUBSYSTEM(mykeeb__ble_management, &ble_management_meta,
                         ble_management_rpc_handle_request);
ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(mykeeb__ble_management, mykeeb_ble_Response);

static mykeeb_ble_StatusResponse current_status(void) {
    mykeeb_ble_StatusResponse status = mykeeb_ble_StatusResponse_init_zero;
    status.active_profile = (uint32_t)zmk_ble_active_profile_index();
    status.profile_count = ZMK_BLE_PROFILE_COUNT;

    for (uint8_t index = 0; index < ZMK_BLE_PROFILE_COUNT; index++) {
        const uint32_t bit = BIT(index);
        if (!zmk_ble_profile_is_open(index)) {
            status.bonded_mask |= bit;
        } else {
            status.open_mask |= bit;
        }
        if (zmk_ble_profile_is_connected(index)) {
            status.connected_mask |= bit;
        }
    }

    return status;
}

static void set_status_response(mykeeb_ble_Response *resp) {
    resp->which_response_type = mykeeb_ble_Response_status_tag;
    resp->response_type.status = current_status();
}

static void set_command_response(mykeeb_ble_Response *resp, int rc) {
    mykeeb_ble_CommandResponse result = mykeeb_ble_CommandResponse_init_zero;
    result.status = rc;
    result.state = current_status();
    resp->which_response_type = mykeeb_ble_Response_command_tag;
    resp->response_type.command = result;
}

static void set_error_response(mykeeb_ble_Response *resp, int rc) {
    mykeeb_ble_ErrorResponse result = mykeeb_ble_ErrorResponse_init_zero;
    result.status = rc;
    resp->which_response_type = mykeeb_ble_Response_error_tag;
    resp->response_type.error = result;
}

static int validate_profile(uint32_t index) {
    return index < ZMK_BLE_PROFILE_COUNT ? 0 : -EINVAL;
}

static int select_profile(uint32_t index) {
    int rc = validate_profile(index);
    if (rc != 0) {
        return rc;
    }
    return zmk_ble_prof_select((uint8_t)index);
}

static int clear_profile(uint32_t index) {
    int rc = validate_profile(index);
    if (rc != 0) {
        return rc;
    }

    if ((uint32_t)zmk_ble_active_profile_index() != index) {
        rc = zmk_ble_prof_select((uint8_t)index);
        if (rc != 0) {
            return rc;
        }
    }

    zmk_ble_clear_bonds();
    return 0;
}

static int disconnect_profile(uint32_t index) {
    int rc = validate_profile(index);
    if (rc != 0) {
        return rc;
    }
    return zmk_ble_prof_disconnect((uint8_t)index);
}

static bool ble_management_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                              pb_callback_t *encode_response) {
    mykeeb_ble_Response *resp = ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(
        mykeeb__ble_management, encode_response);
    mykeeb_ble_Request req = mykeeb_ble_Request_init_zero;

    pb_istream_t stream = pb_istream_from_buffer(raw_request->payload.bytes, raw_request->payload.size);
    if (!pb_decode(&stream, mykeeb_ble_Request_fields, &req)) {
        LOG_WRN("BLE management RPC decode failed: %s", PB_GET_ERROR(&stream));
        set_error_response(resp, -EINVAL);
        return true;
    }

    int rc = 0;
    switch (req.which_request_type) {
    case mykeeb_ble_Request_get_status_tag:
        set_status_response(resp);
        return true;

    case mykeeb_ble_Request_select_profile_tag:
        rc = select_profile(req.request_type.select_profile.index);
        break;

    case mykeeb_ble_Request_clear_profile_tag:
        rc = clear_profile(req.request_type.clear_profile.index);
        break;

    case mykeeb_ble_Request_disconnect_profile_tag:
        rc = disconnect_profile(req.request_type.disconnect_profile.index);
        break;

    case mykeeb_ble_Request_clear_all_tag:
        zmk_ble_clear_all_bonds();
        rc = 0;
        break;

    default:
        LOG_WRN("BLE management RPC unsupported request type: %d", req.which_request_type);
        set_error_response(resp, -ENOTSUP);
        return true;
    }

    LOG_INF("BLE management RPC command=%d rc=%d active=%d", req.which_request_type, rc,
            zmk_ble_active_profile_index());
    set_command_response(resp, rc);
    return true;
}
