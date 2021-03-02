// ----------------------------------------------------------------------
//
// ComLogger.hpp
//
// ----------------------------------------------------------------------

#ifndef Svc_ComLogger_HPP
#define Svc_ComLogger_HPP

#include "Svc/ComLogger/ComLoggerComponentAc.hpp"
#include <Os/File.hpp>
#include <Os/Mutex.hpp>
#include <Fw/Types/Assert.hpp>
#include <Utils/Hash/Hash.hpp>

#include <limits.h>
#include <stdio.h>
#include <cstdarg>

namespace Svc {

  class ComLogger :
    public ComLoggerComponentBase
  {
      // ----------------------------------------------------------------------
      // Construction, initialization, and destruction
      // ----------------------------------------------------------------------

    public:
      ComLogger(const char* compName);

      void init(
          NATIVE_INT_TYPE queueDepth, //!< The queue depth
          NATIVE_INT_TYPE instance //!< The instance number
      );

      ~ComLogger(void);

      void setup(
        const char* filePrefix,
        U32 maxFileSize,
        bool storeBufferLength=true, 
        bool storeFrameKey=true,
        bool resetOnMaxSize=false);

      //! preamble function will be called before the event loop is entered
      //! to make sure setup() has been called at least once
      void preamble(void);

      //! Set filePrefix (filepath and filename) defined by user to be used as
      //! ComLogger output file
      void setFilePrefix(const char* fileName); 

      // ----------------------------------------------------------------------
      // Handler implementations
      // ----------------------------------------------------------------------

    PRIVATE:

      void comIn_handler(
          NATIVE_INT_TYPE portNum,
          Fw::ComBuffer &data,
          U32 context
      );

      void CloseFile_cmdHandler(
          FwOpcodeType opCode,
          U32 cmdSeq
      );

      void StartLogging_cmdHandler(
          FwOpcodeType opCode,
          U32 cmdSeq
      );

      void StopLogging_cmdHandler(
          FwOpcodeType opCode,
          U32 cmdSeq
      );

      //! Handler implementation for pingIn
      //!
      void pingIn_handler(
          const NATIVE_INT_TYPE portNum, /*!< The port number*/
          U32 key /*!< Value to return to pinger*/
      );

      // ----------------------------------------------------------------------
      // Constants:
      // ----------------------------------------------------------------------
      enum { 
        // The maximum size of a filename as defined in limits.h 
        // max bytes in a file name + max bytes in a path
        MAX_FILENAME_LENGTH = NAME_MAX + PATH_MAX, 
        // Suffix has the following format _%d_%d_%06d%s
        // Where %d are BaseTime, seconds, micro secends timestamp 
        // and %s is file extention defined in ComLoggerCfg.hpp
        MAX_SUFFIX_LENGTH = 40
      };

      // Maximum filePrefix size must be at most MAX_SUFFIX_LENGTH smaller 
      // than MAX_FILENAME_LENGTH to allow room for suffix (timestamp and 
      // file extension) which is added by the ComLogger
      U8 filePrefix[MAX_FILENAME_LENGTH - MAX_SUFFIX_LENGTH]; //!< Path and filename defined by user
      U32 maxFileSize; //!< Maximum allowable file size before wraparound or creating new file 

      // ----------------------------------------------------------------------
      // Internal state:
      // ----------------------------------------------------------------------
      enum FileMode {
          CLOSED = 0,
          OPEN = 1
      };

      enum LoggingMode {
          STOPPED = 0,
          STARTED = 1
      };

      LoggingMode logMode;
      FileMode fileMode;
      Os::File file;
      U8 fileName[MAX_FILENAME_LENGTH];
      U8 hashFileName[MAX_FILENAME_LENGTH];
      U32 byteCount;
      bool writeErrorOccured;
      bool openErrorOccured;
      bool storeBufferLength;
      bool storeFrameKey;
      bool fileInfoSet;
      bool resetOnMaxSize; 
      // ----------------------------------------------------------------------
      // File functions:
      // ---------------------------------------------------------------------- 
      void openFile(void);

      void closeFile(void);

      void restartFile(void); //!< resets file to beginning

      void writeComBufferToFile(
        Fw::ComBuffer &data,
        U16 size
      );

      // ----------------------------------------------------------------------
      // Helper functions:
      // ---------------------------------------------------------------------- 

      bool writeToFile(
        void* data, 
        U16 length
      );
    
      void writeHashFile(void);

      void setFileName(void);

      void getFileSuffix(U8* suffix);

    };
  };

  #endif