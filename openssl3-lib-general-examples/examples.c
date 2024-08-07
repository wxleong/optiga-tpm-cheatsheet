/**
 * MIT License
 *
 * Copyright (c) 2021 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 */

#include <string.h>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>

#define PRINT(...) printf(__VA_ARGS__); \
                    printf("\n");

#define RSA_KEY_PATH "/tmp/rsa-key"
#define EC_KEY_PATH "/tmp/ec-key"

int
gen_random()
{
    unsigned char buf[4];

    int rc = RAND_bytes(buf, sizeof(buf));

    if(rc != 1) {
        PRINT("RAND_bytes failed");
        return -1;
    }

    PRINT("Obtained random: %02x%02x%02x%02x", buf[0], buf[1], buf[2], buf[3]);

    return 0;
}

int
gen_rsaKey()
{
    int ret = 1;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    unsigned int bits = 3072;
    BIO *out = NULL;

    /**
     * For more options please refer to the tpm2 provider:
     * https://github.com/tpm2-software/tpm2-openssl/blob/1.1.0/src/tpm2-provider-keymgmt-rsa.c#L224
     */
    params[0] = OSSL_PARAM_construct_uint("bits", &bits);
    params[1] = OSSL_PARAM_construct_end();

    if ((ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", "provider=tpm2")) == NULL ||
        EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_params(ctx, params) <= 0 ||
        EVP_PKEY_generate(ctx, &pkey) <= 0) {
        PRINT("Failed to generate RSA key");
        goto exit;
    }

    // Print the public component (modulus)
    EVP_PKEY_print_public_fp(stdout, pkey, 0, NULL);

    // Store the key object on disk
    if ((out = BIO_new_file(RSA_KEY_PATH, "w")) == NULL) {
        PRINT("Failed to create a new file");
        goto exit;
    }
    if (!PEM_write_bio_PrivateKey(out, pkey, 0, NULL, 0, 0, NULL)) {
        PRINT("Failed to write RSA key to disk");
        goto exit;
    }

    ret = 0;
    PRINT("Generated RSA key and saved to disk");

exit:
    out ? BIO_free_all(out) : 0;
    pkey ? EVP_PKEY_free(pkey) : 0;
    return ret;
}

int
gen_ecKey()
{
    int ret = 1;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    BIO *out = NULL;
    char *group = "P-256";

    /**
     * For more options please refer to the tpm2 provider:
     * https://github.com/tpm2-software/tpm2-openssl/blob/1.1.0/src/tpm2-provider-keymgmt-ec.c#L183
     */
    params[0] = OSSL_PARAM_construct_utf8_string("group", group, sizeof(group));
    params[1] = OSSL_PARAM_construct_end();

    if ((ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", "provider=tpm2")) == NULL ||
        EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_params(ctx, params) <= 0 ||
        EVP_PKEY_generate(ctx, &pkey) <= 0) {
        PRINT("Failed to generate EC key");
        goto exit;
    }

    // Print the public component
    EVP_PKEY_print_public_fp(stdout, pkey, 0, NULL);

    // Store the key object on disk
    if ((out = BIO_new_file(EC_KEY_PATH, "w")) == NULL) {
        PRINT("Failed to create a new file");
        goto exit;
    }
    if (!PEM_write_bio_PrivateKey(out, pkey, 0, NULL, 0, 0, NULL)) {
        PRINT("Failed to write EC key to disk");
        goto exit;
    }

    ret = 0;
    PRINT("Generated EC key and saved to disk");

exit:
    out ? BIO_free_all(out) : 0;
    pkey ? EVP_PKEY_free(pkey) : 0;
    return ret;
}

EVP_PKEY *
load_rsa_key()
{
    EVP_PKEY *pKey = NULL;
    BIO *bio = NULL;

    if ((bio = BIO_new_file(RSA_KEY_PATH, "r")) == NULL) {
        PRINT("Failed to open RSA_KEY_PATH");
        goto exit;
    }

    if ((pKey = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL)) == NULL) {
        PRINT("Failed to read RSA key");
        goto exit;
    }

    PRINT("Loaded RSA key from disk");

exit:
    bio ? BIO_free_all(bio) : 0;
    return pKey;
}

EVP_PKEY *
load_ec_key()
{
    EVP_PKEY *pKey = NULL;
    BIO *bio = NULL;

    if ((bio = BIO_new_file(EC_KEY_PATH, "r")) == NULL) {
        PRINT("Failed to open RSA_KEY_PATH");
        goto exit;
    }

    if ((pKey = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL)) == NULL) {
        PRINT("Failed to read EC key");
        goto exit;
    }

    PRINT("Loaded EC key from disk");

exit:
    bio ? BIO_free_all(bio) : 0;
    return pKey;
}

