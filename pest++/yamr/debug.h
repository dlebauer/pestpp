#ifndef DEBUG_H_
#define DEBUG_H_

#include <ostream>
#include <fstream>
#include <set>
#include <string>
#include <vector>


#define DEBUG

#ifdef DEBUG
extern std::ofstream fout_dbg;
std::ostream& operator<< (std::ostream &os, const std::set<std::string> val);
std::ostream& operator<< (std::ostream &os, const std::vector<std::string> val);
#define debug_initialize(filename) fout_dbg.open(filename)
#define debug_print(x) fout_dbg << #x << ": " << x << endl
#define debug_msg(message) fout_dbg << message << endl
#else
#define debug_print(x)
#define debug_msg(message)
#define debug_initialize(filename)
#endif


#endif /* DEBUG_H_ */