package org.qindaqt.link.sms;

/** Maps a phone number or address to a contact display name; "" when unknown or not permitted. */
public interface ContactResolver {
    String name(String address);
}
