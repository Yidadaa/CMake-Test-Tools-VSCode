#include <iostream>

int main(int argc, char *argv[]) {
  std::cout << "hello world" << std::endl;

  std::cout << argc + 2 << std::endl;
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << std::endl;
  }
}
