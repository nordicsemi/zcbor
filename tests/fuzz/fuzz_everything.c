#include "everything_decode.h"
#include "main_entry.h"

bool fuzz_one_input(const uint8_t *data, size_t size)
{
    size_t payload_len_out = 0;
    struct EverythingUnion result;
    bool ret = cbor_decode_EverythingUnion(data, size,
                               &result,
                               &payload_len_out);
    return ret;
}
