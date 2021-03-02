// ----------------------------------------------------------------------
//
// ComLogger.cpp
//
// ----------------------------------------------------------------------

#include <Svc/ComLogger/ComLogger.hpp>
#include <Fw/Types/BasicTypes.hpp>
#include <Fw/Types/SerialBuffer.hpp>
#include <Os/ValidateFile.hpp>
#include <Fw/Types/StringUtils.hpp>
#include <Svc/ComLogger/ComLoggerCfg.hpp>
#include <stdio.h>

namespace Svc {

  // ----------------------------------------------------------------------
  // Construction, initialization, and destruction 
  // ----------------------------------------------------------------------

  ComLogger ::
    ComLogger(const char* compName) :
    ComLoggerComponentBase(compName), 
    maxFileSize(0),
    logMode(STARTED),
    fileMode(CLOSED), 
    byteCount(0),
    writeErrorOccured(false),
    openErrorOccured(false),
    storeBufferLength(false),
    storeFrameKey(false),
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

      // Write out the hash file to disk:
      this->writeHashFile();

      // Update mode:
      this->fileMode = CLOSED;
    }
  }

  void ComLogger ::
    setup(
      const char* filePrefix,
      U32 maxFileSize,
      bool storeBufferLength, 
      bool storeFrameKey,
      bool resetOnMaxSize
    )
    {
      this->maxFileSize = maxFileSize;
      this->storeBufferLength = storeBufferLength;
      this->storeFrameKey = storeFrameKey;
      this->resetOnMaxSize = resetOnMaxSize;
      
      if(this->storeBufferLength) {
        FW_ASSERT(maxFileSize > sizeof(U16), maxFileSize); // must be a positive integer greater than buffer length size
      }
      else {
        FW_ASSERT(maxFileSize > sizeof(0), maxFileSize); // must be a positive integer
      }
      
      setFilePrefix(filePrefix);
      
      this->fileInfoSet = true;
    }
  
  void ComLogger::preamble(void) {
      FW_ASSERT(this->fileInfoSet);
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
    // ComLogger only writes 16-bit sizes to save space 
    // on disk:
    FW_ASSERT(size32 < 65536, size32);
    U16 size = size32 & 0xFFFF;

    // Close or restart the file if it will be too big:
    if( OPEN == this->fileMode ) {
      U32 projectedByteCount = this->byteCount + size;
      if( this->storeBufferLength ) {
        projectedByteCount += sizeof(size);
      }
      if( projectedByteCount > this->maxFileSize ) {
        if (this->resetOnMaxSize) {
          this->restartFile();
        }
        else {
          this->closeFile();
        }
      }
    }

    // Open the file if it there is not one open:
    if( CLOSED == this->fileMode && STOPPED != this->logMode){
      this->openFile();
    }

    // Write to the file if it is open:
    if( OPEN == this->fileMode ) {
      this->writeComBufferToFile(data, size);
    }
  }

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
    pingIn_handler(
        const NATIVE_INT_TYPE portNum,
        U32 key
    )
  {
      // return key
      this->pingOut_out(0,key);
  }

  void ComLogger ::
    openFile(
    )
  {
    FW_ASSERT( CLOSED == this->fileMode );
    setFileName();

    // Open file
    Os::File::Status ret;
    if (this->resetOnMaxSize) {
      ret = file.open((char*) this->fileName, Os::File::OPEN_CREATE);
    } else {
      ret = file.open((char*) this->fileName, Os::File::OPEN_WRITE);
    }

    if( Os::File::OP_OK != ret ) { 
      if( !openErrorOccured ) { // throttle this event, otherwise a positive 
                                // feedback event loop can occur!
        Fw::LogStringArg logStringArg((char*) this->fileName);
        this->log_WARNING_HI_FileOpenError(ret, logStringArg);
      }
      openErrorOccured = true;
      } else {
        // Reset event throttle:
        openErrorOccured = false;

        // Reset byte count:
        this->byteCount = 0;

        // Set mode:
        this->fileMode = OPEN; 
      }    
  }

  void ComLogger ::
    closeFile(
    )
  {
      if( OPEN == this->fileMode ) {
      // Close file:
      this->file.close();

      // Write out the hash file to disk:
      this->writeHashFile();

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
    // Setup a buffer to store optional key and size
    U16 key = static_cast<U16>(COM_LOGGER_FRAME_START_KEY);
    U8 buffer[sizeof(size) + sizeof(key)];
    Fw::SerialBuffer serialExtra(buffer, sizeof(size) + sizeof(key));

    // Store a frame-key, if desired
    if ( this->storeFrameKey ) {
        FW_ASSERT(serialExtra.serialize(key) == Fw::FW_SERIALIZE_OK);
    }

    // Store a frame-size, if desired
    if( this->storeBufferLength ) {
        FW_ASSERT(serialExtra.serialize(size) == Fw::FW_SERIALIZE_OK);
    }

    // Write out the extras if they exist
    if ( serialExtra.getBuffLength() > 0 ) {
        if(writeToFile(serialExtra.getBuffAddr(), serialExtra.getBuffLength())) {
          this->byteCount += serialExtra.getBuffLength();
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

  bool ComLogger ::
    writeToFile(
      void* data, 
      U16 length
    )
  {
    NATIVE_INT_TYPE size = length;
    Os::File::Status ret = file.write(data, size);
    if( Os::File::OP_OK != ret || size != (NATIVE_INT_TYPE) length ) {
      if( !writeErrorOccured ) { // throttle this event, otherwise a positive 
                                 // feedback event loop can occur!
        Fw::LogStringArg logStringArg((char*) this->fileName);
        this->log_WARNING_HI_FileWriteError(ret, size, length, logStringArg);
      }
      writeErrorOccured = true;
      return false;
    }

    writeErrorOccured = false;
    return true;
  }

  void ComLogger :: 
    writeHashFile(void)
  {
    Os::ValidateFile::Status validateStatus;
    validateStatus = Os::ValidateFile::createValidation((char*) this->fileName, (char*)this->hashFileName);
    if( Os::ValidateFile::VALIDATION_OK != validateStatus ) {
      Fw::LogStringArg logStringArg1((char*) this->fileName);
      Fw::LogStringArg logStringArg2((char*) this->hashFileName);
      this->log_WARNING_LO_FileValidationError(logStringArg1, logStringArg2, validateStatus);
    }
  }

  void ComLogger :: 
    getFileSuffix(
      U8* suffix
      )
    {
      U32 bytesCopied;
      if (resetOnMaxSize) 
      {
        bytesCopied = snprintf((char*) suffix, MAX_SUFFIX_LENGTH, 
                                COM_LOGGER_FILE_EXTENTION);
      } 
      else 
      {
        Fw::Time timestamp = getTime();
        bytesCopied = snprintf((char*) suffix, MAX_SUFFIX_LENGTH, 
                              "_%d_%d_%06d%s", 
                              (U32) timestamp.getTimeBase(), 
                              timestamp.getSeconds(), 
                              timestamp.getUSeconds(),
                              COM_LOGGER_FILE_EXTENTION);
      }
                              
      // A return value of size or more means that the output was truncated
      FW_ASSERT( bytesCopied < MAX_SUFFIX_LENGTH );
    }

  void ComLogger ::
    setFileName(void)
    {
      U8 suffix[MAX_SUFFIX_LENGTH]; 
      getFileSuffix(suffix);
      
      // Create filename:
      U32 bytesCopied;
      bytesCopied = snprintf((char*) this->fileName, sizeof(this->fileName), 
                              "%s%s", 
                              this->filePrefix, 
                              suffix);
      FW_ASSERT( bytesCopied < sizeof(this->fileName) );

      // Create hash filename:
      bytesCopied = snprintf((char*) this->hashFileName, sizeof(this->hashFileName), 
                              "%s%s%s", 
                              this->filePrefix, 
                              suffix,
                              Utils::Hash::getFileExtensionString()); 
      FW_ASSERT( bytesCopied < sizeof(this->hashFileName) );
    }
};