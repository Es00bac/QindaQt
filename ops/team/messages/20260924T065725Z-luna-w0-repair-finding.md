# W0 repair material finding

Qt 6.11.1 on qinda reports zero bytes for `QString::toUtf8()` on isolated high and low UTF-16 surrogates. The requested admission behavior says to count a three-byte U+FFFD replacement, so `utf8ByteCount()` keeps that conservative rule; its tests compare valid UTF-16 strings to `QString::toUtf8().size()` and separately assert three bytes for each lone surrogate. The first test run exposed this mismatch, and I am rerunning after making the rule explicit.
