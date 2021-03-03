// ======================================================================
// \title  ComLogger/test/ut/Tester.hpp
// \author janamian
// \brief  hpp file for ComLogger test harness implementation class
//
// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef TESTER_HPP
#define TESTER_HPP

#include "GTestBase.hpp"
#include "Svc/ComLogger/ComLogger.hpp"

#define MAX_ENTRIES_PER_FILE 5

namespace Svc {

  class Tester :
    public ComLoggerGTestBase
  {

      // ----------------------------------------------------------------------
      // Construction and destruction
      // ----------------------------------------------------------------------

    public:

      //! Construct object Tester
      //!
      Tester(void);

      //! Destroy object Tester
      //!
      ~Tester(void);

    public:

      // ----------------------------------------------------------------------
      // Tests
      // ----------------------------------------------------------------------
      void test_memberVariableInitiation(void);
      void test_setupFunction(void);
      void test_setFilePrefix(void);
      void test_getFileSuffix(bool resetOnMaxSize);
      void test_openFile_no_issue_case(bool resetOnMaxSize);
      void test_openFile_checking_fileName(bool resetOnMaxSize);
      void test_openFile_with_issue_case(void);
      void test_closeFile(void);
      void test_restartFile(void);
      void test_writeToFile_with_issue(void);
      void test_writeComBufferToFile(void);
      void test_startLogging_cmd(void);
      void test_stopLogging_cmd(void);
      void test_closeFile_cmd(void);
      void test_pingIn_cmd(void);
      void test_setRecordName_cmd(void);
      void test_comIn_cmd(bool resetOnMaxSize, bool addRecordName);

    private:

      // ----------------------------------------------------------------------
      // Handlers for typed from ports
      // ----------------------------------------------------------------------

      //! Handler for from_pingOut
      //!
      void from_pingOut_handler(
          const NATIVE_INT_TYPE portNum, /*!< The port number*/
          U32 key /*!< Value to return to pinger*/
      );

    private:

      // ----------------------------------------------------------------------
      // Helper methods
      // ----------------------------------------------------------------------

      //! Connect ports
      //!
      void connectPorts(void);

      //! Initialize components
      //!
      void initComponents(void);

      void dispatchOne(void);
      void dispatchAll(void);
      
      void setUpFixture(void);
      void setExpectedFileName();
      void setExpectedFileSuffix(bool resetOnMaxSize);

    private:

      // ----------------------------------------------------------------------
      // Variables
      // ----------------------------------------------------------------------

      //! The component under test
      //!
      ComLogger component;
      Fw::EightyCharString expectedFileSuffix;
      Fw::Time testTime;
      U8 expectedFileName[ComLogger::MAX_FILENAME_LENGTH];
  };

} // end namespace Svc

#endif
