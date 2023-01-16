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
