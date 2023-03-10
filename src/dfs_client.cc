#include <iostream>
#include <memory>
#include <string>
#include <stdio.h>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/server_credentials.h>
#include "dfs.grpc.pb.h"
#include <unistd.h>
#include "file_cache.h"
#define FAIL 0
#define SUCCESS 1
#define FAIL_GET_FILE 2

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::ClientReader;
using grpc::Status;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using std::ofstream;
using std::ifstream;
using dfs::FileRequest;
using dfs::DFS;
using dfs::Response;
using dfs::Filename;
using dfs::FileHandle;
using dfs::ClientAddress;
using dfs::DirectoryPath;
using dfs::Invalidation;
using dfs::ClientID;
using dfs::FileList;
#include "file_cache.h"
#include <string>
#include <string.h>
FileCacheQueue::FileCacheQueue()
{
  capacity = 4;
  front = 0;
  rear = -1;
  count = 0;
}
int FileCacheQueue::size()
{
  return count;
}
bool FileCacheQueue::isEmpty()
{
  return (size() == 0);
}
bool FileCacheQueue::isFull()
{
  return (size() == capacity);
}

int FileCacheQueue::getCapacity()
{
  return capacity;
}
void FileCacheQueue::dequeue()
{
  if(isEmpty())
  {
	printf("Underflow\n");
  }
  front = (front + 1) % capacity;
  count--;
}
void FileCacheQueue::enqueue(file_cache file_data)
{
  if(isFull())
  {
	printf("Overflow");	
  }
  rear = (rear+1) % capacity;
  fileCacheList[rear].filename = file_data.filename;
  strcpy(fileCacheList[rear].buffer, file_data.buffer);
  fileCacheList[rear].is_valid = true;
}
enum QUERY_TYPE FileCacheQueue::readFileCache(std::string filename, char *outBuffer)
{
  for(int i = rear; i <= front; i++)
  {
	if(filename == fileCacheList[i].filename)
	{	  
	  strcpy(outBuffer, fileCacheList[i].buffer);
	  return HIT;
	}
  }
  return MISS_AND_APPEND;
}

void FileCacheQueue::replaceFileCache(std::string filename, char *inBuffer)
{
  for(int i = rear; i <= front; i++)
  {
	if(filename == fileCacheList[i].filename)
	{
	  strcpy(fileCacheList[i].buffer, inBuffer);
	}
  } 
}


void set_fileCache(file_cache *fcache, const std::string& filename, char *inBuffer)
{
  fcache->filename = filename;
  strcpy(fcache->buffer, inBuffer);
}

class DFSClient {
 public:
  DFSClient(std::shared_ptr<Channel> channel)
      : stub_(DFS::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  bool connect(const std::string& ip_address, int port, int *client_id) {
    // Data we are sending to the server.
    ClientAddress clientAddr;
    clientAddr.set_ip_address(ip_address);
	clientAddr.set_port(port);
    // Container for the data we expect from the server.
    ClientID clientID;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->connectServer(&context, clientAddr, &clientID);
	*client_id = clientID.id();
    // Act upon its status.
    if (status.ok()) {
      if(clientID.id() != -1)
	  {
		std::cout << "Connection successful";
		return 1;
	  }
	  else
	  {
		std::cout << "Connection failed: too many clients!";
		return 0;
	  }
	} else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;      
	  return 0;
	}
  }
  void list_root_directory()
  {
	ClientContext context;
	DirectoryPath dirPath;
	FileList flist;
	stub_->list(&context, dirPath, &flist);
	std::cout << flist.filelist();
	std::cout << std::endl;
  }
  bool pullInvalidation(const std::string& fname, const int& id)
  {
	ClientContext context;
	Invalidation invalidation;
	FileRequest fileRequest;
	fileRequest.set_fname(fname);
	fileRequest.set_id(id);
	stub_->pullInvalidation(&context, fileRequest, &invalidation);
	return invalidation.is_valid();
  }
  bool acquireReadLock(const std::string& fname)
  {
	Filename filename;
	Response response;
	filename.set_fname(fname);
	AcquireReadLockOnce(filename, &response);
	if(response.success() == SUCCESS)
	{
	  std::cout << "Acquired read lock for file.\n";
	 
	  return true;
	}
	else if(response.success() == FAIL)
	{
	  std::cout << "Failed to acquire read lock for file.\n";
	  return false;
	}
	else
	{
	  std::cout << "No file " << filename.fname() << "in filesystem. ";
	  std::cout << "Failed to read file to write." << std::endl;
	  return false;
	}		
  }  
  void releaseReadLock(const std::string& fname)
  {	
	Filename filename;
	Response response;
	filename.set_fname(fname);
	ReleaseReadLockOnce(filename, &response);	
	std::cout << "Release read lock for file." << std::endl;
  }
  
