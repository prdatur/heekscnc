################################################################################
# iso_read.py
#
# Simple ISO NC code parsing
#
# Hirutso Enni, 2009-01-13

import nc_read as nc
import re
import sys

################################################################################
class ParserIso(nc.Parser):

    def __init__(self):
        nc.Parser.__init__(self)

        self.pattern_main = re.compile('(\s+|\w\d*(?:\.\d*)?|\w\#\d+|\(.*?\)|\#\d+\=\d*(?:\.\d*)?)')

        self.a = 0
        self.b = 0
        self.c = 0
        self.f = 0
        self.i = 0
        self.j = 0
        self.k = 0
        self.p = 0
        self.q = 0
        self.r = 0
        self.s = 0
        self.x = 0
        self.y = 0
        self.z = 500

        self.mul = 1.0

    def Parse(self, name):
        self.files_open(name)

        while (self.readline()):
            self.begin_ncblock()

            move = False;

            words = self.pattern_main.findall(self.line)
            for word in words:
                col = "plain"
                if (word[0] == 'A' or word[0] == 'a'):
                    col = "axis"
                    self.a = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'B' or word[0] == 'b'):
                    col = "axis"
                    self.b = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'C' or word[0] == 'c'):
                    col = "axis"
                    self.c = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'F' or word[0] == 'f'):
                    col = "axis"
                    self.f = self.mul * eval(word[1:])
                    move = True
                elif (word == 'G0' or word == 'G00' or word == 'g0' or word == 'g00') : col = "rapid"
                elif (word == 'G1' or word == 'G01' or word == 'g1' or word == 'g01') : col = "feed"
                elif (word == 'G2' or word == 'G02' or word == 'g2' or word == 'g02') : col = "feed"
                elif (word == 'G3' or word == 'G03' or word == 'g3' or word == 'g03') : col = "feed"
                elif (word == 'G20'):
                    col = "prep"
                    self.mul = 25.4
                elif (word == 'G21'):
                    col = "prep"
                    self.mul = 1.0
                elif (word[0] == 'G') : col = "prep"
                elif (word[0] == 'I' or word[0] == 'i'):
                    col = "axis"
                    self.i = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'J' or word[0] == 'j'):
                    col = "axis"
                    self.j = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'K' or word[0] == 'k'):
                    col = "axis"
                    self.k = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'M') : col = "misc"
                elif (word[0] == 'N') : col = "blocknum"
                elif (word[0] == 'O') : col = "program"
                elif (word[0] == 'P' or word[0] == 'p'):
                    col = "axis"
                    self.p = eval(word[1:])
                    move = True
                elif (word[0] == 'Q' or word[0] == 'q'):
                    col = "axis"
                    self.q = eval(word[1:])
                    move = True
                elif (word[0] == 'R' or word[0] == 'r'):
                    col = "axis"
                    self.r = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'S' or word[0] == 's'):
                    col = "axis"
                    self.s = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'T') : col = "tool"
                elif (word[0] == 'X' or word[0] == 'x'):
                    col = "axis"
                    self.x = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'Y' or word[0] == 'y'):
                    col = "axis"
                    self.y = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == 'Z' or word[0] == 'z'):
                    col = "axis"
                    self.z = self.mul * eval(word[1:])
                    move = True
                elif (word[0] == '(') : col = "comment"
                elif (word[0] == '#') : col = "variable"
                self.add_text(word, col)

            if (move):
                self.begin_lines()
                self.add_point(self.x, self.y, self.z)
                self.end_lines()

            self.end_ncblock()

        self.files_close()

################################################################################

if __name__ == '__main__':
    parser = ParserIso()
    parser.Parse(sys.argv[1])
