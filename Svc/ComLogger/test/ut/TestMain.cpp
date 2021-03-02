// ----------------------------------------------------------------------
// TestMain.cpp
// ----------------------------------------------------------------------

#include "Tester.hpp"

TEST(Test, testLogging_BufferStore_Enabled) {
  Svc::Tester tester;
  tester.testComIn(true, false, 876549);
}

// TEST(Test, testLogging_BufferStore_Diabled) {
//   Svc::Tester tester;
//   tester.testComIn(false, false, 187654);
// }

// TEST(Test, openError) {
//   Svc::Tester tester;
//   tester.openError();
// }

// TEST(Test, writeError) {
//   Svc::Tester tester;
//   tester.writeError();
// }

// TEST(Test, closeFileCommand) {
//   Svc::Tester tester;
//   tester.closeFileCommand();
// }

// TEST(Nominal, test_openFile) {
//   Svc::Tester tester;
//   tester.test_openFile();
// }

// TEST(Nominal, test_getFileSuffix) {
//   Svc::Tester tester;
//   tester.test_getFileSuffix();
// }



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
