#include <fstream>
using namespace std;
int main()
{
  ofstream file("/home/jimchan/Desktop/a.txt");
 
  file << "Thisis it";
  file.close();
  return 0;
}