int
ec_evp_pkey_sign_verify(EVP_PKEY *pKey)
{
    BIO *bio = NULL;
    EVP_PKEY *pPubKey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY_CTX *ctx2 = NULL;
    unsigned char sha256[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char *sig = NULL;
    size_t sha256Len = 32, sigLen = 0;
    int ret = -1;

    //ctx = EVP_PKEY_CTX_new(pKey, NULL);
    ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pKey, "provider=tpm2");
    if (!ctx) {
        PRINT("EC sign EVP_PKEY_CTX_new_from_pkey error");
        goto exit;
    }

    /* Signing */

    PRINT("EC signing");

    if (EVP_PKEY_sign_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_sign(ctx, NULL, &sigLen, sha256, sha256Len) <= 0) {
        PRINT("EC sign init error");
        goto exit;
    }

    sig = OPENSSL_malloc(sigLen);

    if (!sig) {
        PRINT("EC malloc error");
        goto exit;
    }

    PRINT("EC generating signature");

    if (EVP_PKEY_sign(ctx, sig, &sigLen, sha256, sha256Len) <= 0) {
        PRINT("EC signing error");
        goto exit;
    }

    /* Verification */

    PRINT("EC verify signature");

    if (!(bio = BIO_new(BIO_s_mem()))) {
        PRINT("BIO_new error");
        goto exit;
    }

    if (PEM_write_bio_PUBKEY(bio, pKey) <= 0) {
        PRINT("PEM_write_bio_PUBKEY error");
        goto exit;
    }

    if (!(pPubKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL))) {
        PRINT("PEM_read_bio_PUBKEY error");
        goto exit;
    }

    if (!(ctx2 = EVP_PKEY_CTX_new_from_pkey(NULL, pPubKey, "provider=default"))) {
        PRINT("EVP_PKEY_CTX_new_from_pkey error");
        goto exit;
    }

    if (EVP_PKEY_verify_init(ctx2) <= 0 ||
        EVP_PKEY_CTX_set_signature_md(ctx2, EVP_sha256()) <= 0) {
        PRINT("EC verification init error");
        goto exit;
    }

    if (EVP_PKEY_verify(ctx2, sig, sigLen, sha256, sha256Len) <= 0) {
        PRINT("EC signature verification error");
        goto exit;
    }

    PRINT("EC signature verification ok");

    // corrupt the hash
    sha256[3] = ~sha256[3];
    if (EVP_PKEY_verify(ctx2, sig, sigLen, sha256, sha256Len) == 0) {
        PRINT("EC signature verification expected to fail, ok");
    } else {
        PRINT("EC signature verification error");
        goto exit;
    }

    ret = 0;

exit:
    ctx2 ? EVP_PKEY_CTX_free(ctx2) : 0;
    pPubKey ? EVP_PKEY_free(pPubKey) : 0;
    bio ? BIO_free(bio) : 0;
    sig ? OPENSSL_free(sig) : 0;
    ctx ? EVP_PKEY_CTX_free(ctx) : 0;
    return ret;
}

