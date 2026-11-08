#pragma once
// Squelchmoth licensing (1.9 "Store").
//  - No key: the Demo edition (free forever, basics only - see Edition.h). There is no time-limited full trial:
//    nobody gets the full program without buying it.
//  - SQM2 keys: one per product bought in the store (CORE, COMPLETE, VOCAL, MASTER, INSTR, BEAT, CREATIVE), signed
//    with the store's Ed25519 key. Format: SQM2-<base64url of product(1) | serial(4) | signature(64)> = 97 characters
//    (the store's key lists take at most 100). Several keys stack: Squelchmoth + any packs.
//  - SQM1 keys (the owner key and 1.x customers): RSA-2048, signed with Jamar Stewart's owner key; they unlock everything.
// Only public keys are compiled in. The private halves never enter the repo, a build or a zip.

#include <JuceHeader.h>
#include "Ed25519.h"
#include "Edition.h"

class Licence
{
public:
    // Owner key (SQM1, RSA-2048): modulus and exponent as a JUCE RSAKey string
    static constexpr const char* publicKey = "10001,ab7d0d76ec1459950a13e177ad10046404c6b163a79c0899110c95192447e976d922cce7ac8e3737e17bc835f3b4f041517a6bd70454a3c94bb2e4d1aa2035dbdbd0dc2bedbb8fd39c080728b576e54ff92685beb4da854413863e68d2d0eddc293e677da38a2f2757f5f1f15ca364c839ab7140906bd49326bb329e718c3ababa3ea306f4549af04fb157fd4610e9f6f2c85835f44371589ae9fe669f196280c861fe9ff354e034d039b33782226bfe57f27cda063dad91a83e5538ffa3afce17531cd14fd8278ba7a206c88a910c9fb83c9a18d6ead4e5e58bce7a90b0c2b92d549e839dd872651b587dbd40bffa6efee10eade75d5b78e1ecd05c918a5f3f";   // RSA-2048 (1.7)
    // Store key (SQM2, Ed25519 public key, hex)
    static constexpr const char* storePublicKey = "08fda090332e4a8e60de356271e589b6e6ceb2f01e04b4aefb23ac62f3511569";
    /** Serials revoked after a refund or a leak (hex, 8 characters). Updated with each release. */
    // serials switched off: refunds, shared keys, and the 1.9.0 sandbox dev key
    static const juce::StringArray& revokedSerials()   { static const juce::StringArray r { "E28E5F72" }; return r; }

    explicit Licence (juce::PropertiesFile& p) : props (p)
    {
        juce::StringArray keys; keys.addLines (props.getValue ("licenceKeys"));
        if (props.getValue ("licenceKey").isNotEmpty()) keys.addIfNotAlreadyThere (props.getValue ("licenceKey"));   // 1.x storage
        for (auto& k : keys) if (k.trim().isNotEmpty()) activate (k, false);
        apply();
    }

    struct Key { juce::String key, product, serial, name; };
    const juce::Array<Key>& keys() const   { return active; }

    bool isLicensed() const             { return owned.core; }            // owns the full program
    bool isDemo() const                 { return ! owned.core; }
    juce::String getLicensee() const    { return licensedTo.isNotEmpty() ? licensedTo : Edition::editionName(); }
    bool canSaveAndBounce() const       { return true; }                  // the Demo saves and bounces; its limits are what it can use

    juce::String statusText() const
    {
        if (licensedTo.isNotEmpty()) return Edition::editionName() + " - licensed to " + licensedTo;
        return Edition::editionName();
    }

    /** Returns an empty string on success or an error message. A valid key is remembered (keys stack). */
    juce::String activate (const juce::String& keyIn, bool persist = true)
    {
        // keys are long and often get wrapped by email apps: ignore every space and line break
        auto key = keyIn.removeCharacters (" \t\r\n");
        if (key.isEmpty()) return "No key entered";
        Key k; k.key = key;
        if (key.startsWith ("SQM2-"))
        {
            if (! verifyStoreKey (key, k.product, k.serial)) return "That key is not valid for Squelchmoth. Check for missing characters and try again.";
            if (revokedSerials().contains (k.serial)) return "That key has been switched off (refunded or shared). Contact support if you think this is a mistake.";
        }
        else
        {
            juce::String name, email;
            if (! verify (key, name, email)) return "That key is not valid for Squelchmoth. Check for missing characters and try again.";
            k.product = "COMPLETE"; k.name = name.isNotEmpty() ? name : email;
        }
        for (auto& a : active) if (a.key == key) return {};
        active.add (k);
        if (k.name.isNotEmpty()) licensedTo = k.name;
        apply();
        if (persist)
        {
            juce::StringArray all; for (auto& a : active) all.add (a.key);
            props.setValue ("licenceKeys", all.joinIntoString ("\n"));
            props.removeValue ("licenceKey");
            props.saveIfNeeded();
        }
        return {};
    }
    /** Forgets every key (Help > Licences > Remove all, and the self-test). */
    void clear (bool persist = true)
    {
        active.clear(); licensedTo.clear(); apply();
        if (persist) { props.removeValue ("licenceKeys"); props.removeValue ("licenceKey"); props.saveIfNeeded(); }
    }

