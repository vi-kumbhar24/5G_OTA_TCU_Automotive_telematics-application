#include <string>
#include <stdio.h>
using namespace std;

class Tail {
  private:
    string _line;
    string _filename;
    bool _opened;
    FILE *_fp;
    int _linenum;
    char _last_eol;

  public:
    Tail() {
        _opened = false;
    }

    bool open(string filename) {
        _fp = fopen(filename.c_str(), "r");
        if (_fp != NULL) {
           _filename = filename;
           _line = "";
           _linenum = 0;
           _last_eol = ' ';
           _opened = true;
        }
        return _opened;
    }

    bool read(string &line) {
        bool retval = false;
        if (_opened) {
            while (true) {
                int ci;
                ci = fgetc(_fp);
                if (ci == EOF) {
                    clearerr(_fp);
                    break;
                }
                else {
                    char c = (char)ci;
                    if ((c == '\r') or (c == '\n')) {
                        if (_last_eol == ' ') {
                           _last_eol = c;
                            line = _line;
                           _line = "";
                           _linenum++;
                           retval = true;
                           break;
                        }
                        else if (c == _last_eol) {
                           line = _line;
                           _linenum++;
                           retval = true;
                           break;
                        }
                    }
                    else {
                        _line += c;
                        _last_eol = ' ';
                    }
                }
            }
        }
        return retval;
    }

    int linenum() {
        return _linenum;
    }

    void close() {
        if (_opened) {
            fclose(_fp);
            _opened = false;
        }
    }
};
