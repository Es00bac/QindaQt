package org.qindaqt.link.android;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.URI;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509ExtendedKeyManager;
import javax.net.ssl.X509TrustManager;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.LinkEngine;

/**
 * TLS 1.3 client with mutual authentication and no PKI.
 *
 * AGENT-NOTE: trust is by pinned SPKI, decided by the Session after the
 * handshake (docs/wiki/adr/0434). The TrustManager therefore accepts any chain
 * and only exists so the handshake can complete; it must never be reused for
 * anything that relies on certificate validation.
 */
final class TlsConnector implements LinkEngine.Connector {
    private final Identity id;
    private final String host;
    private final int port;

    TlsConnector(Identity id, String host, int port) {
        this.id = id;
        this.host = host;
        this.port = port;
    }

    @Override public Link connect() throws IOException {
        try {
            SSLContext ctx = SSLContext.getInstance("TLS");
            ctx.init(new javax.net.ssl.KeyManager[] {new FixedKeyManager(id)}, new TrustManager[] {new AcceptAny()}, null);
            Socket raw = new Socket();
            raw.connect(new InetSocketAddress(host, port), 3000);
            raw.setTcpNoDelay(true);
            final SSLSocket s = (SSLSocket) ctx.getSocketFactory().createSocket(raw, host, port, true);
            s.setEnabledProtocols(new String[] {"TLSv1.3"});
            s.setSoTimeout(10000); // handshake bound only; cleared below
            s.startHandshake();
            s.setSoTimeout(0);
            Certificate[] peer = s.getSession().getPeerCertificates();
            final byte[] peerSpki = MessageDigest.getInstance("SHA-256").digest(peer[0].getPublicKey().getEncoded());
            return new Link() {
                @Override public InputStream in() throws IOException { return s.getInputStream(); }
                @Override public OutputStream out() throws IOException { return s.getOutputStream(); }
                @Override public byte[] peerSpki() { return peerSpki; }
                @Override public byte[] ownSpki() { return id.spki; }
                @Override public void close() { try { s.close(); } catch (IOException ignored) { /* closing */ } }
            };
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new IOException("tls setup failed: " + e.getClass().getSimpleName(), e);
        }
    }

    private static final class AcceptAny implements X509TrustManager {
        @Override public void checkClientTrusted(X509Certificate[] c, String t) { }
        @Override public void checkServerTrusted(X509Certificate[] c, String t) { }
        @Override public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
    }

    private static final class FixedKeyManager extends X509ExtendedKeyManager {
        private final Identity id;
        FixedKeyManager(Identity id) { this.id = id; }
        @Override public String[] getClientAliases(String k, Principal[] i) { return new String[] {Identity.ALIAS_NAME}; }
        @Override public String chooseClientAlias(String[] k, Principal[] i, Socket s) { return Identity.ALIAS_NAME; }
        @Override public String chooseEngineClientAlias(String[] k, Principal[] i, javax.net.ssl.SSLEngine e) { return Identity.ALIAS_NAME; }
        @Override public String[] getServerAliases(String k, Principal[] i) { return null; }
        @Override public String chooseServerAlias(String k, Principal[] i, Socket s) { return null; }
        @Override public X509Certificate[] getCertificateChain(String a) { return new X509Certificate[] {id.cert}; }
        @Override public PrivateKey getPrivateKey(String a) { return id.key; }
    }
}
