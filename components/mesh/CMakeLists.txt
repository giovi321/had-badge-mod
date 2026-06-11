idf_component_register(
    SRCS "aes.c" "regions.c" "mesh_crypto.c" "packet.c" "channels.c" "meshtastic_pb.c"
         "nanopb/pb_common.c" "nanopb/pb_encode.c" "nanopb/pb_decode.c"
         "generated/meshtastic.pb.c"
    INCLUDE_DIRS "include" "nanopb" "generated")
