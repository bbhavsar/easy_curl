# Simple C++ curl library wrapping libcurl

This is the EasyCurl implementation taken from the
[Apache Kudu repository](https://github.com/apache/kudu/blob/master/src/kudu/util/curl_util.h#L46)
to be able to use the library by itself without any other dependencies.

It supports basic HTTP and username/password based authentication. No TLS support.
Only GET and POST HTTP methods are supported.

Steps to build and install the library `libeasy_curl.so` along with the header `easy_curl.h`.
```bash
git clone https://github.com/bbhavsar/easy_curl.git
cd easy_curl
mkdir build/
cd build
cmake ..
make
sudo make install
```

Compilation:
```bash
g++ my_app.cc -o my_app.out -leasy_curl
```

To run the application:
```bash
./my_app.out
```

Sample GET application:
```c++
#include <easy_curl.h>
#include <iostream>

int main() {
  EasyCurl ec;
  string resp;
  auto e = ec.FetchURL("http://localhost:40080/sample_get", &resp);
  if (e.code != kOk) {
    cout << "Error fetching data: " << e.msg << endl;
  }               
  cout << resp << endl;
  return 0;                                                       
}
```

**NOTE**: If you don't have permissions to copy the library and header to default library
and include paths, then you can use the LD_LIBRARY_PATH environment variable while linking
and running the application. See [this post](https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_C_libraries.html) for details.

