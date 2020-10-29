#include <cctype>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>


using namespace std;


int main() {
    ifstream input("input.txt");
    set<string> tokens;
    string str;
    while (getline(input, str)) {
        for (size_t s = 0, e = 0; e <= str.size(); ++e) {
            if (isalnum(str[e]) == 0) {
                if (s < e) tokens.insert(str.substr(s, e - s));
                s = e + 1;
            }
        }
    }
    for (auto &tok: tokens) cout << tok << "\n";
}


// Local Variables:
// compile-command: "g++ -x c++ -std=gnu++17 -O2 task.cxx && time ./a.out"
// End:
