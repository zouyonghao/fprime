// ====================================================================== 
// \title  ComLogger.hpp
// \author janamian
// \brief  cpp file for ComLogger test harness implementation class
//
// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// 
// ====================================================================== 

#include "Tester.hpp"
#include "Fw/Cmd/CmdPacket.hpp"
#include <Os/FileSystem.hpp>
#include <Fw/Types/SerialBuffer.hpp>
#include <Svc/ComLogger/ComLoggerCfg.hpp>
#include <stdio.h>

#define ID_BASE 256

#define INSTANCE 0
#define MAX_HISTORY_SIZE 1000
#define QUEUE_DEPTH 100
#define FILE_PREFIX "build_test_comlogger_file"
#define ARBITRARY_USECOND 9876543
#define MAX_ENTRIES_PER_FILE 5
#define COM_BUFFER_LENGTH 4
#define META_DATA_SIZE 2*sizeof(U16) // size of storeLength token and frame key token
#define MAX_FILE_SIZE (MAX_ENTRIES_PER_FILE*(META_DATA_SIZE+COM_BUFFER_LENGTH))

namespace Svc {

  // ----------------------------------------------------------------------
  // Construction and destruction 
  // ----------------------------------------------------------------------

  Tester ::
    Tester(void) : 
#if FW_OBJECT_NAMES == 1
      ComLoggerGTestBase("Tester", MAX_HISTORY_SIZE),
      component("ComLogger")
#else
      ComLoggerGTestBase(MAX_HISTORY_SIZE),
      component()
#endif
  {
    this->initComponents();
    this->connectPorts();
  }

  Tester ::
    ~Tester(void) 
  {
    
  }

  // ----------------------------------------------------------------------
  // Tests 
  // ----------------------------------------------------------------------
  void Tester ::
    test_memberVariableInitiation(void)
    {
      ASSERT_EQ(component.maxFileSize, 0);
      ASSERT_EQ(component.byteCount, 0);
      ASSERT_EQ(component.logMode, ComLogger::STARTED);
      ASSERT_EQ(component.fileMode, ComLogger::CLOSED); 
      ASSERT_EQ(component.writeErrorOccurred, false);
      ASSERT_EQ(component.openErrorOccurred,false);
      ASSERT_EQ(component.fileInfoSet,false);
      ASSERT_EQ(component.resetOnMaxSize,false);
      ASSERT_STREQ(component.fileInfix.toChar(), "");
      ASSERT_STREQ(component.fileSuffix.toChar(), "");
    }

  void Tester ::
    test_setupFunction(void)
    {
      setUpFixture();

      ASSERT_STREQ((char *)component.filePrefix, FILE_PREFIX);
      ASSERT_EQ(component.maxFileSize, MAX_FILE_SIZE);
      ASSERT_FALSE(component.resetOnMaxSize);
      ASSERT_TRUE(component.fileInfoSet);
    }

  void Tester ::
    test_setFilePrefix(void)
    {
      setUpFixture();
      component.setFilePrefix(FILE_PREFIX);
      ASSERT_STREQ((char *)component.filePrefix, FILE_PREFIX);
    }

  void Tester ::
    test_getFileSuffix(bool resetOnMaxSize)
    {
      setUpFixture();
      component.resetOnMaxSize = resetOnMaxSize;
      component.setFileSuffix();

      this->setExpectedFileSuffix(resetOnMaxSize);

      ASSERT_STREQ(
        component.fileSuffix.toChar(), 
        this->expectedFileSuffix.toChar());
    }

  void Tester ::
    test_openFile_no_issue_case(bool resetOnMaxSize)
    {
      setUpFixture();
      component.resetOnMaxSize = resetOnMaxSize;
      component.openFile();
      ASSERT_EQ(component.byteCount, 0);
      ASSERT_EQ(component.byteCount, 0);
      ASSERT_FALSE(component.openErrorOccurred);
      ASSERT_EQ(component.fileMode, ComLogger::OPEN);
    }

  void Tester ::
    test_openFile_checking_fileName(bool resetOnMaxSize)
    {
      setUpFixture();
      component.resetOnMaxSize = resetOnMaxSize;
      component.openFile();
      
      this->setExpectedFileSuffix(resetOnMaxSize);
      this->setExpectedFileName();
      
      ASSERT_STREQ(
        (char*)component.fileName,
        (char*)this->expectedFileName
        );
    }

