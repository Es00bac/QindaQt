# Literal screenshot URI fixture mismatch

Exacted7a4e163d952e64cbc67dcc358a0bbe00f749fe11-row private-bus gate stopped
at first unexpected policy failure. CTestexit8, only1/11 attempted,0pass/1fail;
Qt4pass/1fail/0skip, elapsed0.095s. Raw build/ed7a4e16-unit-tests.log/statusJSON
preserved. Ownedgroup171914 gone, no survivors/scoped taskcores/observed crash.
Private slot released immediately; no native authority/helper/PipeWire activity.

Primary exact Qt6.11.1 [QUrl formatting](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/corelib/io/qurl.cpp)
and [recoder table](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/corelib/io/qurlrecode.cpp)
show FullyEncoded preserves allowed path sub-delimiters: dollar uses
LeaveCharacter. Space, percent and backtick are encoded. The fixture supplied
%24, whereas the selected literal fromLocalFile path serialization keeps '$'.
Product helper and captureResults already use the same public Qt serialization.

Only the owned test now supplies %20%25$%60, independently asserts public Qt
serialization and exact special-character toLocalFile roundtrip, and rejects
the mismatched %24 form at the exact broker result boundary. Wrong directory,
traversal and remote-host negatives remain unchanged. No production validation
or assertion weakening. Reference clarifies literal URI serialization/no shell.
Next exact fixture compile and same11-row rerun require grants after root's
fork driver compiler release. No other10rows/teardown/native evidence yet.
