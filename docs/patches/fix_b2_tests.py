# Updates two Core B2 tests that reused ONE observer object to fill all slots. attach() now refuses
# a double registration, so those tests need distinct observers. Safe to re-run: prints SKIP if already done.
p = 'test/test_coreb2/test_main.cpp'
s = open(p).read()
pairs = [
('''  LogIndex idx;
  for (size_t i = 0; i < MAX_OBSERVERS; i++) TEST_ASSERT_TRUE(l.attach(&idx));
  TEST_ASSERT_FALSE(l.attach(&idx));''',
'''  LogIndex idx[MAX_OBSERVERS], extra;
  for (size_t i = 0; i < MAX_OBSERVERS; i++) TEST_ASSERT_TRUE(l.attach(&idx[i]));
  TEST_ASSERT_FALSE(l.attach(&extra));          // slots full
  TEST_ASSERT_FALSE(l.attach(&idx[0]));         // already registered'''),
('''  LogIndex filler;
  for (size_t i = 0; i < MAX_OBSERVERS; i++) l.attach(&filler);''',
'''  LogIndex filler[MAX_OBSERVERS];
  for (size_t i = 0; i < MAX_OBSERVERS; i++) l.attach(&filler[i]);'''),
]
for a, b in pairs:
    if s.count(a) == 1:
        s = s.replace(a, b); print('patched')
    else:
        print('SKIP (not found or already patched):', a.split('\n')[0].strip())
open(p, 'w').write(s)
