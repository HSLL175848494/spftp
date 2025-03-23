/**
 * @file FTPTask.h
 * @brief FTP Server task management and thread pooling
 * @details Handles asynchronous processing of FTP server events using thread pool
 */

 #ifndef HSLL_FTPTASK
 #define HSLL_FTPTASK
 
 #include "../FtpServer/FtpServer.h"
 
 namespace HSLL
 {
     /**
      * @brief FTP task type enumeration
      * @details Specifies different types of FTP operations to be processed
      */
     enum FTP_TASK_TYPE
     {
         FTP_TASK_TYPE_READ,    //!< Data read operation task
         FTP_TASK_TYPE_WRITE,   //!< Data write operation task
         FTP_TASK_TYPE_ACCEPT    //!< New connection acceptance task
     };
 
     /**
      * @brief FTP task structure
      * @details Encapsulates FTP server tasks for thread pool processing
      */
     struct FTPTask
     {
         FTP_TASK_TYPE type;    //!< Type of the task
         FTPServer *ftpServer;  //!< Associated FTP server instance
 
         /**
          * @brief Execute the contained task
          * @details Routes to appropriate handling method based on task type
          */
         void execute()
         {
             switch (type)
             {
             case FTP_TASK_TYPE_READ:
                 ftpServer->DealRead();
                 break;
             case FTP_TASK_TYPE_WRITE:
                 ftpServer->DealWrite();
                 break;
             default:
                 ftpServer->DealAccept();
                 break;
             }
         }
     };
 
     /// Global thread pool instance for FTP task processing
     ThreadPool<FTPTask> pool;
 
     /**
      * @brief Handle new FTP connection
      * @param evb Event buffer for the connection
      * @param info Connection information structure
      * @return Pointer to created FTPServer instance
      * @details Initializes new server instance and queues accept task
      */
     void *FTPConnection(EVBuffer evb, ConnectionInfo info)
     {
         FTPServer *ftpServer = new FTPServer(evb,info);
         ftpServer->DisableRW();
 
         if (pool.Append(FTPTask{FTP_TASK_TYPE_ACCEPT, ftpServer}) == false)
             ftpServer->EnableRW();
 
         return ftpServer;
     }
 
     /**
      * @brief Clean up FTP server resources
      * @param ctx FTPServer instance pointer
      * @details Safely destroys server instance after ensuring completion
      */
     void FTPDisconnection(void *ctx)
     {
         FTPServer *ftpServer = (FTPServer *)ctx;
         while (ftpServer->CheakFree() == false)
             std::this_thread::sleep_for(std::chrono::milliseconds(2));
         delete ftpServer;
     }
 
     /**
      * @brief Handle read event processing
      * @param ctx FTPServer instance pointer
      * @return true if successful, false if error detected
      * @details Queues read task to thread pool
      */
     bool FTPRead(void *ctx)
     {
         FTPServer *ftpServer = (FTPServer *)ctx;
         if (ftpServer->CheakError())
             return false;
 
         ftpServer->DisableRW();
 
         if (pool.Append(FTPTask{FTP_TASK_TYPE_READ, ftpServer}) == false)
             ftpServer->EnableRW();
 
         return true;
     }
 
     /**
      * @brief Handle write event processing
      * @param ctx FTPServer instance pointer
      * @return true if successful, false if error detected
      * @details Queues write task to thread pool
      */
     bool FTPWrite(void *ctx)
     {
         FTPServer *ftpServer = (FTPServer *)ctx;
         if (ftpServer->CheakError())
             return false;
 
         ftpServer->DisableRW();
         
         if (pool.Append(FTPTask{FTP_TASK_TYPE_WRITE, ftpServer}) == false)
             ftpServer->EnableRW();
 
         return true;
     }
 }
 
 #endif