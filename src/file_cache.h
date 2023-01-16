#ifndef _CACHE_
#define _CACHE_

#include <string>
typedef struct
{
  std::string filename;
  char buffer[5000];
  int is_valid;
} file_cache;

enum QUERY_TYPE {HIT, MISS_AND_REPLACE, MISS_AND_APPEND};
class FileCacheQueue
{
  file_cache fileCacheList[4];
  int capacity;
  int front;
  int rear;
  int count;
public:
  FileCacheQueue();
  void init(int size);
  int getCapacity();
  void dequeue();
  void enqueue(file_cache file_data);
  int size();
  bool isEmpty();
  bool isFull();
  enum QUERY_TYPE readFileCache(std::string filename, char *outBuffer);
  void replaceFileCache(std::string filename, char *inBuffer);
};

#endif