int
rsa_evp_pkey_sign_verify(EVP_PKEY *pKey)
{
    BIO *bio = NULL;
    EVP_PKEY *pPubKey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY_CTX *ctx2 = NULL;
    unsigned char sha256[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char *sig = NULL;
    size_t sha256Len = 32, sigLen = 0;
    int ret = -1;

    ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pKey, "provider=tpm2");
    if (!ctx) {
        PRINT("RSA sign EVP_PKEY_CTX_new_from_pkey error");
        goto exit;
    }

    /* Signing */

    PRINT("RSA signing");
    if (EVP_PKEY_sign_init(ctx) <= 0 ) {
        PRINT("RSA sign init error");
        goto exit;
    }
    if ( EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <=0) {
        PRINT("set md error");
        goto exit;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0) {
        PRINT("EVP_PKEY_CTX_set_rsa_padding error");
        goto exit;
    }

    if (EVP_PKEY_sign(ctx, NULL, &sigLen, sha256, sha256Len) <= 0) {
        PRINT("get siglen error");
        goto exit;
    }

    sig = OPENSSL_malloc(sigLen);

    if (!sig) {
        PRINT("RSA malloc error");
        goto exit;
    }

    PRINT("RSA generating signature");

    if (EVP_PKEY_sign(ctx, sig, &sigLen, sha256, sha256Len) <= 0) {
        PRINT("RSA signing error");
        goto exit;
    }

    /* Verification */

    PRINT("RSA verify signature");

    if (!(bio = BIO_new(BIO_s_mem()))) {
        PRINT("BIO_new error");
        goto exit;
    }

    if (PEM_write_bio_PUBKEY(bio, pKey) <= 0) {
        PRINT("PEM_write_bio_PUBKEY error");
        goto exit;
    }

    if (!(pPubKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL))) {
        PRINT("PEM_read_bio_PUBKEY error");
        goto exit;
    }

    if (!(ctx2 = EVP_PKEY_CTX_new_from_pkey(NULL, pPubKey, "provider=default"))) {
        PRINT("EVP_PKEY_CTX_new_from_pkey error");
        goto exit;
    }

    if (EVP_PKEY_verify_init(ctx2) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(ctx2, RSA_PKCS1_PSS_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_signature_md(ctx2, EVP_sha256()) <= 0) {
        PRINT("RSA verification init error");
        goto exit;
    }

    if (EVP_PKEY_verify(ctx2, sig, sigLen, sha256, sha256Len) <= 0) {
        PRINT("RSA signature verification error");
        goto exit;
    }

    PRINT("RSA signature verification ok");

    // corrupt the hash
    sha256[3] = ~sha256[3];
    if (EVP_PKEY_verify(ctx2, sig, sigLen, sha256, sha256Len) == 0) {
        PRINT("RSA signature verification expected to fail, ok");
    } else {
        PRINT("RSA signature verification error");
        goto exit;
    }

    ret = 0;

exit:
    ctx2 ? EVP_PKEY_CTX_free(ctx2) : 0;
    pPubKey ? EVP_PKEY_free(pPubKey) : 0;
    bio ? BIO_free(bio) : 0;
    sig ? OPENSSL_free(sig) : 0;
    ctx ? EVP_PKEY_CTX_free(ctx) : 0;
    return ret;
}

int
rsa_evp_pkey_encrypt_decrypt(EVP_PKEY *pKey)
{
    BIO *bio = NULL;
    EVP_PKEY *pPubKey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY_CTX *ctx2 = NULL;
    unsigned char clear[] = {1,2,3};
    unsigned char *ciphered = NULL, *deciphered = NULL;
    size_t cipheredLen = 0, decipheredLen = 0, clearLen = 3;
    int ret = -1;

    /* Encryption (RSA_PKCS1_PADDING == TPM2_ALG_RSAES) */

    if (!(bio = BIO_new(BIO_s_mem()))) {
        PRINT("BIO_new error");
        goto exit;
    }

    if (PEM_write_bio_PUBKEY(bio, pKey) <= 0) {
        PRINT("PEM_write_bio_PUBKEY error");
        goto exit;
    }

    if (!(pPubKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL))) {
        PRINT("PEM_read_bio_PUBKEY error");
        goto exit;
    }

    if (!(ctx = EVP_PKEY_CTX_new_from_pkey(NULL, pPubKey, "provider=default"))) {
        PRINT("EVP_PKEY_CTX_new_from_pkey error");
        goto exit;
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0 ||
        EVP_PKEY_encrypt(ctx, NULL, &cipheredLen, clear, clearLen) <= 0) {
        PRINT("Encryption init error");
        goto exit;
    }

    ciphered = OPENSSL_malloc(cipheredLen);
    if (!ciphered) {
        PRINT("malloc error");
        goto exit;
    }

    PRINT("Generating encryption blob");

    if (EVP_PKEY_encrypt(ctx, ciphered, &cipheredLen, clear, clearLen) <= 0) {
        PRINT("Encryption error");
        goto exit;
    }

    /* Decryption (RSA_PKCS1_PADDING == TPM2_ALG_RSAES) */

    ctx2 = EVP_PKEY_CTX_new_from_pkey(NULL, pKey, "provider=tpm2");
    if (!ctx2) {
        PRINT("RSA decrypt EVP_PKEY_CTX_new_from_pkey error");
        goto exit;
    }

    if (EVP_PKEY_decrypt_init(ctx2) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(ctx2, RSA_PKCS1_PADDING) <= 0 ||
        EVP_PKEY_decrypt(ctx2, NULL, &decipheredLen, ciphered, cipheredLen) <= 0) {
        PRINT("Decryption init error");
        goto exit;
    }

    deciphered = OPENSSL_malloc(decipheredLen);
    if (!deciphered) {
        PRINT("malloc error");
        goto exit;
    }

    memset(deciphered, 0, decipheredLen);

    PRINT("Decrypting encrypted blob");

    if (EVP_PKEY_decrypt(ctx2, deciphered, &decipheredLen, ciphered, cipheredLen) <= 0) {
        PRINT("Decryption error");
        goto exit;
    }

    if((decipheredLen != clearLen) || (strncmp((const char *)clear, (const char *)deciphered, decipheredLen) != 0))
    {
        PRINT("Decryption error, value not the same");
        goto exit;
    }

    PRINT("Decryption verification ok");

    ret = 0;