    //==========================================================================
    static bool verifyStoreKey (const juce::String& key, juce::String& productOut, juce::String& serialOut, const juce::String& pubHex = storePublicKey)
    {
        if (! key.startsWith ("SQM2-")) return false;
        juce::MemoryOutputStream decoded;
        auto b64 = key.substring (5).replaceCharacters ("-_", "+/");
        while (b64.length() % 4 != 0) b64 << "=";
        if (! juce::Base64::convertFromBase64 (decoded, b64)) return false;
        if (decoded.getDataSize() != 69) return false;
        auto* d = static_cast<const uint8_t*> (decoded.getData());
        juce::MemoryBlock pk; pk.loadFromHexString (pubHex); if (pk.getSize() != 32) return false;
        uint8_t msg[17]; std::memcpy (msg, "squelchmoth2", 12); std::memcpy (msg + 12, d, 5);
        if (! Ed25519::verify (d + 5, msg, sizeof (msg), static_cast<const uint8_t*> (pk.getData()))) return false;
        static const char* codes[] = { "", "CORE", "COMPLETE", "VOCAL", "MASTER", "INSTR", "BEAT", "CREATIVE", "COLOR" };
        if (d[0] < 1 || d[0] > 8) return false;
        productOut = codes[d[0]];
        serialOut = juce::String::toHexString (d + 1, 4, 0).toUpperCase();
        return true;
    }

    static bool verify (const juce::String& key, juce::String& nameOut, juce::String& emailOut, const juce::String& pubKeyString = publicKey)
    {
        if (! key.startsWith ("SQM1-")) return false;
        juce::MemoryOutputStream decoded;
        if (! juce::Base64::convertFromBase64 (decoded, key.substring (5).replaceCharacters ("-_", "+/"))) return false;
        auto payload = decoded.toString();
        auto parts = juce::StringArray::fromTokens (payload, "|", "");
        if (parts.size() != 4) return false;

        juce::RSAKey pub (pubKeyString);
        juce::BigInteger sig;
        sig.parseString (parts[3], 16);
        pub.applyToValue (sig);

        auto expected = juce::SHA256 (("squelchmoth|" + parts[0] + "|" + parts[1] + "|" + parts[2]).toUTF8()).toHexString();
        juce::BigInteger expectedInt;
        expectedInt.parseString (expected, 16);

        if (sig != expectedInt) return false;
        nameOut = parts[0];
        emailOut = parts[1];
        return true;
    }

    /** Owner-only: makes an SQM1 key with the owner's private RSA key. Used by `Squelchmoth --make-licence`. */
    static juce::String makeKey (const juce::String& privateKey, const juce::String& name, const juce::String& email)
    {
        auto issued = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
        auto hash = juce::SHA256 (("squelchmoth|" + name + "|" + email + "|" + issued).toUTF8()).toHexString();
        juce::BigInteger h;
        h.parseString (hash, 16);
        juce::RSAKey priv (privateKey);
        priv.applyToValue (h);
        auto payload = name + "|" + email + "|" + issued + "|" + h.toString (16);
        return "SQM1-" + juce::Base64::toBase64 (payload.toRawUTF8(), payload.getNumBytesAsUTF8()).replaceCharacters ("+/", "-_");
    }

    static juce::File markerFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (".squelchmoth-first-run");
    }

private:
    juce::PropertiesFile& props;
    juce::Array<Key> active;
    juce::String licensedTo;
    Edition::Owned owned;

    void apply()
    {
        Edition::Owned o;
        for (auto& k : active)
        {
            const auto& p = k.product;
            if (p == "CORE") o.core = true;
            else if (p == "COMPLETE") o.core = o.vocal = o.mastering = o.instruments = o.beat = o.creative = o.color = true;
            else if (p == "VOCAL") o.vocal = true; else if (p == "MASTER") o.mastering = true; else if (p == "INSTR") o.instruments = true;
            else if (p == "BEAT") o.beat = true; else if (p == "CREATIVE") o.creative = true; else if (p == "COLOR") o.color = true;
        }
        owned = o; Edition::owned() = o;
    }
};
