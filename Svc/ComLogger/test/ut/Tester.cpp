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
#include <Os/ValidateFile.hpp>
#include <Os/FileSystem.hpp>
#include <Fw/Types/SerialBuffer.hpp>
#include <stdio.h>
#include <string>

#define ID_BASE 256

#define INSTANCE 0
#define MAX_HISTORY_SIZE 1000
#define QUEUE_DEPTH 10
#define FILE_PREFIX "build_test_comlogger_file"
#define MAX_ENTRIES_PER_FILE 5
#define COM_BUFFER_LENGTH 4
#define STORE_BUFFER_LENGTH_TRUE true
#define EXTRA_DATA_SIZE sizeof(U16) // size of storeLength token or frame key token
#define MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED (MAX_ENTRIES_PER_FILE*(EXTRA_DATA_SIZE+COM_BUFFER_LENGTH))
#define MAX_FILE_SIZE_WITH_STORE_LEN_DISABLED (MAX_ENTRIES_PER_FILE*(COM_BUFFER_LENGTH))

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
    createTestFileName(U8* expectedFileName)
    {
      U8 expectedSuffix[ComLogger::MAX_SUFFIX_LENGTH]; 
      component.getFileSuffix(expectedSuffix);
      
      U32 bytesCopied;
      bytesCopied = snprintf(
        (char*) expectedFileName, 
        ComLogger::MAX_FILENAME_LENGTH, 
        "%s%s", 
        FILE_PREFIX, 
        (char*) expectedSuffix);
      ASSERT_LT( bytesCopied, ComLogger::MAX_FILENAME_LENGTH );
    }

  void Tester ::
    testComIn(
      bool storeBufferLength, 
      bool storeFrameKey, 
      U32 arbitraryUSecond)
    {

      U32 maxSize = 0;
      if (storeBufferLength && !storeFrameKey) {
        maxSize = MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED;
      } else if (!storeBufferLength && !storeFrameKey) {
        maxSize = MAX_FILE_SIZE_WITH_STORE_LEN_DISABLED;
      }

      component.setup(
        FILE_PREFIX, 
        maxSize, 
        storeBufferLength,
        storeFrameKey, false);
      
      U8 data[COM_BUFFER_LENGTH] = {0xde,0xad,0xbe,0xef};
      
      Fw::ComBuffer buffer(&data[0], sizeof(data));
      Fw::Time testTime(TB_NONE, 0, arbitraryUSecond);
      
      // Set the test time to the current time:
      setTestTime(testTime);
      
      // Create expected filename:      
      U8 expectedFileName[ComLogger::MAX_FILENAME_LENGTH];
      createTestFileName(expectedFileName);

      // Before sending any data:
      // Initial fileMode must be CLOSED
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);

      // Initial logMode must be STARTED
      ASSERT_TRUE(component.logMode == ComLogger::STARTED);

      // Make sure there is no event
      ASSERT_EVENTS_SIZE(0);

      // Write to file until it reaches its maximum allowable size:
      for(int i = 0; i < MAX_ENTRIES_PER_FILE; i++)
      {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        // Since the logMode is STARTED a new file should have been created
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
        // Make sure file name is what is expected
        ASSERT_TRUE(strcmp((char*) component.fileName, (char*) expectedFileName) == 0 );
      }

      // OK a new file should be opened after this final invoke, 
      // set a new test time so that a file with a new name gets opened:
      Fw::Time testTimeNext(TB_NONE, 1, arbitraryUSecond);
      setTestTime(testTimeNext);
      invoke_to_comIn(0, buffer, 0);
      dispatchAll();

      // Create next expected filename:      
      U8 nextExpectedFileName[ComLogger::MAX_FILENAME_LENGTH];
      createTestFileName(nextExpectedFileName);
      
      // The new file should be in open mode
      ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      
      // Make sure file name is what is expected
      ASSERT_TRUE(strcmp((char*) component.fileName, (char*) nextExpectedFileName) == 0 );
      
      // Make sure we got a closed file event for the previous file:
      ASSERT_EVENTS_SIZE(1);
      ASSERT_EVENTS_FileClosed_SIZE(1);
      ASSERT_EVENTS_FileClosed(0, (char*) expectedFileName);

      // Make sure the file size is smaller or equal to the limit:
      Os::FileSystem::Status fsStat;
      U64 fileSize = 0;
      // Get the size of the file (in bytes) at location path
      fsStat = Os::FileSystem::getFileSize((char*) expectedFileName, fileSize);
      // Make sure file size is smaller than maximum size
      ASSERT_EQ(fsStat, Os::FileSystem::OP_OK);
      ASSERT_LE(fileSize, maxSize);

      Os::File::Status ret;
      Os::File file;
      // Open file:
      ret = file.open((char*) expectedFileName, Os::File::OPEN_READ);
      ASSERT_EQ(Os::File::OP_OK, ret);

      // Storage for reading file data 
      U8 buf[1024];
      NATIVE_INT_TYPE expectedByteSize = 0;
      if (storeBufferLength && !storeFrameKey) {
        expectedByteSize = (NATIVE_INT_TYPE) sizeof(U16);
      } else if (!storeBufferLength && !storeFrameKey){
        expectedByteSize = COM_BUFFER_LENGTH;
      } else {
        // TBD: add other cases
      }

      // Check data:
      for(int i = 0; i < 5; i++)
      {
        // Get length of buffer to read
        NATIVE_INT_TYPE sizeOfReadBlock = expectedByteSize;
        if (storeBufferLength) {
          // Read first chunk of file which contains the length of the buffer
          ret = file.read(&buf, sizeOfReadBlock);
          ASSERT_EQ(Os::File::OP_OK, ret);
          ASSERT_EQ(sizeOfReadBlock, expectedByteSize);
          // Deserialize and make sure we have storred the correct buffer length value
          // in the file
          Fw::SerialBuffer comBuffLength(buf, sizeOfReadBlock);
          comBuffLength.fill();
          U16 bufferSize = 0;
          Fw::SerializeStatus stat = comBuffLength.deserialize(bufferSize);
          ASSERT_EQ(stat, Fw::FW_SERIALIZE_OK);

          ASSERT_EQ((U16) COM_BUFFER_LENGTH, bufferSize);
          // Read the rest of the buffer and check for correcness
          sizeOfReadBlock = bufferSize;
          ret = file.read(&buf, sizeOfReadBlock);
          ASSERT_EQ(Os::File::OP_OK,ret);
          ASSERT_EQ(sizeOfReadBlock, (NATIVE_INT_TYPE) bufferSize);
          ASSERT_EQ(memcmp(buf, data, COM_BUFFER_LENGTH), 0);
        } else if (!storeBufferLength && !storeFrameKey) {
          ret = file.read(&buf, sizeOfReadBlock);
          ASSERT_EQ(Os::File::OP_OK,ret);
          ASSERT_EQ(sizeOfReadBlock, (NATIVE_INT_TYPE) COM_BUFFER_LENGTH);
          ASSERT_EQ(memcmp(buf, data, COM_BUFFER_LENGTH), 0);
        }
      }

      // Make sure we reached the end of the file:
      NATIVE_INT_TYPE sizeOfLastBlock = expectedByteSize;
      ret = file.read(&buf, sizeOfLastBlock );
      ASSERT_EQ(Os::File::OP_OK,ret);
      ASSERT_EQ(sizeOfLastBlock, 0);
      file.close();
    }


  void Tester ::
    openError(void) 
  {
      component.setup(FILE_PREFIX, MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED, true, false, false);
      
      // Construct illegal filePrefix, and set it via the friend:
      U8 filePrefix[2048];
      U8 fileName[2048];
      memset(fileName, 0, sizeof(fileName));
      memset(filePrefix, 0, sizeof(filePrefix));
      snprintf((char*) filePrefix, sizeof(filePrefix), "illegal/fname?;\\*");

      strncpy((char*) component.filePrefix, (char*) filePrefix, sizeof(component.filePrefix));
      
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
      ASSERT_EVENTS_SIZE(0);

      const U8 data[COM_BUFFER_LENGTH] = {0xde,0xad,0xbe,0xef};
      Fw::ComBuffer buffer(data, sizeof(data));
      
      Fw::Time testTime(TB_NONE, 4, 9876543);
      setTestTime(testTime);

      snprintf((char*) fileName, sizeof(fileName), "%s_%d_%d_%06d.com", filePrefix, (U32) testTime.getTimeBase(), testTime.getSeconds(), testTime.getUSeconds());

      for(int i = 0; i < 3; i++)
      {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
      }

      // Check generated events:
      // We only expect 1 because of the throttle
      ASSERT_EVENTS_SIZE(1);
      ASSERT_EVENTS_FileOpenError_SIZE(1);
      ASSERT_EVENTS_FileOpenError(
        0,
        Os::File::DOESNT_EXIST,
        (char*) fileName
      );

      // Try again with valid file name:
      memset(fileName, 0, sizeof(fileName));
      memset(filePrefix, 0, sizeof(filePrefix));
      snprintf((char*) filePrefix, sizeof(filePrefix), "good_");

      strncpy((char*) component.filePrefix, (char*) filePrefix, sizeof(component.filePrefix));
      
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);

      snprintf((char*) fileName, sizeof(fileName), "%s_%d_%d_%06d.com", filePrefix, (U32) testTime.getTimeBase(), testTime.getSeconds(), testTime.getUSeconds());

      for(int i = 0; i < 3; i++)
      {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      }

      // Check generated events:
      // We only expect 1 because of the throttle
      ASSERT_EVENTS_SIZE(1);
      ASSERT_EVENTS_FileOpenError_SIZE(1);
      component.closeFile();

      // Try again with invalid file name:
      memset(fileName, 0, sizeof(fileName));
      memset(filePrefix, 0, sizeof(filePrefix));
      snprintf((char*) filePrefix, sizeof(filePrefix), "illegal/fname?;\\*");

      strncpy((char*) component.filePrefix, (char*) filePrefix, sizeof(component.filePrefix));
      
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);

      snprintf((char*) fileName, sizeof(fileName), "%s_%d_%d_%06d.com", filePrefix, (U32) testTime.getTimeBase(), testTime.getSeconds(), testTime.getUSeconds());

      for(int i = 0; i < 3; i++)
      {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
      }

      // Check generated events:
      // We only expect 1 extra because of the throttle
      ASSERT_EVENTS_SIZE(3); // extra event here because of the file close
      ASSERT_EVENTS_FileOpenError_SIZE(2);
  }