exit:
    deciphered ? OPENSSL_free(deciphered) : 0;
    ctx2 ? EVP_PKEY_CTX_free(ctx2) : 0;
    ciphered ? OPENSSL_free(ciphered) : 0;
    ctx ? EVP_PKEY_CTX_free(ctx) : 0;
    pPubKey ? EVP_PKEY_free(pPubKey) : 0;
    bio ? BIO_free(bio) : 0;
    return ret;
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    int ret = 1;
    OSSL_PROVIDER *prov_default = NULL;
    OSSL_PROVIDER *prov_tpm2 = NULL;
    EVP_PKEY *pRsaKey = NULL;
    EVP_PKEY *pEcKey = NULL;

    PRINT("Starting...");

    /*
     * Known issue:
     *
     * Cant set tcti programmatically
     * Open topic: https://github.com/openssl/openssl/issues/17182
     * Tentatively, set parameters feature will be implemented in OpenSSL 3.1
     *
     * Here we relies on ENV TPM2OPENSSL_TCTI
     */

    /* Load TPM2 provider */
    if ((prov_tpm2 = OSSL_PROVIDER_load(NULL, "tpm2")) == NULL)
        goto exit;

    /* Self-test */
    if (!OSSL_PROVIDER_self_test(prov_tpm2))
        goto exit;

    /* Load default provider */
    if ((prov_default = OSSL_PROVIDER_load(NULL, "default")) == NULL)
        goto exit;

    /* Self-test */
    if (!OSSL_PROVIDER_self_test(prov_default))
        goto exit;

    /* Generate true random */
    if (gen_random())
        goto exit;

    /* Generate RSA key */
    if (gen_rsaKey())
        goto exit;

    /* Generate EC key */
    if (gen_ecKey())
        goto exit;

    /* Load RSA key */
    if ((pRsaKey = load_rsa_key()) == NULL)
        goto exit;

    /* Load EC key */
    if ((pEcKey = load_ec_key()) == NULL)
        goto exit;

    /* RSA signing & verification */
    if (rsa_evp_pkey_sign_verify(pRsaKey))
        goto exit;

    /* EC signing & verification */
    if (ec_evp_pkey_sign_verify(pEcKey))
        goto exit;

    /* RSA encryption & decryption */
    if (rsa_evp_pkey_encrypt_decrypt(pRsaKey))
        goto exit;

    PRINT("Completed without err...");

    ret = 0;

exit:
    pEcKey ? EVP_PKEY_free(pEcKey) : 0;
    pRsaKey ? EVP_PKEY_free(pRsaKey) : 0;
    prov_tpm2 ? OSSL_PROVIDER_unload(prov_tpm2) : 0;
    prov_default ? OSSL_PROVIDER_unload(prov_default) : 0;

    return ret;
}
