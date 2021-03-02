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
      void testComIn(
        bool storeBufferLength, 
        bool storeFrameKey,
        U32 arbitraryUSecond
        );

      void openError(void);
      void writeError(void);
      void closeFileCommand(void);
      
      void test_getFileSuffix(void);
      void test_openFile(void);
      

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
      void createTestFileName(U8* expectedFileName);

    private:

      // ----------------------------------------------------------------------
      // Variables
      // ----------------------------------------------------------------------

      //! The component under test
      //!
      ComLogger component;

  };

} // end namespace Svc

#endif
