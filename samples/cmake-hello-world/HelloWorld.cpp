#include <Speaker.h>

using namespace std;
using namespace Hello;

// GIVEN

int main(int argc, char *argv[]) {
  Speaker *speaker = new Speaker();

  speaker->sayHello();

  std::cout << argc << std::endl;
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << std::endl;
  }
}
