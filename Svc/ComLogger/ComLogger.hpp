// ----------------------------------------------------------------------
//
// ComLogger.hpp
//
// ----------------------------------------------------------------------

#ifndef Svc_ComLogger_HPP
#define Svc_ComLogger_HPP

#include "Svc/ComLogger/ComLoggerComponentAc.hpp"
#include <Os/File.hpp>
#include <Fw/Types/EightyCharString.hpp>

#include <limits.h>

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
        bool resetOnMaxSize=false);

      //! preamble function will be called before the event loop is entered
      //! to make sure setup() has been called at least once
      void preamble(void);

    PRIVATE:

      // ----------------------------------------------------------------------
      // Handler implementations
      // ----------------------------------------------------------------------
      
      //! Handler implementation for comIn
      //!
      void comIn_handler(
          const NATIVE_INT_TYPE portNum, /*!< The port number*/
          Fw::ComBuffer &data, /*!< Buffer containing packet data*/
          U32 context /*!< Call context value; meaning chosen by user*/

      );

      //! Handler implementation for pingIn
      //!
      void pingIn_handler(
          const NATIVE_INT_TYPE portNum, /*!< The port number*/
          U32 key /*!< Value to return to pinger*/
      );

    PRIVATE:

      // ----------------------------------------------------------------------
      // Command handler implementations
      // ----------------------------------------------------------------------

      //! Implementation for CloseFile command handler
      //! Forces a close of the currently opened file.
      void CloseFile_cmdHandler(
          const FwOpcodeType opCode, /*!< The opcode*/
          const U32 cmdSeq /*!< The command sequence number*/
      );

      //! Implementation for StopLogging command handler
      //! Forces a close of the currently opened file, stops opening new file.
      void StopLogging_cmdHandler(
          const FwOpcodeType opCode, /*!< The opcode*/
          const U32 cmdSeq /*!< The command sequence number*/
      );

      //! Implementation for StartLogging command handler
      //! Allows opening new file after stop.
      void StartLogging_cmdHandler(
          const FwOpcodeType opCode, /*!< The opcode*/
          const U32 cmdSeq /*!< The command sequence number*/
      );

      //! Implementation for SetLogName command handler
      //! Set a name to be added after the file prefix.
      void SetRecordName_cmdHandler(
          const FwOpcodeType opCode, /*!< The opcode*/
          const U32 cmdSeq, /*!< The command sequence number*/
          const Fw::CmdStringArg& recordName 
      );

      // ----------------------------------------------------------------------
      // Constants:
      // ----------------------------------------------------------------------
      enum { 
        // "frame key" and "length of buffer" are U16 and are saved as meta data
        // before each frame
        META_DATA_SIZE = 2 * sizeof(U16),
        // The maximum size of a filename as defined in limits.h 
        MAX_FILENAME_LENGTH = NAME_MAX + PATH_MAX, 
        // Suffix is file counter, timestamp and file extention
        MAX_SUFFIX_LENGTH = 80,
        // Infix (Record Name) is an optional string that user can add to the file prefix
        MAX_INFIX_LENGTH = 80,
        // Prefix is file path and base name of log file set during the setup
        MAX_PREFIX_LENGTH = MAX_FILENAME_LENGTH - MAX_INFIX_LENGTH - MAX_SUFFIX_LENGTH 
      };

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

      // ----------------------------------------------------------------------
      // Member variables:
      // ---------------------------------------------------------------------- 
      
      U8 filePrefix[MAX_PREFIX_LENGTH]; //!< Path and filename defined by user
      Fw::EightyCharString fileInfix;   //!< An optional string (record name) that could be added to file prefix
      Fw::EightyCharString fileSuffix;  //!< TimeStamp and file extention added during opening log file
      U8 fileName[MAX_FILENAME_LENGTH]; //!< Complete filename after adding prefix infix and suffix
      Os::File file;                    //!< File handler
      U32 maxFileSize;                  //!< Maximum allowable file size before wraparound or creating new file 
      U32 byteCount;                    //!< Number of bytes written to current open file
      LoggingMode logMode;              //!< Indicating current state of logging (stopped or started)
      FileMode fileMode;                //!< Indicating if the file is open or closed
      bool writeErrorOccurred;          //<! Indicating there was an error writing to the file
      bool openErrorOccurred;           //<! Indicating there was an error opening the file
      bool fileInfoSet;                 //<! Indicating setup was called and was successful
      bool resetOnMaxSize;              //<! If true will wraparound the file instead of creating new file

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

      bool writeToFile(
        void* data, 
        U16 length
      );

      // ----------------------------------------------------------------------
      // Helper functions:
      // ---------------------------------------------------------------------- 

      //! Set filePrefix (filepath and filename) defined by user
      void setFilePrefix(const char* fileName); 

      void setFileName(void);

      void setFileSuffix(void);

      void setFileInfix(const Fw::CmdStringArg& recordName);
    };
  };

  #endif