  void Tester ::
    test_openFile_with_issue_case()
    {
      setUpFixture();
      component.resetOnMaxSize = false;
      component.setFilePrefix("/path/to/a/NoneExistingFile"); // Create a not existing path to cause fileopen error
      component.openFile();
      ASSERT_TRUE(component.openErrorOccurred);
      
      // Check generated events:
      // We only expect 1 because of the throttle
      ASSERT_EVENTS_SIZE(1);
      ASSERT_EVENTS_FileOpenError_SIZE(1);
      ASSERT_EVENTS_FileOpenError(
        0,
        Os::File::DOESNT_EXIST,
        (char*) component.fileName
      );

      // Check also for resetOnMaxSize
      component.resetOnMaxSize = true;
      component.openFile();
      // There won't be any new warning because of the throttle
      ASSERT_EVENTS_SIZE(1);
      ASSERT_TRUE(component.openErrorOccurred);
      ASSERT_EVENTS_FileOpenError_SIZE(1);
    }

  void Tester ::
    test_closeFile(void)
    {
      setUpFixture();
      component.openFile();
      component.closeFile();
      
      ASSERT_EQ(component.fileMode, ComLogger::CLOSED);
      // Make sure we get FileClosed event
      ASSERT_EVENTS_FileClosed_SIZE(1);
    }

  void Tester ::
    test_restartFile(void)
    {
      setUpFixture();
      component.openFile();
      component.restartFile();
      
      ASSERT_EQ(component.byteCount, 0);
      ASSERT_EVENTS_FileRestarted_SIZE(1);
    }

  void Tester ::
    test_writeToFile_with_issue(void)
    {
      setUpFixture();
      component.openFile();
      
      // Force to close file to create issue
      component.closeFile();
      U8 data[COM_BUFFER_LENGTH] = {0xde,0xad,0xbe,0xef};
      Fw::ComBuffer buffer(data, sizeof(data));
      component.writeToFile(data, sizeof(data));
      
      // Make sure we got one FileWriteError event
      ASSERT_EVENTS_FileWriteError_SIZE(1);
      ASSERT_EVENTS_FileWriteError(
          0,
          Os::File::NOT_OPENED,
          0,
          4,
          (char*) component.fileName
      );
    }

  void Tester ::
    test_writeComBufferToFile(void)
    {
      setUpFixture();
      component.openFile();
      U8 data[COM_BUFFER_LENGTH] = {0xde,0xad,0xbe,0xef};
      Fw::ComBuffer buffer(data, sizeof(data));
      component.writeComBufferToFile(buffer, COM_BUFFER_LENGTH);

      // Make sure the file size is smaller or equal to the limit:
      Os::FileSystem::Status fsStat;
      U64 fileSize = 0;
      
      // Get the size of the file (in bytes) at location path
      fsStat = Os::FileSystem::getFileSize((char*) component.fileName, fileSize);
      
      // Make sure file size is smaller than maximum size
      ASSERT_EQ(fsStat, Os::FileSystem::OP_OK);
      ASSERT_LE(fileSize, MAX_FILE_SIZE);
    }

  void Tester ::
    test_startLogging_cmd(void)
    {
      sendCmd_StartLogging(0, 0);
      component.doDispatch();
      ASSERT_EQ(component.logMode, ComLogger::STARTED);
      ASSERT_CMD_RESPONSE(
        0,
        ComLogger::OPCODE_STARTLOGGING,
        0,
        Fw::COMMAND_OK
      );
    }

  void Tester ::
    test_stopLogging_cmd(void)
    {
      setUpFixture();
      component.openFile();
      sendCmd_StopLogging(0, 0);
      component.doDispatch();
      ASSERT_EQ(component.logMode, ComLogger::STOPPED);
      ASSERT_EVENTS_FileClosed_SIZE(1);
      ASSERT_CMD_RESPONSE(
        0,
        ComLogger::OPCODE_STOPLOGGING,
        0,
        Fw::COMMAND_OK
      );
    }

  void Tester ::
    test_closeFile_cmd(void)
    {
      setUpFixture();
      component.openFile();
      sendCmd_CloseFile(0, 0);
      component.doDispatch();
      ASSERT_CMD_RESPONSE(
        0,
        ComLogger::OPCODE_CLOSEFILE,
        0,
        Fw::COMMAND_OK
      );
    }

  void Tester ::
    test_pingIn_cmd(void)
  {
    // invoke ping port
    invoke_to_pingIn(0,0x123);
    // dispatch message
    component.doDispatch();
    // look for return port call
    ASSERT_FROM_PORT_HISTORY_SIZE(1);
    // look for key
    ASSERT_from_pingOut(0,(U32)0x123);
  }

