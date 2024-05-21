set(COMPONENT_SRCS
        "http_stream.c"
        "http_playlist.c"
        "lib/hls/hls_parse.c"
        "lib/hls/hls_playlist.c"
        "lib/hls/line_reader.c"
        "lib/hls/join_path.c"
        "lib/gzip/gzip_miniz.c"
        "audio_mem.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
        "include"
        "lib/hls/include"
        "lib/hls"
        "lib/gzip"
        "lib/gzip/include"
)

set(COMPONENT_PRIV_INCLUDEDIRS
)

set(COMPONENT_REQUIRES
        "driver"
        "esp_http_client"
        "tcp_transport"
        "mbedtls"
        "esp_http_client")

register_component()
