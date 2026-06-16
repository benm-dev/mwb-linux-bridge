#include "CryptoHelper.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <limits>

namespace mwb {

CryptoHelper::CryptoHelper(const std::string& securityKey) : m_securityKey(securityKey), m_encryptCtx(nullptr), m_decryptCtx(nullptr) {
    std::string ivStr = "1844674407370955";
    m_iv.resize(16);
    std::memcpy(m_iv.data(), ivStr.data(), 16);

    std::string fullIv = "18446744073709551615";
    std::vector<uint8_t> salt;
    for (char c : fullIv) {
        salt.push_back(c);
        salt.push_back(0);
    }

    m_key.resize(32);
    const auto maxInt = static_cast<size_t>(std::numeric_limits<int>::max());
    if (m_securityKey.size() > maxInt || salt.size() > maxInt) {
        throw std::runtime_error("Security key or salt is too large");
    }
    if (!PKCS5_PBKDF2_HMAC(m_securityKey.c_str(), static_cast<int>(m_securityKey.length()),
                           salt.data(), static_cast<int>(salt.size()),
                           50000, EVP_sha512(),
                           32, m_key.data())) {
        throw std::runtime_error("PBKDF2 HMAC Failed");
    }

    m_encryptCtx = EVP_CIPHER_CTX_new();
    if (!m_encryptCtx ||
        EVP_EncryptInit_ex(m_encryptCtx, EVP_aes_256_cbc(), nullptr, m_key.data(), m_iv.data()) != 1 ||
        EVP_CIPHER_CTX_set_padding(m_encryptCtx, 0) != 1) {
        throw std::runtime_error("OpenSSL encrypt context initialization failed");
    }

    m_decryptCtx = EVP_CIPHER_CTX_new();
    if (!m_decryptCtx ||
        EVP_DecryptInit_ex(m_decryptCtx, EVP_aes_256_cbc(), nullptr, m_key.data(), m_iv.data()) != 1 ||
        EVP_CIPHER_CTX_set_padding(m_decryptCtx, 0) != 1) {
        throw std::runtime_error("OpenSSL decrypt context initialization failed");
    }
}

CryptoHelper::~CryptoHelper() {
    if (!m_securityKey.empty()) OPENSSL_cleanse(m_securityKey.data(), m_securityKey.size());
    if (!m_key.empty()) OPENSSL_cleanse(m_key.data(), m_key.size());
    if (!m_iv.empty()) OPENSSL_cleanse(m_iv.data(), m_iv.size());
    if (m_encryptCtx) EVP_CIPHER_CTX_free(m_encryptCtx);
    if (m_decryptCtx) EVP_CIPHER_CTX_free(m_decryptCtx);
}

uint32_t CryptoHelper::Get24BitHash() {
    std::vector<uint8_t> bytes(32, 0);
    for (size_t i = 0; i < 32 && i < m_securityKey.length(); i++) {
        bytes[i] = static_cast<uint8_t>(m_securityKey[i]);
    }

    std::vector<uint8_t> hashValue(64);
    unsigned int len = 0;
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        throw std::runtime_error("OpenSSL digest context allocation failed");
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha512(), nullptr) != 1 ||
        EVP_DigestUpdate(mdctx, bytes.data(), bytes.size()) != 1 ||
        EVP_DigestFinal_ex(mdctx, hashValue.data(), &len) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("OpenSSL digest failed");
    }

    for (int i = 0; i < 50000; i++) {
        if (EVP_DigestInit_ex(mdctx, EVP_sha512(), nullptr) != 1 ||
            EVP_DigestUpdate(mdctx, hashValue.data(), hashValue.size()) != 1 ||
            EVP_DigestFinal_ex(mdctx, hashValue.data(), &len) != 1) {
            EVP_MD_CTX_free(mdctx);
            throw std::runtime_error("OpenSSL digest failed");
        }
    }
    EVP_MD_CTX_free(mdctx);

    // Match C# Encryption.Get24BitHash exactly:
    // return (uint)((hashValue[0] << 23) + (hashValue[1] << 16) + (hashValue[^1] << 8) + hashValue[2]);
    uint32_t magic = (static_cast<uint32_t>(hashValue[0]) << 23) +
                     (static_cast<uint32_t>(hashValue[1]) << 16) +
                     (static_cast<uint32_t>(hashValue[63]) << 8) +
                     static_cast<uint32_t>(hashValue[2]);
    return magic;
}

void CryptoHelper::Reset() {
    if (EVP_EncryptInit_ex(m_encryptCtx, EVP_aes_256_cbc(), nullptr, m_key.data(), m_iv.data()) != 1 ||
        EVP_CIPHER_CTX_set_padding(m_encryptCtx, 0) != 1 ||
        EVP_DecryptInit_ex(m_decryptCtx, EVP_aes_256_cbc(), nullptr, m_key.data(), m_iv.data()) != 1 ||
        EVP_CIPHER_CTX_set_padding(m_decryptCtx, 0) != 1) {
        throw std::runtime_error("OpenSSL context reset failed");
    }
}

bool CryptoHelper::EncryptStream(const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext) {
    if (plaintext.empty() || plaintext.size() % 16 != 0) return false;
    if (plaintext.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
    ciphertext.resize(plaintext.size());
    int len = 0;
    if (1 != EVP_EncryptUpdate(m_encryptCtx, ciphertext.data(), &len, plaintext.data(), static_cast<int>(plaintext.size()))) return false;
    return true;
}

bool CryptoHelper::DecryptStream(const std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& plaintext) {
    if (ciphertext.empty() || ciphertext.size() % 16 != 0) return false;
    if (ciphertext.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
    plaintext.resize(ciphertext.size());
    int len = 0;
    if (1 != EVP_DecryptUpdate(m_decryptCtx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size()))) return false;
    return true;
}

}