  void Tester ::
    test_setRecordName_cmd()
    {
      setUpFixture();
      Fw::EightyCharString recordName("REC_1");
      sendCmd_SetRecordName(0, 0, recordName);
      component.doDispatch();
    }
  void Tester ::
    test_comIn_cmd( bool resetOnMaxSize, bool addRecordName)
    {
      setUpFixture();
      component.resetOnMaxSize = resetOnMaxSize;
      
      // Make sure there is no event
      ASSERT_EVENTS_SIZE(0);

      if (addRecordName) {
        Fw::EightyCharString recordName("REC_1");
        sendCmd_SetRecordName(0, 0, recordName);
        component.doDispatch();
      }
      // set a test time
      this->testTime.set(TB_NONE, 0, ARBITRARY_USECOND );
      setTestTime(this->testTime);

      // Set some data to be written to the file
      U8 data[COM_BUFFER_LENGTH] = {0xde, 0xad, 0xbe, 0xef};
      Fw::ComBuffer buffer(&data[0], sizeof(data));
     
      // Create expected filename    
      this->setExpectedFileSuffix(resetOnMaxSize);
      this->setExpectedFileName();

      // Before sending any data initial fileMode must be CLOSED
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);

      // Initial logMode must be STARTED
      ASSERT_TRUE(component.logMode == ComLogger::STARTED);

      // Write to file until it reaches its maximum allowable size:
      for(int i = 0; i < MAX_ENTRIES_PER_FILE; i++)
      {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        // Since the logMode is STARTED a new file should have been created
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
        // Make sure file name is what is expected
        ASSERT_STREQ((char*)component.fileName, (char*)this->expectedFileName);
      }

      // OK a new file should be opened after this final invoke, 
      // set a new test time so that a file with a new name gets opened:
      this->testTime.set(TB_NONE, 1, ARBITRARY_USECOND );
      setTestTime(this->testTime);
      invoke_to_comIn(0, buffer, 0);
      dispatchAll();

      // Keep the name of first file
      U8 previousLogFile[ComLogger::MAX_FILENAME_LENGTH];
      snprintf(
        (char*)previousLogFile, 
        sizeof(previousLogFile),
        "%s", 
        this->expectedFileName);
      
      // Update expected filename
      this->setExpectedFileSuffix(resetOnMaxSize);
      this->setExpectedFileName();      
      
      // Make sure file name is what is expected
      ASSERT_STREQ((char*)component.fileName, (char*)this->expectedFileName);

      // The new file should be in open mode
      ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      
      if ( !component.resetOnMaxSize ) {
        // Make sure we got a closed file event for the previous file:
        ASSERT_EVENTS_FileClosed_SIZE(1);
        ASSERT_EVENTS_FileClosed(0, (char*)previousLogFile);
      }

      // Make sure the file size is smaller or equal to the limit:
      Os::FileSystem::Status fsStat;
      U64 fileSize = 0;
      // Get the size of the file (in bytes) at location path
      fsStat = Os::FileSystem::getFileSize((char*)previousLogFile, fileSize);
      // Make sure file size is smaller than maximum size
      ASSERT_EQ(fsStat, Os::FileSystem::OP_OK);
      ASSERT_LE(fileSize, MAX_FILE_SIZE);

      Os::File::Status ret;
      Os::File file;
      // Open file:
      ret = file.open((char*)previousLogFile, Os::File::OPEN_READ);
      ASSERT_EQ(Os::File::OP_OK, ret);

      // Storage for reading file data 
      U8 buf[1024];
      NATIVE_INT_TYPE expectedByteSize = sizeof(U16);

      // Check data:
      for(int i = 0; i < 5; i++)
      {
        // Get length of buffer to read
        NATIVE_INT_TYPE sizeOfReadBlock = expectedByteSize;
        // Read first chunk of file which is frame key
        ret = file.read(&buf, sizeOfReadBlock);
        ASSERT_EQ(Os::File::OP_OK, ret);
        ASSERT_EQ(sizeOfReadBlock, expectedByteSize);

        // Deserialize and make sure we have storred the correct frame key
        Fw::SerialBuffer comBuffLength_frame_key(buf, sizeOfReadBlock);
        comBuffLength_frame_key.fill();
        U16 frameKey = 0;
        Fw::SerializeStatus stat_frame_key = comBuffLength_frame_key.deserialize(frameKey);
        ASSERT_EQ(stat_frame_key, Fw::FW_SERIALIZE_OK);
        const U16 FRAME_KEY = static_cast<U16>(COM_LOGGER_FRAME_START_KEY);
        ASSERT_EQ(FRAME_KEY, frameKey);
      
        // Read the second chunk of file which is frame length
        ret = file.read(&buf, sizeOfReadBlock);
        ASSERT_EQ(Os::File::OP_OK, ret);
        ASSERT_EQ(sizeOfReadBlock, expectedByteSize);

        // Deserialize and make sure we have storred the correct buffer length
        Fw::SerialBuffer comBuffLength(buf, sizeOfReadBlock);
        comBuffLength.fill();
        U16 bufferSize = 0;
        Fw::SerializeStatus stat_buff_size = comBuffLength.deserialize(bufferSize);
        ASSERT_EQ(stat_buff_size, Fw::FW_SERIALIZE_OK);
        ASSERT_EQ((U16) COM_BUFFER_LENGTH, bufferSize);
        
        // Read the rest of the buffer and check for correcness
        sizeOfReadBlock = bufferSize;
        ret = file.read(&buf, sizeOfReadBlock);
        ASSERT_EQ(Os::File::OP_OK,ret);
        ASSERT_EQ(sizeOfReadBlock, (NATIVE_INT_TYPE) bufferSize);
        ASSERT_EQ(memcmp(buf, data, COM_BUFFER_LENGTH), 0);
      }

