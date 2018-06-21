
#include <iostream>
#include <string>
#include <fstream>

using namespace std;
#define FILE_NAME_INDEX 1

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage - " << argv[0] << " file-name" << endl;
    }

    string line;
    ifstream out(argv[FILE_NAME_INDEX]);
    while (getline(out, line))
    {
        cout << line << endl;
    }
    out.close();

    return 0;
}
