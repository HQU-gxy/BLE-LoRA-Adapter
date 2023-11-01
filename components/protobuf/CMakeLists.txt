set(NANOPB_INCLUDE_DIR
        nanopb)

set(GEN_PROTOBUF_INCLUDE_DIR
        out)

set(HELPER_INCLUDE_DIR
        helper)

idf_component_register(
        SRCS
        nanopb/pb_common.c
        nanopb/pb_decode.c
        nanopb/pb_encode.c
        out/ble.pb.c
        helper/whitelist.cpp

        INCLUDE_DIRS
        ${NANOPB_INCLUDE_DIR}
        ${GEN_PROTOBUF_INCLUDE_DIR}
        ${HELPER_INCLUDE_DIR}

        REQUIRES
        etl
)