      // Make sure we reached the end of the file:
      NATIVE_INT_TYPE sizeOfLastBlock = expectedByteSize;
      ret = file.read(&buf, sizeOfLastBlock );
      ASSERT_EQ(Os::File::OP_OK,ret);
      ASSERT_EQ(sizeOfLastBlock, 0);
      file.close();
    }

  // ----------------------------------------------------------------------
  // Handlers for typed from ports
  // ----------------------------------------------------------------------

  void Tester ::
    from_pingOut_handler(
        const NATIVE_INT_TYPE portNum,
        U32 key
    )
  {
    this->pushFromPortEntry_pingOut(key);
  }

  // ----------------------------------------------------------------------
  // Helper methods 
  // ----------------------------------------------------------------------

  void Tester ::
    connectPorts(void) 
  {

    // comIn
    this->connect_to_comIn(
        0,
        this->component.get_comIn_InputPort(0)
    );

    // cmdIn
    this->connect_to_cmdIn(
        0,
        this->component.get_cmdIn_InputPort(0)
    );

    // pingIn
    this->connect_to_pingIn(
        0,
        this->component.get_pingIn_InputPort(0)
    );

    // timeCaller
    this->component.set_timeCaller_OutputPort(
        0, 
        this->get_from_timeCaller(0)
    );

    // cmdRegOut
    this->component.set_cmdRegOut_OutputPort(
        0, 
        this->get_from_cmdRegOut(0)
    );

    // logOut
    this->component.set_logOut_OutputPort(
        0, 
        this->get_from_logOut(0)
    );

    // cmdResponseOut
    this->component.set_cmdResponseOut_OutputPort(
        0, 
        this->get_from_cmdResponseOut(0)
    );

    // pingOut
    this->component.set_pingOut_OutputPort(
        0, 
        this->get_from_pingOut(0)
    );

    // textLogOut
    this->component.set_textLogOut_OutputPort(
        0, 
        this->get_from_textLogOut(0)
    );

  }

  void Tester ::
    initComponents(void) 
  {
    this->init();
    this->component.init(
        QUEUE_DEPTH, INSTANCE
    );
  }

  void Tester ::
    dispatchOne(void)
  {
    this->component.doDispatch();
  }
  
  void Tester ::
    dispatchAll(void)
  {
    while(this->component.m_queue.getNumMsgs() > 0)
      this->dispatchOne();
  }
  
  void Tester ::
    setUpFixture(void)
  {
      component.setup( FILE_PREFIX, MAX_FILE_SIZE, false);
      component.preamble();
      this->testTime.set( TB_NONE, 4, ARBITRARY_USECOND );
      setTestTime(this->testTime);
  }

  void Tester :: 
    setExpectedFileSuffix(bool resetOnMaxSize)
    {
      if (resetOnMaxSize)
      {
        this->expectedFileSuffix = COM_LOGGER_FILE_EXTENTION;
      }
      else
      {
        this->expectedFileSuffix.format(
          "_%d_%d_%06d%s",
          (U32) this->testTime.getTimeBase(), 
          this->testTime.getSeconds(), 
          this->testTime.getUSeconds(),
          COM_LOGGER_FILE_EXTENTION
        );
      }
    }

  void Tester ::
    setExpectedFileName()
    {
      U32 bytesCopied = 0;
      bytesCopied = snprintf(
        (char*) this->expectedFileName, 
        sizeof(this->expectedFileName), 
        "%s%s%s",
        component.filePrefix,
        component.fileInfix.toChar(),
        this->expectedFileSuffix.toChar());

      ASSERT_LT( bytesCopied, ComLogger::MAX_FILENAME_LENGTH );
    }

} // end namespace Svc
