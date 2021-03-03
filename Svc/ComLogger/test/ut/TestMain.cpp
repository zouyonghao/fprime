// ----------------------------------------------------------------------
// TestMain.cpp
// ----------------------------------------------------------------------

#include "Tester.hpp"


TEST(Test, test_memberVariableInitiation) {
  Svc::Tester tester;
  tester.test_memberVariableInitiation();
}

TEST(Test, test_setupFunction) {
  Svc::Tester tester;
  tester.test_setupFunction();
}

// ----------------------------------------------------------------------
// Tests of Helper functions:
// ---------------------------------------------------------------------- 

TEST(Test, test_setFilePrefix) {
  Svc::Tester tester;
  tester.test_setFilePrefix();
}

TEST(Test, test_getFileSuffix_no_resetOnMaxSize) {
  Svc::Tester tester;
  tester.test_getFileSuffix(false);
}

TEST(Test, test_getFileSuffix_with_resetOnMaxSize) {
  Svc::Tester tester;
  tester.test_getFileSuffix(true);
}

TEST(Test, test_openFile_no_issue_case_no_resetOnMax) {
  Svc::Tester tester;
  tester.test_openFile_no_issue_case(false);
}

TEST(Test, test_openFile_no_issue_case_resetOnMax) {
  Svc::Tester tester;
  tester.test_openFile_no_issue_case(true);
}

TEST(Test, test_openFile_no_resetOnMax_fileName) {
  Svc::Tester tester;
  tester.test_openFile_checking_fileName(false);
}

TEST(Test, test_openFile_resetOnMax_fileName) {
  Svc::Tester tester;
  tester.test_openFile_checking_fileName(true);
}

TEST(Test, test_openFile_with_issue_case_no_resetOnMax) {
  Svc::Tester tester;
  tester.test_openFile_with_issue_case();
}

TEST(Test, test_closeFile) {
  Svc::Tester tester;
  tester.test_closeFile();
}

TEST(Test, test_restartFile) {
  Svc::Tester tester;
  tester.test_restartFile();
}

TEST(Test, test_writeToFile_with_issue) {
  Svc::Tester tester;
  tester.test_writeToFile_with_issue();
}

TEST(Test, test_writeComBufferToFile) {
  Svc::Tester tester;
  tester.test_writeComBufferToFile();
}

TEST(Test, test_startLogging_cmd) {
  Svc::Tester tester;
  tester.test_startLogging_cmd();
}

TEST(Test, test_stopLogging_cmd) {
  Svc::Tester tester;
  tester.test_stopLogging_cmd();
}

TEST(Test, test_closeFile_cmd) {
  Svc::Tester tester;
  tester.test_closeFile_cmd();
}

TEST(Test, test_pingIn_cmd) {
  Svc::Tester tester;
  tester.test_pingIn_cmd();
}

TEST(Test, test_setRecordName_cmd) {
  Svc::Tester tester;
  tester.test_setRecordName_cmd();
}

TEST(Test, test_comIn_cmd_with_no_resetOnMax) {
  Svc::Tester tester;
  tester.test_comIn_cmd(false, false);
}

TEST(Test, test_comIn_cmd_with_resetOnMax) {
  Svc::Tester tester;
  tester.test_comIn_cmd(true, false);
}

TEST(Test, test_comIn_cmd_with_recordName) {
  Svc::Tester tester;
  tester.test_comIn_cmd(false, true);
}

TEST(Test, test_comIn_cmd_with_reset_and_recordName) {
  Svc::Tester tester;
  tester.test_comIn_cmd(true, true);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
