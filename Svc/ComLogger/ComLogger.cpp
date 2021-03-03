// ----------------------------------------------------------------------
//
// ComLogger.cpp
//
// ----------------------------------------------------------------------

#include <Svc/ComLogger/ComLogger.hpp>
#include <Svc/ComLogger/ComLoggerCfg.hpp>
#include <Fw/Types/SerialBuffer.hpp>
#include <Fw/Types/StringUtils.hpp>

namespace Svc {

  // ----------------------------------------------------------------------
  // Construction, initialization, and destruction 
  // ----------------------------------------------------------------------

  ComLogger ::
    ComLogger(const char* compName) :
    ComLoggerComponentBase(compName), 
    fileInfix(""),
    fileSuffix(""),
    maxFileSize(0),
    byteCount(0),
    logMode(STARTED),
    fileMode(CLOSED), 
    writeErrorOccurred(false),
    openErrorOccurred(false),
    fileInfoSet(false),
    resetOnMaxSize(false)
  {
  }

  void ComLogger :: 
    init(
      NATIVE_INT_TYPE queueDepth, //!< The queue depth
      NATIVE_INT_TYPE instance //!< The instance number
    )
  {
    ComLoggerComponentBase::init(queueDepth, instance);
  }

  ComLogger ::
    ~ComLogger(void)
  {
    if( OPEN == this->fileMode ) {
      // Close file:
      // NOTE: Did not this->closeFile(); to avoid issuing an event in 
      // the destructor which can cause "virtual method called" segmentation 
      // faults.
      this->file.close(); 

      // Update mode:
      this->fileMode = CLOSED;
    }
  }

  void ComLogger ::
    setup(
      const char* filePrefix,
      U32 maxFileSize,
      bool resetOnMaxSize
    )
    {
      this->maxFileSize = maxFileSize;
      this->resetOnMaxSize = resetOnMaxSize;
      
      // must be a positive value greater than meta data size
      FW_ASSERT(maxFileSize > META_DATA_SIZE, maxFileSize); 
      
      setFilePrefix(filePrefix);
      
      this->fileInfoSet = true;
    }
  
  void ComLogger::preamble(void) {
      FW_ASSERT(this->fileInfoSet);
  }

  // ----------------------------------------------------------------------
  // Handler implementations
  // ----------------------------------------------------------------------

  void ComLogger ::
    comIn_handler(
        NATIVE_INT_TYPE portNum,
        Fw::ComBuffer &data,
        U32 context
    )
  {
    FW_ASSERT(portNum == 0);

    // Get length of buffer:
    U32 size32 = data.getBuffLength();
    
    // ComLogger only writes 16-bit sizes to save space on disk:
    FW_ASSERT(size32 < 65536, size32);
    U16 size = size32 & 0xFFFF;

    // Close or restart the file if it will be too big:
    if( OPEN == this->fileMode ) {
      U32 projectedByteCount = this->byteCount + size + META_DATA_SIZE;

      if( projectedByteCount > this->maxFileSize ) {
        if (this->resetOnMaxSize) {
          this->restartFile();
        }
        else {
          this->closeFile();
        }
      }
    }

    // Open the file if there is not one open:
    if( CLOSED == this->fileMode && STOPPED != this->logMode){
      this->openFile();
    }

    // Write to the file if it is open:
    if( OPEN == this->fileMode ) {
      this->writeComBufferToFile(data, size);
    }
  }

  void ComLogger ::
    pingIn_handler(
        const NATIVE_INT_TYPE portNum,
        U32 key
    )
  {
      // return key
      this->pingOut_out(0,key);
  }

  // ----------------------------------------------------------------------
  // Command handler implementations
  // ----------------------------------------------------------------------