  bool readFile(const std::string& fname, const int& id, FileHandle *fileHandle)
  {
	FileRequest fileRequest;
	
	fileRequest.set_fname(fname);
	fileRequest.set_id(id);
	ReadFileOnce(fileRequest, fileHandle);
	if(fileHandle->access() == SUCCESS)	  
	{
	  std::cout << "Acquired read lock for file.\n";
	  std::cout << "File data:\n";
	  std::cout << fileHandle->buffer();
	  std::cout << std::endl;
	  return true;
	}
	else if(fileHandle->access() == FAIL)
	{
	  std::cout << "Failed to acquire read lock." << std::endl;
	  return false;
	}
	else 
	{
	  std::cout << "No file " << fileRequest.fname() << "in filesystem. ";
	  std::cout << "Failed to read file" << std::endl;
	  
	  return false;
	}
  }
  bool getFileDataToWrite(const std::string& fname, const int& id)
  {
	FileRequest fileRequest;
	FileHandle fileHandle;
	fileRequest.set_id(id);
	fileRequest.set_fname(fname);
	GetFileDataToWriteOnce(fileRequest, &fileHandle);
	if(fileHandle.access() == SUCCESS)	  
	{	 
	  std::cout << "File data:\n";
	  std::cout << fileHandle.buffer(); 
	  std::cout << std::endl;
	  return true;
	}
	else if(fileHandle.access() == FAIL)
	{
	
	  std::cout << "Failed to acquire write lock." << std::endl;
	  return false;
	}
	else 
	{
	  std::cout << "No file " << fileRequest.fname() << "in filesystem. ";
	  std::cout << "Failed to read file to write." << std::endl;
	  return false;
	}
	std::cout << std::endl;
  }
  bool acquireWriteLock(const std::string &fname, const int& id)
  {
	FileRequest fileRequest;
	Response response;
	fileRequest.set_id(id);
	fileRequest.set_fname(fname);
	AcquireWriteLockOnce(fileRequest, &response);
	if(response.success() == SUCCESS)	  
	{
	  std::cout << "Acquired write lock." << std::endl;
	  return true;
	}
	else if(response.success() == FAIL)
	{
	
	  std::cout << "Failed to acquire write lock." << std::endl;
	  return false;
	}
	else 
	{
	  std::cout << "No file " << fileRequest.fname() << "in filesystem. ";
	  std::cout << "Failed to read file to write." << std::endl;
	  return false;
	}
	std::cout << std::endl;
  }
  void writeFileAndReleaseLock(const std::string& fname, const std::string& writeBuffer)
  {
	ClientContext context;
	FileHandle fileHandle;
	Response response;
	fileHandle.set_fname(fname);
	fileHandle.set_buffer(writeBuffer);
    WriteFileAndReleaseLockOnce(fileHandle, &response);
	std::cout << "Release write lock for file." << std::endl;
  }
  void createFile(const std::string& fname, const std::string& writeBuffer)
  {
	FileHandle fileHandle;
	Response response;
	fileHandle.set_fname(fname);
	fileHandle.set_buffer(writeBuffer);
    CreateFileOnce(fileHandle, &response);
	if(response.success() == FAIL)
	{
	  std::cout << "File already exists in filesystem" << std::endl;
	}
	else
	  std::cout << "Created file " << fname << std::endl;
  }
  void deleteFile(const std::string& fname)
  {
	Filename filename;
	Response response;
	filename.set_fname(fname);
	DeleteFileOnce(filename, &response);
	if(response.success() == FAIL)
	{
	  std::cout << "Error: No such file in filesystem" << std::endl;
	}
	else
	{
	  std::cout << "Delete file success." << std::endl;
	}
  }
  
private:
  std::unique_ptr<DFS::Stub> stub_;
  