void Tester ::
    writeError(void) 
  {
      component.setup(FILE_PREFIX, MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED, true, false, false);

      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
      ASSERT_EVENTS_SIZE(0);

      const U8 data[4] = {0xde,0xad,0xbe,0xef};
      Fw::ComBuffer buffer(data, sizeof(data));
      
      Fw::Time testTime(TB_NONE, 5, 9876543);
      setTestTime(testTime);

      for(int i = 0; i < 3; i++)
      {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      }

      // Force close the file from underneath the component:
      component.file.close();

      // Send 2 packets:
      for(int i = 0; i < 3; i++) {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      }

      // Construct filename:
      U8 fileName[2048];
      memset(fileName, 0, sizeof(fileName));
      snprintf((char*) fileName, sizeof(fileName), "%s_%d_%d_%06d.com", FILE_PREFIX, (U32) testTime.getTimeBase(), testTime.getSeconds(), testTime.getUSeconds());  

      // Check generated events:
      // We should only see a single event because write
      // errors are throttled.
      ASSERT_EVENTS_SIZE(1);
      ASSERT_EVENTS_FileWriteError_SIZE(1);
      ASSERT_EVENTS_FileWriteError(
          0,
          Os::File::NOT_OPENED,
          0,
          2,
          (char*) fileName
      );

      // Make comlogger open a new file:
      component.fileMode = ComLogger::CLOSED;
      component.openFile();

      // Try to write and make sure it succeeds:
      // Send 2 packets:
      for(int i = 0; i < 3; i++) {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      }

      // Expect no new errors:
      ASSERT_EVENTS_SIZE(1);
      ASSERT_EVENTS_FileWriteError_SIZE(1);

      // Force close the file from underneath the component:
      component.file.close();

      // Send 2 packets:
      for(int i = 0; i < 3; i++) {
        invoke_to_comIn(0, buffer, 0);
        dispatchAll();
        ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
      }

      // Check generated events:
      // We should only see a single event because write
      // errors are throttled.
      ASSERT_EVENTS_SIZE(2);
      ASSERT_EVENTS_FileWriteError_SIZE(2);
      ASSERT_EVENTS_FileWriteError(
          1,
          Os::File::NOT_OPENED,
          0,
          2,
          (char*) fileName
      );
  }

  void Tester ::
    closeFileCommand(void) 
  {
    component.setup(FILE_PREFIX, MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED, true, false, false);
    Os::File file;
    U8 fileName[2048];
    Os::File::Status ret;

    // Form filenames:
    Fw::Time testTime(TB_NONE, 6, 9876543);
    setTestTime(testTime);
    memset(fileName, 0, sizeof(fileName));
    snprintf((char*) fileName, sizeof(fileName), "%s_%d_%d_%06d.com", FILE_PREFIX, testTime.getTimeBase(), testTime.getSeconds(), testTime.getUSeconds());

    ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
    ASSERT_EVENTS_SIZE(0);

    // Send close file commands:
    for(int i = 0; i < 3; i++) {
      sendCmd_CloseFile(0, i+1);
      dispatchAll();
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
    }

    ASSERT_CMD_RESPONSE_SIZE(3);

    for(int i = 0; i < 3; i++) {
      ASSERT_CMD_RESPONSE(
          i,
          ComLogger::OPCODE_CLOSEFILE,
          i+1,
          Fw::COMMAND_OK
      );
    }
    
    const U8 data[COM_BUFFER_LENGTH] = {0xde,0xad,0xbe,0xef};
    Fw::ComBuffer buffer(data, sizeof(data));

    for(int i = 0; i < 3; i++)
    {
      invoke_to_comIn(0, buffer, 0);
      dispatchAll();
      ASSERT_TRUE(component.fileMode == ComLogger::OPEN);
    }

     // Send close file commands:
    for(int i = 0; i < 3; i++) {
      sendCmd_CloseFile(0, i+1);
      dispatchAll();
      ASSERT_TRUE(component.fileMode == ComLogger::CLOSED);
    }

    ASSERT_CMD_RESPONSE_SIZE(6);

    for(int i = 0; i < 3; i++) {
      ASSERT_CMD_RESPONSE(
          i,
          ComLogger::OPCODE_CLOSEFILE,
          i+1,
          Fw::COMMAND_OK
      );
    }

    // Make sure we got a closed file event:
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_FileClosed_SIZE(1);
    ASSERT_EVENTS_FileClosed(0, (char*) fileName);

    // Open files to make sure they exist:
    ret = file.open((char*) fileName, Os::File::OPEN_READ);
    ASSERT_EQ(Os::File::OP_OK,ret);
    file.close();
  }

  void Tester ::
    test_getFileSuffix(void) 
  {
    Fw::Time testTime(TB_NONE, 4, 9876543);
    setTestTime(testTime);
    
    U8 suffix[ComLogger::MAX_SUFFIX_LENGTH];
    component.setup(FILE_PREFIX, MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED, true, false, false);
    this->component.getFileSuffix(suffix);
    ASSERT_STREQ("_0_4_9876543.com", (char *)suffix);
  }

  void Tester ::
    test_openFile(void)
    {
      Fw::Time testTime(TB_NONE, 4, 9876543);
      setTestTime(testTime);
      component.setup(FILE_PREFIX, MAX_FILE_SIZE_WITH_STORE_LEN_ENABLED, true, false, false);
      this->component.openFile();
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

  // void Tester ::
  //   createFileNames(){

  //   }
} // end namespace Svc