  void ComLogger :: 
    StartLogging_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq
    )
  {
    this->logMode = STARTED;
    this->cmdResponse_out(opCode, cmdSeq, Fw::COMMAND_OK);
  }

  void ComLogger ::
    StopLogging_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq
    )
  {
    this->logMode = STOPPED;
    this->closeFile();
    this->cmdResponse_out(opCode, cmdSeq, Fw::COMMAND_OK);
  }

  void ComLogger ::
    CloseFile_cmdHandler(
      FwOpcodeType opCode,
      U32 cmdSeq
    )
  {
    this->closeFile();
    this->cmdResponse_out(opCode, cmdSeq, Fw::COMMAND_OK);
  }

  void ComLogger ::
    SetRecordName_cmdHandler(
        const FwOpcodeType opCode,
        const U32 cmdSeq,
        const Fw::CmdStringArg& recordName
    )
  {
    this->setFileInfix(recordName);
    this->cmdResponse_out(opCode,cmdSeq,Fw::COMMAND_OK);
  }

  // ----------------------------------------------------------------------
  // File functions:
  // ---------------------------------------------------------------------- 

  void ComLogger ::
    openFile(void)
  {
    FW_ASSERT( CLOSED == this->fileMode );
    setFileSuffix();
    setFileName();

    // Open file
    Os::File::Status ret;
    if (this->resetOnMaxSize) {
      ret = file.open((char*) this->fileName, Os::File::OPEN_CREATE);
    } else {
      ret = file.open((char*) this->fileName, Os::File::OPEN_WRITE);
    }

    if( Os::File::OP_OK != ret ) { 
      if( !openErrorOccurred ) { // throttle this event, otherwise a positive 
                                // feedback event loop can occur!
        Fw::LogStringArg logStringArg((char*) this->fileName);
        this->log_WARNING_HI_FileOpenError(ret, logStringArg);
      }
      openErrorOccurred = true;
      } else {
        // Reset event throttle:
        openErrorOccurred = false;

        // Reset byte count:
        this->byteCount = 0;

        // Set mode:
        this->fileMode = OPEN; 
      }    
  }

  void ComLogger ::
    closeFile(void)
  {
      if( OPEN == this->fileMode ) {
      // Close file:
      this->file.close();

      // Update mode:
      this->fileMode = CLOSED;

      // Send event:
      Fw::LogStringArg logStringArg((char*) this->fileName);
      this->log_DIAGNOSTIC_FileClosed(logStringArg);
    }
  }

  void ComLogger ::
    restartFile(void) 
    {
      if( OPEN == this->fileMode ) {
        // see to beginning of file:
        this->file.seek(0);

        // Reset byte count:
        this->byteCount = 0;

        // Send event:
        Fw::LogStringArg logStringArg((char*) this->fileName);
        this->log_ACTIVITY_LO_FileRestarted(logStringArg);
      }
    }

  void ComLogger ::
    writeComBufferToFile(
      Fw::ComBuffer &data,
      U16 size
    )
  {
    // Setup a buffer to store meta data key and frame size
    const U16 key = static_cast<U16>(COM_LOGGER_FRAME_START_KEY);
    U8 metaData[META_DATA_SIZE];
    Fw::SerialBuffer serialMetaData(metaData, META_DATA_SIZE);

    // Store a frame-key
    FW_ASSERT(serialMetaData.serialize(key) == Fw::FW_SERIALIZE_OK);

    // Store a frame-size
    FW_ASSERT(serialMetaData.serialize(size) == Fw::FW_SERIALIZE_OK);

    // Write out the extras if they exist
    if ( serialMetaData.getBuffLength() > 0 ) {
        if(
            writeToFile(
              serialMetaData.getBuffAddr(), 
              serialMetaData.getBuffLength()
              )
          ) {
          this->byteCount += serialMetaData.getBuffLength();
        }
        else {
          return;
        }
    }

    // Write buffer to file:
    if(writeToFile(data.getBuffAddr(), size)) {
      this->byteCount += size;
    }
  }

  // ----------------------------------------------------------------------
  // Helper functions:
  // ---------------------------------------------------------------------- 

  bool ComLogger ::
    writeToFile(
      void* data, 
      U16 length
    )
  {
    NATIVE_INT_TYPE size = length;
    Os::File::Status ret = file.write(data, size);
    
    if( Os::File::OP_OK != ret || size != (NATIVE_INT_TYPE) length ) {
      if( !writeErrorOccurred ) { // throttle this event, otherwise a positive 
                                 // feedback event loop can occur!
        Fw::LogStringArg logStringArg((char*) this->fileName);
        this->log_WARNING_HI_FileWriteError(ret, size, length, logStringArg);
      }
      writeErrorOccurred = true;
      return false;
    }

    writeErrorOccurred = false;
    return true;
  }

  void ComLogger ::
    setFilePrefix(
      const char* filePrefix
      ) 
    {
      // Copy the file name
      U8* dest = (U8*) Fw::StringUtils::string_copy(
                  (char*)this->filePrefix,
                  filePrefix, 
                  sizeof(this->filePrefix)
                  );

      FW_ASSERT(dest == this->filePrefix, 
                reinterpret_cast<POINTER_CAST>(dest), 
                reinterpret_cast<POINTER_CAST>(this->filePrefix));
    }

  void ComLogger :: 
    setFileSuffix(void)
    {
      if (resetOnMaxSize)
      {
        this->fileSuffix = COM_LOGGER_FILE_EXTENTION;
      }
      else
      {
        Fw::Time timestamp = getTime();
        this->fileSuffix.format(
          "_%d_%d_%06d%s",
          (U32) timestamp.getTimeBase(), 
          timestamp.getSeconds(), 
          timestamp.getUSeconds(),
          COM_LOGGER_FILE_EXTENTION
        );
      }
    }

  void ComLogger ::
    setFileName(void)
    {
      // Create filename:
      U32 bytesCopied = 0;
      bytesCopied = snprintf((char*) this->fileName, sizeof(this->fileName), 
                              "%s%s%s", 
                              this->filePrefix,
                              this->fileInfix.toChar(),
                              this->fileSuffix.toChar());
      FW_ASSERT( bytesCopied < sizeof(this->fileName) );
    }

  void ComLogger ::
    setFileInfix(const Fw::CmdStringArg& recordName)
    {
      if (recordName.length() > 0) {
        this->fileInfix.format("_%s", recordName.toChar());

        // Send event:
        Fw::LogStringArg logStringArg(recordName.toChar());
        this->log_ACTIVITY_LO_RecordNameAdded(logStringArg);
      } 
      else 
      {
        this->fileInfix = "";
      }
    }
};