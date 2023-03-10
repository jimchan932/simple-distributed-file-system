#include <map>
#include <ctime>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "dfs.grpc.pb.h"

#define FAIL 0
#define SUCCESS 1
#define FAIL_GET_FILE 2

#define MAX_CLIENTS 5
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using std::ofstream;
using std::ifstream;
using dfs::DFS;
using dfs::Invalidation;
using dfs::Response;
using dfs::FileRequest;
using dfs::Filename;
using dfs::FileHandle;
using dfs::ClientAddress;
using dfs::DirectoryPath;
using dfs::ClientID;
using dfs::FileList;
const char *filesystem_rootpath = "/home/jimchan/Desktop/myDFS/root";			

std::string convertFilePath(std::string filename)
{
  std::string rootFilePath(filesystem_rootpath);  
  rootFilePath.push_back('/');
  return rootFilePath + filename;
}

class DFS_Server final : public DFS::Service {
 public:
  ~DFS_Server()
  {
	std::map<std::string, pthread_rwlock_t>::iterator it;
	for(it = rwlock_map.begin(); it != rwlock_map.end(); it++)
	{
	  pthread_rwlock_destroy(&(it->second));
	}
  }
  void Init()
  {
	num_of_clients = 0;
	rwlock_map[std::string("poem.txt")] =  PTHREAD_RWLOCK_INITIALIZER;
	rwlock_map[std::string("poem2.txt")] =  PTHREAD_RWLOCK_INITIALIZER;
	rwlock_map[std::string("aphorism.txt")] =  PTHREAD_RWLOCK_INITIALIZER;
	rwlock_map[std::string("text.txt")] =  PTHREAD_RWLOCK_INITIALIZER;
	
  }
  
  Status connectServer(ServerContext* context, const ClientAddress *client_addr, ClientID *clientID) override
  {	  
	if(num_of_clients == MAX_CLIENTS)
	{
		std::cout << "Connect to server failure, already reached max clients\n";
		clientID->set_id(-1);
	}
	else
	{		  				
		client_address_list[num_of_clients] = client_addr->port();

		// client id
		int unique_id = num_of_clients+1;
		client_id_list[num_of_clients] = unique_id;
		clientID->set_id(unique_id);

		invalidationNotice[unique_id] = std::map<std::string, bool>();
		num_of_clients++;
	}

	return Status::OK;
  }
  Status pullInvalidation(ServerContext *context, const FileRequest *fileRequest, Invalidation *invalidation) override
  {
	int is_valid = invalidationNotice[fileRequest->id()][fileRequest->fname()];
	invalidation->set_is_valid(is_valid);
	
	return Status::OK;
  }
  
  Status list(ServerContext *context, const DirectoryPath *directoryPath, FileList *fileList) override
  {
      
	  struct dirent *pDirent;
	  DIR *pDir;
	  DIR *dr;
	  pDir = opendir(filesystem_rootpath);
	  std::string file_list;
	  while((pDirent = readdir(pDir)) != NULL)
	  {
		file_list += pDirent->d_name;
		file_list += ' ';
	  }
	  closedir(pDir);
	  fileList->set_filelist(file_list);
	  return Status::OK;
  }
  
