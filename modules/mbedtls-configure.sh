to_disable=(MBEDTLS_FS_IO
            MBEDTLS_ENTROPY_NV_SEED
            MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
            MBEDTLS_ARC4_C
            MBEDTLS_BLOWFISH_C
            MBEDTLS_CAMELLIA_C
            MBEDTLS_CERTS_C
            MBEDTLS_DEBUG_C
            MBEDTLS_DES_C
            MBEDTLS_ERROR_C
            MBEDTLS_NET_C
            MBEDTLS_PEM_WRITE_C
            MBEDTLS_PK_WRITE_C
            MBEDTLS_PLATFORM_C
            MBEDTLS_RIPEMD160_C
            MBEDTLS_SELF_TEST
            MBEDTLS_SSL_PROTO_DTLS
            MBEDTLS_SSL_DTLS_HELLO_VERIFY
            MBEDTLS_SSL_DTLS_ANTI_REPLAY
            MBEDTLS_SSL_DTLS_BADMAC_LIMIT
            MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
            MBEDTLS_TIMING_C
            MBEDTLS_VERSION_C
            MBEDTLS_VERSION_FEATURES
            MBEDTLS_X509_CRL_PARSE_C
            MBEDTLS_X509_CSR_PARSE_C
            MBEDTLS_X509_CREATE_C
            MBEDTLS_X509_CRT_WRITE_C
            MBEDTLS_X509_CSR_WRITE_C
            MBEDTLS_XTEA_C)

to_enable=(MBEDTLS_THREADING_C
           MBEDTLS_THREADING_PTHREAD
           MBEDTLS_KEY_EXCHANGE_DH_ANON_ENABLED)

cd mbedtls

for v in ${to_disable[@]}; do
  scripts/config.pl unset $v
done

for v in ${to_enable[@]}; do
  scripts/config.pl set $v
done