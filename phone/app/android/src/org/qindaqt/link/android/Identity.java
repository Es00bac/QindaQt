package org.qindaqt.link.android;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.security.spec.ECGenParameterSpec;
import java.util.Base64;
import javax.security.auth.x500.X500Principal;

/**
 * The phone's TLS identity: an EC P-256 key in the AndroidKeyStore (TEE where
 * available, no user-authentication requirement so the service can use it
 * unattended) with the self-signed certificate the keystore generates.
 *
 * AGENT-GUARD: never export or log the private key; it cannot leave the
 * keystore. The deviceId is derived from the SPKI, not the certificate bytes,
 * so certificate regeneration with the same key keeps the identity.
 */
final class Identity {
    private static final String ALIAS = "qindalink-identity";
    static final String ALIAS_NAME = ALIAS;

    final PrivateKey key;
    final X509Certificate cert;
    final byte[] spki;

    private Identity(PrivateKey key, X509Certificate cert) throws Exception {
        this.key = key;
        this.cert = cert;
        this.spki = MessageDigest.getInstance("SHA-256").digest(cert.getPublicKey().getEncoded());
    }

    static Identity loadOrCreate() throws Exception {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);
        if (!ks.containsAlias(ALIAS)) {
            KeyPairGenerator g = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
            g.initialize(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                    // NONE is needed by some TLS 1.3 signature paths in Conscrypt.
                    .setDigests(KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_NONE)
                    .setCertificateSubject(new X500Principal("CN=QindaLink phone"))
                    .setUserAuthenticationRequired(false)
                    .build());
            g.generateKeyPair();
        }
        return new Identity((PrivateKey) ks.getKey(ALIAS, null), (X509Certificate) ks.getCertificate(ALIAS));
    }

    /**
     * Writes the PUBLIC certificate as PEM. Used only by the dev fake desktop
     * (phone/tools/fake-desktop.py) to trust this phone; contains no secret.
     */
    void exportCertificatePem(File f) throws IOException {
        try {
            String b64 = Base64.getMimeEncoder(64, "\n".getBytes(StandardCharsets.US_ASCII)).encodeToString(cert.getEncoded());
            String pem = "-----BEGIN CERTIFICATE-----\n" + b64 + "\n-----END CERTIFICATE-----\n";
            Files.write(f.toPath(), pem.getBytes(StandardCharsets.US_ASCII));
        } catch (java.security.cert.CertificateEncodingException e) {
            throw new IOException(e);
        }
    }
}