  Status getFileDataToWrite(ServerContext* context, const FileRequest *fileRequest, FileHandle *fileHandle) override
  {
	std::string filePath = convertFilePath(fileRequest->fname());
	auto search = rwlock_map.find(fileRequest->fname());
	if( search != rwlock_map.end())
	{
	  
		int rc;
		int count =0;
		std::cout <<"Entered thread, getting write lock for file\n";

		// attempt to get write lock
	retry_write_lock:
		rc = pthread_rwlock_trywrlock(&rwlock_map[fileRequest->fname()]);
		
		if(rc == EBUSY)
		{
		  if(count >= 5)
		  {
			fileHandle->set_access(FAIL);
			return Status::OK;
		  }
		  std::cout << "Lock is busy, do sth in the meantime.\n";
		  ++count;
		  sleep(1);
		  goto retry_write_lock;
		} else if(rc != 0)
		{
		  std::cout << "Error: \n";
		}

		// invalidate files for all clientID
		for(auto it = invalidationNotice.begin(); it != invalidationNotice.end(); it++)
		{
		  if(it->second.find(fileRequest->fname()) != it->second.end())
		  {
			it->second[fileRequest->fname()] = 0;
		  }
		}
		invalidationNotice[fileRequest->id()][fileRequest->fname()] = 1;
		// read file data
		ifstream fileToRead(filePath);
		std::stringstream strStream;
		strStream << fileToRead.rdbuf();
		fileToRead.close();
		fileHandle->set_fname(fileRequest->fname());
		fileHandle->set_buffer(strStream.str());
		fileHandle->set_access(SUCCESS);

	}
	else
	{
	  fileHandle->set_access(FAIL_GET_FILE);
	}
	return Status::OK;
  }
  Status acquireWriteLock(ServerContext *context, const FileRequest *fileRequest, Response *response)
  {
	std::string filePath = convertFilePath(fileRequest->fname());
	auto search = rwlock_map.find(fileRequest->fname());
	if( search != rwlock_map.end())
	{
	  
		int rc;
		int count =0;
		std::cout <<"Entered thread, getting write lock for file\n";
	retry_write_lock:
		rc = pthread_rwlock_trywrlock(&rwlock_map[fileRequest->fname()]);
		
		
		if(rc == EBUSY)
		{
		  if(count >= 5)
		  {
			response->set_success(FAIL);
			return Status::OK;
		  }
		  std::cout << "Lock is busy, do sth in the meantime.\n";
		  ++count;
		  sleep(1);
		  goto retry_write_lock;
		} else if(rc != 0)
		{
		  std::cout << "Error: \n";
		}

		// invalidate files for all clientID except own
		for(auto it = invalidationNotice.begin(); it != invalidationNotice.end(); it++)
		{
		  if(it->second.find(fileRequest->fname()) != it->second.end())
		  {
			it->second[fileRequest->fname()] = 0;
		  }
		}
		invalidationNotice[fileRequest->id()][fileRequest->fname()] = 1;

		// successfully acquired write lock
		response->set_success(SUCCESS);		
	}
	else
	{
	  response->set_success(FAIL_GET_FILE);
	}	
	return Status::OK;
  }
  Status readFile(ServerContext* context, const FileRequest *fileRequest, FileHandle *fileHandle) override
  {
	
	std::string filePath = convertFilePath(fileRequest->fname());
	auto search = rwlock_map.find(fileRequest->fname());
	if( search != rwlock_map.end())
	{
	  
	  int rc;
	  int count = 0;
	  std::cout <<"Entered thread, getting read lock for file\n";
  retry_read_lock:
	   rc = pthread_rwlock_tryrdlock(&rwlock_map[fileRequest->fname()]);
	  
	  if(rc == EBUSY)
	  {
		if(count >= 5)
		{
		  fileHandle->set_access(FAIL);
		  return Status::OK;
		}
		std::cout << "Lock is busy, do sth in the meantime.\n";
		++count;
		sleep(1);
		goto retry_read_lock;
	  }
	  else if(rc != 0)
	  {
		std::cout << "Error: \n";
	  }
	  // validate
	  invalidationNotice[fileRequest->id()][fileRequest->fname()] = 1;
	  // set file data
	  std::ifstream fileToRead(filePath);
	  std::stringstream strStream;
	  strStream << fileToRead.rdbuf();
	  fileHandle->set_fname(fileRequest->fname());
	  fileHandle->set_buffer(strStream.str());
	  fileHandle->set_access(SUCCESS);
	  fileToRead.close();
	}
	else
	{	
	  fileHandle->set_access(FAIL_GET_FILE);
	}
	return Status::OK;
  }