  bool ReadFileOnce(const FileRequest& fileRequest, FileHandle *fileHandle)
  {
	ClientContext context;
	Status status = stub_->readFile(&context, fileRequest, fileHandle);
	return true;
  } 
  bool AcquireReadLockOnce(const Filename& filename, Response *response)
  {
	ClientContext context;
	Status status = stub_->acquireReadLock(&context, filename, response);
	
	return response->success();
  }
  
  bool ReleaseReadLockOnce(const Filename& filename, Response *response)
  {
	ClientContext context;
	Status status = stub_->releaseReadLock(&context, filename, response);
	
	return response->success();
  }
  bool GetFileDataToWriteOnce(const FileRequest& fileRequest, FileHandle *fileHandle)
  {
	ClientContext context;
	Status status = stub_->getFileDataToWrite(&context, fileRequest, fileHandle);
	return true;
  }
  bool AcquireWriteLockOnce(const FileRequest& fileRequest, Response *response)
  {
	ClientContext context;
	Status status = stub_->acquireWriteLock(&context, fileRequest, response);
	return true;
  }
  bool WriteFileAndReleaseLockOnce(const FileHandle& fileHandle, Response *response)
  {
	ClientContext context;
	Status status = stub_->writeFileAndReleaseLock(&context, fileHandle, response);
	
	return response->success();
  }
  bool CreateFileOnce(const FileHandle& fileHandle,  Response *response) 
  {
	ClientContext context;
	Status status = stub_->createFile(&context, fileHandle, response);
	std::cout << "Response:" << response;
	return true;
  }
   bool DeleteFileOnce(const Filename& fileHandle,  Response *response) 
  {
	ClientContext context;
	Status status = stub_->deleteFile(&context, fileHandle, response);
	return true;
  }
};