  Status deleteFile(ServerContext* context, const Filename *filename, Response *response) override
  {
	std::string filePath = convertFilePath(filename->fname());
	auto search = rwlock_map.find(filename->fname());
	if( search != rwlock_map.end())
	{
	  std::cout <<"Entered thread, getting write lock for file\n";
	  int rc = pthread_rwlock_trywrlock(&rwlock_map[filename->fname()]);
	  if(rc == EBUSY)
	  {
		std::cout << "Failed to get write thread.\n";
		response->set_success(FAIL);
		return Status::OK;
	  }
	  remove(filePath.c_str());
	  std::cout << "Unlock the write lock for file.\n";
	  pthread_rwlock_unlock(&rwlock_map[filename->fname()]);
	  std::cout << "rwlock destroyed for file.\n";
	  pthread_rwlock_destroy(&rwlock_map[filename->fname()]);
	  response->set_success(SUCCESS);		
	}
	else
	{
	  response->set_success(FAIL_GET_FILE);
	}
	return Status::OK;
  }
  Status createFile(ServerContext* context, const FileHandle *fileHandle, Response *response) override
  {
	char filePath[50];
	strcpy(filePath,"/home/jimchan/Desktop/myDFS/root/");
	
	strcat(filePath, fileHandle->fname().c_str());
		

	  std::ofstream outfile(filePath);
	  outfile << fileHandle->buffer();
	  response->set_success(SUCCESS);
	  rwlock_map[fileHandle->fname()] =  PTHREAD_RWLOCK_INITIALIZER;
	  outfile.close();
	  return Status::OK;

  }
  Status writeFileAndReleaseLock(ServerContext* context, const FileHandle *fileHandle, Response *response) override
  {
	std::string writeFilePath = convertFilePath(fileHandle->fname());
	ofstream writeFile(writeFilePath);
	writeFile << fileHandle->buffer();
	writeFile.close();

	// release write lock
	auto search = rwlock_map.find(fileHandle->fname());

	if(search != rwlock_map.end())
	{
	  int rc = pthread_rwlock_unlock(&rwlock_map[fileHandle->fname()]);	  
	  
	  if(rc != 0)
		std::cout <<"Error: "<< rc<<std::endl; 
	  // pthread_rwlock_unlock(&rwlock_map[std::string("poem.txt")]);
	  std::cout << "Unlock the write lock for file \n";
	  
	  response->set_success(1);
	}   
	return Status::OK;	  
  }

  Status acquireReadLock(ServerContext* context, const Filename *filename, Response *response) override
  {
	std::string filePath = convertFilePath(filename->fname());
	auto search = rwlock_map.find(filename->fname());
	if( search != rwlock_map.end())
	{	  
	  int rc;
	  int count = 0;
	  std::cout <<"Entered thread, getting read lock for file\n";
	  // attempt to acquire read lock
	retry_read_lock:
	  rc = pthread_rwlock_tryrdlock(&rwlock_map[filename->fname()]);
	  
	  if(rc == EBUSY)
	  {
		if(count >= 5)
		{
		  response->set_success(FAIL);
		  return Status::OK;
		}
		std::cout << "Lock is busy, do sth in the meantime.\n";
		++count;
		sleep(1);
		goto retry_read_lock;
	  } else if(rc != 0)
		{
		  std::cout << "Error: \n";
		}
	  response->set_success(SUCCESS);
	}
	else
	  response->set_success(FAIL_GET_FILE);
	return Status::OK;
  }
  Status releaseReadLock(ServerContext* context, const Filename *filename,  Response *response) override
  {
	  auto search = rwlock_map.find(filename->fname());
	  if( search != rwlock_map.end())
	  {
		int rc = pthread_rwlock_unlock(&rwlock_map[filename->fname()]);
		
		if(rc != 0)
		{
		  std::cout << "Error: "<< rc;
		}
		
		response->set_success(SUCCESS);
		std::cout << "Unlock the read lock for file \n";
	  }
	  else
		response->set_success(FAIL);
	  return Status::OK;
  }


private:
	std::map<std::string, pthread_rwlock_t> rwlock_map; // ??????????????????
	int client_address_list[5];  // client ???????????? ????????????????????????
	int client_id_list[5];    // client??????????????????
	std::map<int, std::map<std::string, bool> > invalidationNotice; // ?????????????????????
	int num_of_clients;  // ??????????????????

};

void RunServer()
{

  std::string server_address("0.0.0.0:50051");
  DFS_Server service;
  service.Init();
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv)
{  
  RunServer();
  return 0;
}