int main(int argc, char** argv) { 
  // localhost at port 500051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  DFSClient client(grpc::CreateChannel(
      "localhost:50051", grpc::InsecureChannelCredentials()));

  int unique_id;
  std::string ip_addr("localhost");
  int PORT = atoi(argv[0]);
  if(client.connect(ip_addr, PORT, &unique_id) == FAIL)
  {	
	return 0;
  }
  FileCacheQueue fCache;
  while(true)
  {	
	std::cout << "Enter command number(0 for read, 1 for write, 2 for list, 3 for create, 4 for delete, 5 for quit):" << std::endl;
	int command_no;
	std::cin >> command_no;
	if(command_no == 0)
	{
	  // input filename
	  std::cout << "Enter filename to read: ";
	  
	  char input_filename[10];
	  scanf("%s", input_filename);
	  std::string filename(input_filename);
	  // attempt to get file from cache 
	  char outBuffer[5000];
	  enum QUERY_TYPE cache_result = fCache.readFileCache(filename, outBuffer);
	  if(cache_result == HIT)
	  {
		if(client.pullInvalidation(filename, unique_id) == 1) // valid
		{
		  if(client.acquireReadLock(filename) == SUCCESS)
		  {		
			std::cout << "File data (cache hit):\n";
			std::cout << outBuffer;
			std::cout << std::endl;
		    sleep(5);
			client.releaseReadLock(filename);	 
		  }
		}
		else // cache is invalid, refresh cache
		{
		  std::cout << "# Cache is dirty.\n";
		  FileHandle fileHandle;
		  if(client.readFile(filename, unique_id, &fileHandle) == SUCCESS)
		  {
			std::cout << std::endl;
			sleep(5);
			client.releaseReadLock(filename);
			fCache.replaceFileCache(filename, const_cast<char *>(fileHandle.buffer().c_str()));
		  }
		}
	  }
	  else // not in cache
	  {
		FileHandle fileHandle;
		if(client.readFile(filename, unique_id, &fileHandle)==SUCCESS)
		
		  sleep(5);
		  client.releaseReadLock(filename);

		  // write to cache (adding entry)
		  if(fCache.isFull())	  
			fCache.dequeue();
		  // add entry
		  file_cache entry;
		  set_fileCache(&entry, fileHandle.fname(),const_cast<char *>(fileHandle.buffer().c_str()));
		  entry.is_valid = 1;
		  fCache.enqueue(entry);
		
	  } 
	}
	if(command_no == 1)
	{
	  std::cout << "Enter filename to write: ";
	  std::string filename;
	  std::cin.ignore();
	  std::cin >> filename;
	  
	  char outBuffer[5000];
	  enum QUERY_TYPE cache_result = fCache.readFileCache(filename, outBuffer);
	  if(cache_result == HIT)
	  {
		if(client.pullInvalidation(filename, unique_id) == 1) // valid
		{
		  if(client.acquireWriteLock(filename, unique_id) == SUCCESS)
		  {		
			std::cout << "File data (cache hit):\n";
			std::cout << outBuffer;
			std::cout << std::endl;
			char writeBuffer[5000];
			std::cout << "Enter text to write to file(no longer than 5000 chars):\n";
			std::cin.ignore();
			scanf("%4999[^\n]", writeBuffer);
			client.writeFileAndReleaseLock(filename, std::string(writeBuffer));
			fCache.replaceFileCache(filename, writeBuffer);
		  }
		}
		else // cache is invalid, refresh cache
		{
		  FileHandle fileHandle;
		  if(client.getFileDataToWrite(filename, unique_id) == SUCCESS)
		  {
			char writeBuffer[5000];
			std::cout << "Enter text to write to file:\n";
			std::cin.ignore();
			scanf("%4999[^\n]", writeBuffer);
			client.writeFileAndReleaseLock(filename, std::string(writeBuffer));
			fCache.replaceFileCache(filename, writeBuffer);
		  }
		}
	  }
	  else // not in cache
	  {
		FileHandle fileHandle;
		if(client.getFileDataToWrite(filename, unique_id))
		{
		  char writeBuffer[5000];
		  std::cout << "Enter text to write to file:\n";
		  std::cin.ignore();
		  scanf("%4999[^\n]", writeBuffer);
		  client.writeFileAndReleaseLock(filename, std::string(writeBuffer));
		  // write to cache (adding entry)
		  if(fCache.isFull())	  
			fCache.dequeue();
		  // add entry
		  file_cache entry;
		  set_fileCache(&entry, filename, writeBuffer);		  
		  fCache.enqueue(entry);		  
		}
	  }
	}
	if(command_no == 2)
	{
	  client.list_root_directory();
	}
	if(command_no == 3)
	{
	  std::cout << "Enter filename to create: ";
	  std::string filename;
	  std::cin >> filename;	 
	  std::cout << "Enter text to write to file " << filename << ": ";
	  std::string writeBuffer;
	  std::cin.ignore();
	  std::getline(std::cin, writeBuffer);
	  client.createFile(filename, writeBuffer);
	}
	if(command_no == 4)
	{
	  std::cout << "Enter filename to delete: ";
	  std::string filename;
	  std::cin >> filename; 
	  client.deleteFile(filename);
	}
	if(command_no == 5)
	{	 
	  break;
	}
  }
    
  return 0;
}
